/* arch/x86_64/drivers/mouse.c
 * PS/2 mouse via the i8042 controller's auxiliary port. Standard
 * 3-byte packet protocol — enough for a cursor and click, no wheel.
 */

#include <arch/x86_64/mouse.h>
#include <arch/x86_64/framebuffer.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>

#define I8042_DATA    0x60
#define I8042_STATUS  0x64
#define I8042_CMD     0x64

#define I8042_OUT_FULL 0x01
#define I8042_IN_FULL  0x02

static i32 mouse_x = 0;
static i32 mouse_y = 0;
static u8  mouse_buttons = 0;
static volatile bool state_dirty = false;

static u8  packet[3];
static int packet_index = 0;

static void wait_input_clear(void)
{
    for (int timeout = 100000; timeout > 0; timeout--)
        if (!(inb(I8042_STATUS) & I8042_IN_FULL))
            return;
}

static void wait_output_full(void)
{
    for (int timeout = 100000; timeout > 0; timeout--)
        if (inb(I8042_STATUS) & I8042_OUT_FULL)
            return;
}

static void mouse_write(u8 val)
{
    wait_input_clear();
    outb(I8042_CMD, 0xD4);   /* next data byte goes to the mouse */
    wait_input_clear();
    outb(I8042_DATA, val);
}

static u8 mouse_read(void)
{
    wait_output_full();
    return inb(I8042_DATA);
}

static void mouse_irq(registers_t* regs)
{
    UNUSED(regs);
    u8 data = inb(I8042_DATA);

    if (packet_index == 0 && !(data & 0x08)) {
        return;   /* not a sync byte — drop until we see one */
    }

    packet[packet_index++] = data;
    if (packet_index < 3)
        return;
    packet_index = 0;

    i32 dx = packet[1];
    i32 dy = packet[2];
    if (packet[0] & 0x10) dx -= 256;
    if (packet[0] & 0x20) dy -= 256;

    mouse_x += dx;
    mouse_y -= dy;   /* PS/2 Y+ is "up"; framebuffer Y+ is down */

    i32 max_x = (i32)fb_width()  - 1;
    i32 max_y = (i32)fb_height() - 1;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (max_x >= 0 && mouse_x > max_x) mouse_x = max_x;
    if (max_y >= 0 && mouse_y > max_y) mouse_y = max_y;

    mouse_buttons = packet[0] & 0x07;
    state_dirty = true;
}

void mouse_init(void)
{
    mouse_x = (i32)(fb_width()  / 2);
    mouse_y = (i32)(fb_height() / 2);

    wait_input_clear();
    outb(I8042_CMD, 0xA8);         /* enable auxiliary device */

    wait_input_clear();
    outb(I8042_CMD, 0x20);         /* read controller command byte */
    u8 status = mouse_read();
    status |= 0x02;                /* enable IRQ12 */
    status &= (u8)~0x20;           /* enable aux port clock */
    wait_input_clear();
    outb(I8042_CMD, 0x60);         /* write controller command byte */
    wait_input_clear();
    outb(I8042_DATA, status);

    mouse_write(0xF6);             /* set defaults */
    mouse_read();                  /* ACK */
    mouse_write(0xF4);             /* enable data reporting */
    mouse_read();                  /* ACK */

    irq_install_handler(12, mouse_irq);
}

bool mouse_get_state(i32* x, i32* y, u8* buttons)
{
    if (!state_dirty)
        return false;

    *x = mouse_x;
    *y = mouse_y;
    *buttons = mouse_buttons;
    state_dirty = false;
    return true;
}
