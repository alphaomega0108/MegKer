/* arch/x86_64/drivers/keyboard.c
 * PS/2 keyboard, US QWERTY, scan code set 1. Extended (0xE0-prefixed)
 * keys and non-shift modifiers are ignored for now — just enough to
 * type at a terminal.
 */

#include <arch/x86_64/keyboard.h>
#include <arch/x86_64/idt.h>
#include <arch/x86_64/io.h>

#define KB_DATA_PORT 0x60

#define SC_LSHIFT       0x2A
#define SC_RSHIFT       0x36
#define SC_LSHIFT_UP    (SC_LSHIFT | 0x80)
#define SC_RSHIFT_UP    (SC_RSHIFT | 0x80)

static const char scancode_ascii[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6',    /* 0x00-0x07 */
    '7',  '8', '9', '0', '-', '=', '\b','\t',   /* 0x08-0x0F */
    'q',  'w', 'e', 'r', 't', 'y', 'u', 'i',    /* 0x10-0x17 */
    'o',  'p', '[', ']', '\n', 0,  'a', 's',    /* 0x18-0x1F */
    'd',  'f', 'g', 'h', 'j', 'k', 'l', ';',    /* 0x20-0x27 */
    '\'', '`', 0,   '\\','z', 'x', 'c', 'v',    /* 0x28-0x2F */
    'b',  'n', 'm', ',', '.', '/', 0,   '*',    /* 0x30-0x37 */
    0,    ' ', 0,   0,   0,   0,   0,   0,      /* 0x38-0x3F */
    /* rest (function keys, keypad, etc.) unmapped */
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

#define KB_BUF_SIZE 256
static char buf[KB_BUF_SIZE];
static volatile u32 buf_head = 0;
static volatile u32 buf_tail = 0;
static bool shift_held = false;

static void buf_push(char c)
{
    u32 next = (buf_head + 1) % KB_BUF_SIZE;
    if (next == buf_tail)
        return;   /* full — drop the keystroke */
    buf[buf_head] = c;
    buf_head = next;
}

char keyboard_getchar(void)
{
    if (buf_tail == buf_head)
        return 0;
    char c = buf[buf_tail];
    buf_tail = (buf_tail + 1) % KB_BUF_SIZE;
    return c;
}

static void keyboard_irq(registers_t* regs)
{
    UNUSED(regs);
    u8 sc = inb(KB_DATA_PORT);

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift_held = true;
        return;
    }
    if (sc == SC_LSHIFT_UP || sc == SC_RSHIFT_UP) {
        shift_held = false;
        return;
    }
    if (sc & 0x80)
        return;   /* key release, nothing else to do with it */
    if (sc >= 128)
        return;   /* extended/unsupported */

    char c = shift_held ? scancode_ascii_shift[sc] : scancode_ascii[sc];
    if (c)
        buf_push(c);
}

void keyboard_init(void)
{
    irq_install_handler(1, keyboard_irq);
}
