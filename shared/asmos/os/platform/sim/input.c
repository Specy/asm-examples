/*
 * The keyboard and the mouse are the editor's Input controller, a device at 0xFFFF0300 that MIPS
 * and RISC-V programs read as memory. What a driver for it does, this File does:
 *
 *   - It checks the ID register, "INP" and the version, to know the device is there: on MARS and
 *     RARS themselves the block is plain memory reading 0, and asmOS then has no input.
 *   - It drains the event queue, one 32-bit word an event read with one lw: the kind (key down,
 *     up or repeat, a button down or up, the wheel, or "events were lost"), a code, and where the
 *     pointer was. Keys come as USB HID usages, the code a USB keyboard sends, and a table turns
 *     them into asmOS's key codes (keys.h), as Linux's HID driver turns them into its own.
 *   - It reads the pointer register, where the pointer is now, and reports a move when that
 *     changed. Moves are state, not events: the queue keeps only presses and releases.
 *   - When the queue overflowed and events were lost, it compares the key and button state
 *     registers with what it has reported and makes up the difference, so no key stays down.
 *   - It waits by sleeping between polls (syscall 32): the device has no interrupt asmOS uses.
 */
#include <sim.h>

/* asmOS's key code (keys.h) for each HID usage of the Keyboard/Keypad page, 0 for none: Linux's
 * hid_keyboard table, which numbers keys as asmOS does. */
static const unsigned char HID_KEYS[256] = {
    [0x04] = KEY_A, [0x05] = KEY_B, [0x06] = KEY_C, [0x07] = KEY_D, [0x08] = KEY_E, [0x09] = KEY_F,
    [0x0a] = KEY_G, [0x0b] = KEY_H, [0x0c] = KEY_I, [0x0d] = KEY_J, [0x0e] = KEY_K, [0x0f] = KEY_L,
    [0x10] = KEY_M, [0x11] = KEY_N, [0x12] = KEY_O, [0x13] = KEY_P, [0x14] = KEY_Q, [0x15] = KEY_R,
    [0x16] = KEY_S, [0x17] = KEY_T, [0x18] = KEY_U, [0x19] = KEY_V, [0x1a] = KEY_W, [0x1b] = KEY_X,
    [0x1c] = KEY_Y, [0x1d] = KEY_Z,
    [0x1e] = KEY_1, [0x1f] = KEY_2, [0x20] = KEY_3, [0x21] = KEY_4, [0x22] = KEY_5,
    [0x23] = KEY_6, [0x24] = KEY_7, [0x25] = KEY_8, [0x26] = KEY_9, [0x27] = KEY_0,
    [0x28] = KEY_ENTER, [0x29] = KEY_ESC, [0x2a] = KEY_BACKSPACE, [0x2b] = KEY_TAB, [0x2c] = KEY_SPACE,
    [0x2d] = KEY_MINUS, [0x2e] = KEY_EQUAL, [0x2f] = KEY_LEFTBRACE, [0x30] = KEY_RIGHTBRACE,
    [0x31] = KEY_BACKSLASH, [0x32] = KEY_BACKSLASH, [0x33] = KEY_SEMICOLON, [0x34] = KEY_APOSTROPHE,
    [0x35] = KEY_GRAVE, [0x36] = KEY_COMMA, [0x37] = KEY_DOT, [0x38] = KEY_SLASH, [0x39] = KEY_CAPSLOCK,
    [0x3a] = KEY_F1, [0x3b] = KEY_F2, [0x3c] = KEY_F3, [0x3d] = KEY_F4, [0x3e] = KEY_F5, [0x3f] = KEY_F6,
    [0x40] = KEY_F7, [0x41] = KEY_F8, [0x42] = KEY_F9, [0x43] = KEY_F10, [0x44] = KEY_F11, [0x45] = KEY_F12,
    [0x46] = KEY_SYSRQ, [0x47] = KEY_SCROLLLOCK, [0x48] = KEY_PAUSE, [0x49] = KEY_INSERT,
    [0x4a] = KEY_HOME, [0x4b] = KEY_PAGEUP, [0x4c] = KEY_DELETE, [0x4d] = KEY_END, [0x4e] = KEY_PAGEDOWN,
    [0x4f] = KEY_RIGHT, [0x50] = KEY_LEFT, [0x51] = KEY_DOWN, [0x52] = KEY_UP,
    [0x53] = KEY_NUMLOCK, [0x54] = KEY_KPSLASH, [0x55] = KEY_KPASTERISK, [0x56] = KEY_KPMINUS,
    [0x57] = KEY_KPPLUS, [0x58] = KEY_KPENTER, [0x59] = KEY_KP1, [0x5a] = KEY_KP2, [0x5b] = KEY_KP3,
    [0x5c] = KEY_KP4, [0x5d] = KEY_KP5, [0x5e] = KEY_KP6, [0x5f] = KEY_KP7, [0x60] = KEY_KP8,
    [0x61] = KEY_KP9, [0x62] = KEY_KP0, [0x63] = KEY_KPDOT, [0x64] = KEY_102ND, [0x65] = KEY_COMPOSE,
    [0x67] = KEY_KPEQUAL, [0x85] = KEY_KPCOMMA,
    [0xe0] = KEY_LEFTCTRL, [0xe1] = KEY_LEFTSHIFT, [0xe2] = KEY_LEFTALT, [0xe3] = KEY_LEFTMETA,
    [0xe4] = KEY_RIGHTCTRL, [0xe5] = KEY_RIGHTSHIFT, [0xe6] = KEY_RIGHTALT, [0xe7] = KEY_RIGHTMETA,
};

/* The device's buttons, HID's numbering in events and bits in the pointer register, as asmOS's. */
static int button_of(unsigned code) {
    return code == SIM_INPUT_BUTTON_LEFT ? BUTTON_LEFT
           : code == SIM_INPUT_BUTTON_RIGHT ? BUTTON_RIGHT
           : code == SIM_INPUT_BUTTON_MIDDLE ? BUTTON_MIDDLE : 0;
}
static int buttons_of(unsigned held) {
    return (held & SIM_INPUT_HELD_LEFT ? BUTTON_LEFT : 0) | (held & SIM_INPUT_HELD_RIGHT ? BUTTON_RIGHT : 0) |
           (held & SIM_INPUT_HELD_MIDDLE ? BUTTON_MIDDLE : 0);
}

static int input_present;
/* What this driver has told the kernel: the keys down, by HID usage, the buttons held and where
 * the pointer is. A dropped record is made good against these. */
static unsigned char reported_keys[32];
static int reported_buttons;
static int reported_x = -1, reported_y = -1;

int platform_input_open(void) {
    input_present = sim_input_present();
    if (!input_present) platform_print("No Input controller at 0xFFFF0300: asmOS has no keyboard or mouse here\n");
    return input_present;
}

/* Adds an event, when there is room: answers how many there are now. */
static int add_event(InputEvent *events, int count, int max, int kind, int code, int x, int y) {
    if (count >= max) return count;
    InputEvent *event = &events[count];
    event->kind = kind;
    event->code = code;
    event->x = x;
    event->y = y;
    return count + 1;
}

/* A move to where an event happened, before the event, so that a press and its release far apart
 * between two polls still drag what they pressed on. */
static int move_to(InputEvent *events, int count, int max, int x, int y) {
    if (x == reported_x && y == reported_y) return count;
    reported_x = x;
    reported_y = y;
    return add_event(events, count, max, INPUT_MOVE, 0, x, y);
}

static int key_event(InputEvent *events, int count, int max, unsigned usage, int kind, int x, int y) {
    unsigned char bit = (unsigned char)(1u << (usage & 7));
    if (kind == INPUT_KEY_UP) reported_keys[usage >> 3] &= (unsigned char)~bit;
    else reported_keys[usage >> 3] |= bit;
    return HID_KEYS[usage] ? add_event(events, count, max, kind, HID_KEYS[usage], x, y) : count;
}

/* Events were lost: the keys and buttons whose state differs from what was reported go down or up
 * now, from the state registers, as Linux's input core does after SYN_DROPPED. */
static int resynchronize(InputEvent *events, int count, int max) {
    unsigned pointer = sim_input_pointer();
    int x = (int)SIM_INPUT_X(pointer), y = (int)SIM_INPUT_Y(pointer);
    for (unsigned usage = 0; usage < 256; usage++) {
        int held = sim_input_key_down(usage), was = reported_keys[usage >> 3] >> (usage & 7) & 1;
        if (held != was) count = key_event(events, count, max, usage, held ? INPUT_KEY_DOWN : INPUT_KEY_UP, x, y);
    }
    int buttons = buttons_of(SIM_INPUT_BUTTONS(pointer));
    for (int button = BUTTON_LEFT; button <= BUTTON_MIDDLE; button <<= 1)
        if ((buttons ^ reported_buttons) & button) {
            count = move_to(events, count, max, x, y);
            count = add_event(events, count, max, buttons & button ? INPUT_BUTTON_DOWN : INPUT_BUTTON_UP, button, x, y);
        }
    reported_buttons = buttons;
    return count;
}

/* What waits in the queue, then a move if the pointer has moved since. */
static int input_poll(InputEvent *events, int max) {
    int count = 0;
    while (count + 2 <= max && sim_input_pending()) {
        unsigned event = sim_input_event();
        unsigned kind = SIM_INPUT_KIND(event), code = SIM_INPUT_CODE(event);
        int x = (int)SIM_INPUT_X(event), y = (int)SIM_INPUT_Y(event);
        if (kind == SIM_INPUT_KEY_DOWN || kind == SIM_INPUT_KEY_REPEAT || kind == SIM_INPUT_KEY_UP) {
            int to = kind == SIM_INPUT_KEY_DOWN ? INPUT_KEY_DOWN : kind == SIM_INPUT_KEY_UP ? INPUT_KEY_UP : INPUT_KEY_REPEAT;
            count = key_event(events, count, max, code, to, x, y);
        } else if (kind == SIM_INPUT_BUTTON_DOWN || kind == SIM_INPUT_BUTTON_UP) {
            int button = button_of(code);
            if (!button) continue;
            count = move_to(events, count, max, x, y);
            if (kind == SIM_INPUT_BUTTON_DOWN) reported_buttons |= button;
            else reported_buttons &= ~button;
            count = add_event(events, count, max, kind == SIM_INPUT_BUTTON_DOWN ? INPUT_BUTTON_DOWN : INPUT_BUTTON_UP,
                              button, x, y);
        } else if (kind == SIM_INPUT_WHEEL) {
            count = move_to(events, count, max, x, y);
            count = add_event(events, count, max, INPUT_WHEEL, SIM_INPUT_WHEEL_NOTCHES(event), x, y);
        } else if (kind == SIM_INPUT_DROPPED) {
            count = resynchronize(events, count, max);
        }
    }
    unsigned pointer = sim_input_pointer();
    return move_to(events, count, max, (int)SIM_INPUT_X(pointer), (int)SIM_INPUT_Y(pointer));
}

/* How long a wait sleeps between two polls: short enough that a click is answered at once. */
enum { INPUT_POLL_MS = 10 };

int platform_input_read(InputEvent *events, int max, int timeout_ms) {
    if (!input_present) {
        if (timeout_ms != 0) platform_sleep_ms(timeout_ms < 0 ? INPUT_POLL_MS : timeout_ms);
        return 0;
    }
    long start = platform_now_ms();
    for (;;) {
        int count = input_poll(events, max);
        if (count || timeout_ms == 0) return count;
        long waited = platform_now_ms() - start;
        if (timeout_ms > 0 && waited >= timeout_ms) return 0;
        int pause = timeout_ms < 0 ? INPUT_POLL_MS : (int)min_int(INPUT_POLL_MS, timeout_ms - (int)waited);
        platform_sleep_ms(pause);
    }
}
