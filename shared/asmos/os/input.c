/*
 * The keyboard and the pointer, as the platform reads them (platform_input_read): key codes become
 * characters with a US layout, the keys held are kept for os_key_down, two quick presses make a
 * double click, and events go to the window manager as MouseEvent and KeyEvent.
 */

/* What the pointer and the keyboard did, as the window manager takes them. */
enum { MOUSE_DOWN, MOUSE_UP, MOUSE_MOVE, MOUSE_WHEEL };
typedef struct {
    int kind;       /* MOUSE_DOWN, MOUSE_UP, MOUSE_MOVE or MOUSE_WHEEL */
    int x, y;       /* in screen pixels */
    int button;     /* the button that went down or up */
    int buttons;    /* the buttons held */
    int clicks;     /* 2 for the second press of a double click */
    int wheel;
} MouseEvent;
typedef struct {
    int code;       /* asmOS's KEY_ code, keys.h */
    int ch;         /* the character it types, or 0 */
    int mods;
} KeyEvent;

void wm_mouse_event(const MouseEvent *event);
void wm_key_event(const KeyEvent *event, int released);

static int modifiers, caps_lock;
static unsigned char keys_held[KEY_COUNT / 8];

int os_key_down(int key) { return key >= 0 && key < KEY_COUNT && keys_held[key / 8] >> (key % 8) & 1; }
static int key_modifiers(void) { return modifiers; }

/* What each key types, by its code: unshifted, then shifted. */
static const char KEYMAP[] =
    "\0\0331234567890-=\b\tqwertyuiop[]\n\0asdfghjkl;'`\0\\zxcvbnm,./\0*\0 ";
static const char KEYMAP_SHIFT[] =
    "\0\033!@#$%^&*()_+\b\tQWERTYUIOP{}\n\0ASDFGHJKL:\"~\0|ZXCVBNM<>?\0*\0 ";

/* ------------------------------------------------------------------ the keyboard */

static int key_character(int code) {
    int ch = 0;
    if (code < (int)sizeof KEYMAP - 1)
        ch = (modifiers & MOD_SHIFT ? KEYMAP_SHIFT : KEYMAP)[code];
    else if (code >= KEY_KP7 && code <= KEY_KPDOT)
        ch = "789-456+1230."[code - KEY_KP7];
    else if (code == KEY_KPENTER)
        ch = '\n';
    else if (code == KEY_KPSLASH)
        ch = '/';
    if (caps_lock && ch >= 'a' && ch <= 'z') ch -= 32;
    else if (caps_lock && ch >= 'A' && ch <= 'Z') ch += 32;
    return ch;
}

static int modifier_of(int code) {
    switch (code) {
    case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT: return MOD_SHIFT;
    case KEY_LEFTCTRL: case KEY_RIGHTCTRL: return MOD_CTRL;
    case KEY_LEFTALT: case KEY_RIGHTALT: return MOD_ALT;
    }
    return 0;
}

static void keyboard_event(const InputEvent *event) {
    int code = event->code, down = event->kind != INPUT_KEY_UP;
    if (code < 0 || code >= KEY_COUNT) return;
    if (down) keys_held[code / 8] |= (unsigned char)(1 << (code % 8));
    else keys_held[code / 8] &= (unsigned char)~(1 << (code % 8));
    int modifier = modifier_of(code);
    if (modifier) {
        if (down) modifiers |= modifier;
        else modifiers &= ~modifier;
        return;
    }
    if (!down) {
        KeyEvent key = { code, 0, modifiers };
        wm_key_event(&key, 1);
        return;
    }
    if (code == KEY_CAPSLOCK) {
        caps_lock = !caps_lock;
        return;
    }
    KeyEvent key = { code, key_character(code), modifiers };
    if (modifiers & (MOD_CTRL | MOD_ALT)) key.ch = 0;
    wm_key_event(&key, 0);
}

/* ------------------------------------------------------------------ the pointer */

static int pointer_x, pointer_y, pointer_buttons;
static int moved;                       /* a move not yet given to the window manager */
static long last_press_time = -100000;
static int last_press_x, last_press_y, last_press_button;

static void send_pointer(int kind, int button, int wheel) {
    MouseEvent event = { kind, pointer_x, pointer_y, button, pointer_buttons, 1, wheel };
    if (kind == MOUSE_DOWN) {
        long now = os_now_ms();
        int near = abs(pointer_x - last_press_x) < 5 && abs(pointer_y - last_press_y) < 5;
        if (button == last_press_button && near && now - last_press_time < os_settings()->double_click_ms) {
            event.clicks = 2;
            last_press_time = -100000;      /* a third press starts again */
        } else {
            last_press_time = now;
        }
        last_press_x = pointer_x;
        last_press_y = pointer_y;
        last_press_button = button;
    }
    wm_mouse_event(&event);
}

/* Moves wait until the batch of events is read, or until a button changes, so that a fast drag
 * redraws once per batch rather than once per pixel. */
static void flush_move(void) {
    if (moved) {
        moved = 0;
        send_pointer(MOUSE_MOVE, 0, 0);
    }
}

static void pointer_event(const InputEvent *event) {
    pointer_x = clamp_int(event->x, 0, screen_w - 1);
    pointer_y = clamp_int(event->y, 0, screen_h - 1);
    if (event->kind == INPUT_MOVE) {
        moved = 1;
    } else if (event->kind == INPUT_WHEEL) {
        flush_move();
        send_pointer(MOUSE_WHEEL, 0, event->code);
    } else {
        flush_move();
        if (event->kind == INPUT_BUTTON_DOWN) pointer_buttons |= event->code;
        else pointer_buttons &= ~event->code;
        send_pointer(event->kind == INPUT_BUTTON_DOWN ? MOUSE_DOWN : MOUSE_UP, event->code, 0);
    }
}

/* Waits up to timeout milliseconds (-1: for ever) for input, then hands over every event that
 * arrived. */
static void input_wait(int timeout) {
    InputEvent events[128];
    for (int count = platform_input_read(events, 128, timeout); count > 0;
         count = platform_input_read(events, 128, 0))
        for (int i = 0; i < count; i++) {
            if (events[i].kind <= INPUT_KEY_REPEAT) keyboard_event(&events[i]);
            else pointer_event(&events[i]);
        }
    flush_move();
}
