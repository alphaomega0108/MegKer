/* arch/aarch64/drivers/virtio_input.c
 * virtio-input keyboard + mouse. Unlike virtio-blk's synchronous
 * request/response, the input eventq is device-driven: pre-post a
 * pool of device-writable buffers, then poll for whichever ones the
 * device has filled with a {type, code, value} event, recycling each
 * buffer right back onto the queue immediately.
 *
 * QEMU attaches keyboard and mouse as two separate virtio-input
 * devices, both with DeviceID=18 — indistinguishable by slot alone,
 * so each candidate's config-space name (VIRTIO_INPUT_CFG_ID_NAME)
 * is queried to tell them apart, the same "probe properly, don't
 * assume slot order" principle already applied to virtio-blk.
 *
 * Keycode table: virtio-input (like real evdev) numbers the main
 * alphanumeric keys identically to the original AT scancode set 1 —
 * KEY_A really is 30, same as x86_64's PS/2 scancode for 'a' — so
 * the ASCII lookup tables are a direct copy of keyboard.c's, not a
 * coincidence.
 */

#include <arch/aarch64/virtio_input.h>
#include <arch/aarch64/virtio.h>
#include <arch/aarch64/framebuffer.h>
#include <mm/heap.h>

#define VIRTIO_INPUT_CFG_ID_NAME 0x01

#define EV_SYN 0x00
#define EV_KEY 0x01
#define EV_REL 0x02

#define REL_X 0x00
#define REL_Y 0x01

#define BTN_LEFT   0x110
#define BTN_RIGHT  0x111
#define BTN_MIDDLE 0x112

typedef struct {
    u16 type;
    u16 code;
    u32 value;
} __attribute__((packed)) virtio_input_event_t;

static const char scancode_ascii[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6',    /* 0x00-0x07 */
    '7',  '8', '9', '0', '-', '=', '\b','\t',   /* 0x08-0x0F */
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',    /* 0x10-0x17 */
    'o',  'p', '[', ']', '\n', 0,  'a', 's',    /* 0x18-0x1F */
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';',    /* 0x20-0x27 */
    '\'', '`', 0,   '\\','z', 'x', 'c', 'v',    /* 0x28-0x2F */
    'b',  'n', 'm', ',', '.', '/', 0,   '*',    /* 0x30-0x37 */
    0,    ' ', 0,   0,   0,   0,   0,   0,      /* 0x38-0x3F */
};

static const char scancode_ascii_shift[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^',    /* 0x00-0x07 */
    '&',  '*', '(', ')', '_', '+', '\b','\t',   /* 0x08-0x0F */
    'Q',  'W', 'E', 'R', 'T', 'Y', 'U', 'I',    /* 0x10-0x17 */
    'O',  'P', '{', '}', '\n', 0,  'A', 'S',    /* 0x18-0x1F */
    'D',  'F', 'G', 'H', 'J', 'K', 'L', ':',    /* 0x20-0x27 */
    '"',  '~', 0,   '|', 'Z', 'X', 'C', 'V',    /* 0x28-0x2F */
    'B',  'N', 'M', '<', '>', '?', 0,   '*',    /* 0x30-0x37 */
    0,    ' ', 0,   0,   0,   0,   0,   0,      /* 0x38-0x3F */
};

#define KEY_LEFTSHIFT  42
#define KEY_RIGHTSHIFT 54

#define KB_BUF_SIZE 256
static char kb_buf[KB_BUF_SIZE];
static volatile u32 kb_head = 0;
static volatile u32 kb_tail = 0;
static bool shift_held = false;

static virtio_device_t kbd_dev;
static virtio_device_t mouse_dev;
static bool kbd_present = false;
static bool mouse_present = false;
static virtio_input_event_t* kbd_bufs;
static virtio_input_event_t* mouse_bufs;

static i32 mouse_x = 0;
static i32 mouse_y = 0;
static u8  mouse_buttons = 0;
static volatile bool mouse_dirty = false;

static bool str_contains(const char* haystack, const char* needle)
{
    for (; *haystack; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n)
            return true;
    }
    return false;
}

static void seed_queue(virtio_device_t* dev, virtio_input_event_t* bufs)
{
    for (u16 i = 0; i < dev->queue_size; i++) {
        dev->desc[i].addr  = (u64)(usize)&bufs[i];
        dev->desc[i].len   = sizeof(virtio_input_event_t);
        dev->desc[i].flags = VIRTQ_DESC_F_WRITE;
        dev->desc[i].next  = 0;
        virtio_publish_avail(dev, i);
    }
}

static bool poll_event(virtio_device_t* dev, virtio_input_event_t* bufs, virtio_input_event_t* out)
{
    u16 idx;
    u32 len;
    if (!virtio_poll_used(dev, &idx, &len))
        return false;
    *out = bufs[idx];
    virtio_publish_avail(dev, idx);   /* recycle immediately */
    return true;
}

static void kb_push(char c)
{
    u32 next = (kb_head + 1) % KB_BUF_SIZE;
    if (next == kb_tail)
        return;   /* full — drop the keystroke */
    kb_buf[kb_head] = c;
    kb_head = next;
}

static void handle_kbd_event(virtio_input_event_t e)
{
    if (e.type != EV_KEY)
        return;

    if (e.code == KEY_LEFTSHIFT || e.code == KEY_RIGHTSHIFT) {
        shift_held = (e.value != 0);
        return;
    }
    if (e.value == 0)
        return;   /* release — nothing else to do with it */
    if (e.code >= 128)
        return;   /* unsupported */

    char c = shift_held ? scancode_ascii_shift[e.code] : scancode_ascii[e.code];
    if (c)
        kb_push(c);
}

static void handle_mouse_event(virtio_input_event_t e)
{
    if (e.type == EV_REL) {
        i32 delta = (i32)e.value;
        if (e.code == REL_X) mouse_x += delta;
        else if (e.code == REL_Y) mouse_y += delta;   /* evdev Y+ is down, same as the framebuffer */
        else return;

        i32 max_x = (i32)fb_width()  - 1;
        i32 max_y = (i32)fb_height() - 1;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (max_x >= 0 && mouse_x > max_x) mouse_x = max_x;
        if (max_y >= 0 && mouse_y > max_y) mouse_y = max_y;
        mouse_dirty = true;
    } else if (e.type == EV_KEY) {
        u8 bit = 0;
        if (e.code == BTN_LEFT) bit = 0x01;
        else if (e.code == BTN_RIGHT) bit = 0x02;
        else if (e.code == BTN_MIDDLE) bit = 0x04;
        else return;

        if (e.value) mouse_buttons |= bit;
        else         mouse_buttons &= (u8)~bit;
        mouse_dirty = true;
    }
    /* EV_SYN (event-group boundary) needs no handling here — every
     * event already gets applied to state as it arrives. */
}

/* One scan, claiming each virtio-input slot exactly once — probing a
 * slot resets its virtqueue, so re-probing an already-claimed device
 * while looking for the other one would silently detach it from the
 * frames seed_queue() just populated. */
void virtio_input_init(void)
{
    mouse_x = (i32)(fb_width()  / 2);
    mouse_y = (i32)(fb_height() / 2);

    for (u32 i = 0; !(kbd_present && mouse_present); i++) {
        virtio_device_t candidate;
        if (!virtio_mmio_probe_nth(VIRTIO_DEV_INPUT, i, &candidate))
            break;   /* no more virtio-input devices */

        virtio_config_write8(&candidate, 0, VIRTIO_INPUT_CFG_ID_NAME);   /* select */
        virtio_config_write8(&candidate, 1, 0);                          /* subsel */
        u8 size = virtio_config_read8(&candidate, 2);

        char name[129];
        u8 n = size < 128 ? size : 128;
        for (u8 b = 0; b < n; b++)
            name[b] = (char)virtio_config_read8(&candidate, 8 + b);
        name[n] = '\0';

        if (!kbd_present && str_contains(name, "Keyboard")) {
            kbd_dev = candidate;
            kbd_present = true;
            kbd_bufs = (virtio_input_event_t*)kmalloc(sizeof(virtio_input_event_t) * kbd_dev.queue_size);
            seed_queue(&kbd_dev, kbd_bufs);
        } else if (!mouse_present && str_contains(name, "Mouse")) {
            mouse_dev = candidate;
            mouse_present = true;
            mouse_bufs = (virtio_input_event_t*)kmalloc(sizeof(virtio_input_event_t) * mouse_dev.queue_size);
            seed_queue(&mouse_dev, mouse_bufs);
        }
    }
}

char virtio_keyboard_getchar(void)
{
    if (kbd_present) {
        virtio_input_event_t e;
        while (poll_event(&kbd_dev, kbd_bufs, &e))
            handle_kbd_event(e);
    }

    if (kb_tail == kb_head)
        return 0;
    char c = kb_buf[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUF_SIZE;
    return c;
}

bool virtio_mouse_get_state(i32* x, i32* y, u8* buttons)
{
    if (mouse_present) {
        virtio_input_event_t e;
        while (poll_event(&mouse_dev, mouse_bufs, &e))
            handle_mouse_event(e);
    }

    if (!mouse_dirty)
        return false;

    *x = mouse_x;
    *y = mouse_y;
    *buttons = mouse_buttons;
    mouse_dirty = false;
    return true;
}
