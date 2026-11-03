/*
 * asmOS's key codes: which key of the keyboard a key event is about, whatever it types. They are
 * the numbers Linux gives the keys of a PC keyboard (<linux/input-event-codes.h>), so on x86, where
 * asmOS reads Linux's input devices, a key arrives with its code already. A machine that numbers
 * its keys otherwise, as USB's HID usages do, has its platform file translate them (os/platform/).
 *
 * The codes go with Message.key and os_key_down. The character a key types is Message.ch, which
 * the kernel works out from the code, the Shift key and Caps Lock (os/input.c).
 */
#ifndef ASMOS_KEYS_H
#define ASMOS_KEYS_H

enum {
    KEY_RESERVED = 0,
    KEY_ESC = 1,
    KEY_1 = 2, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
    KEY_MINUS = 12, KEY_EQUAL, KEY_BACKSPACE, KEY_TAB,
    KEY_Q = 16, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P,
    KEY_LEFTBRACE = 26, KEY_RIGHTBRACE, KEY_ENTER, KEY_LEFTCTRL,
    KEY_A = 30, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L,
    KEY_SEMICOLON = 39, KEY_APOSTROPHE, KEY_GRAVE, KEY_LEFTSHIFT, KEY_BACKSLASH,
    KEY_Z = 44, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M,
    KEY_COMMA = 51, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_KPASTERISK, KEY_LEFTALT, KEY_SPACE,
    KEY_CAPSLOCK = 58,
    KEY_F1 = 59, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUMLOCK = 69, KEY_SCROLLLOCK,
    KEY_KP7 = 71, KEY_KP8, KEY_KP9, KEY_KPMINUS, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KPPLUS,
    KEY_KP1 = 79, KEY_KP2, KEY_KP3, KEY_KP0, KEY_KPDOT,
    KEY_102ND = 86,                     /* the key between left Shift and Z on ISO keyboards */
    KEY_F11 = 87, KEY_F12,
    KEY_KPENTER = 96, KEY_RIGHTCTRL, KEY_KPSLASH, KEY_SYSRQ, KEY_RIGHTALT,
    KEY_HOME = 102, KEY_UP, KEY_PAGEUP, KEY_LEFT, KEY_RIGHT, KEY_END, KEY_DOWN, KEY_PAGEDOWN,
    KEY_INSERT = 110, KEY_DELETE,
    KEY_KPEQUAL = 117,
    KEY_PAUSE = 119,
    KEY_KPCOMMA = 121,
    KEY_LEFTMETA = 125, KEY_RIGHTMETA, KEY_COMPOSE,   /* the Windows keys, and the Menu key */
    KEY_COUNT = 256                     /* every code is below this */
};

#endif
