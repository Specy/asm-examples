/*
 * Calculator: the four operations, percent and sign, on a pocket calculator's keys. The keyboard
 * works too: digits, + - * / . %, Enter or = for the result, Backspace, and Escape to clear; the
 * key shows pushed in for a moment. Edit copies the number shown to the clipboard, or takes a
 * number from it.
 */

enum { CALC_W = 212, CALC_H = 258, CALC_PAD_W = 44, CALC_PAD_H = 32, CALC_PAD_GAP = 6, CALC_PADS_TOP = 64,
       CALC_KEYS = 20, CALC_DIGITS = 15 };
enum { CALC_COPY = 1, CALC_PASTE, CALC_ABOUT };
enum { CALC_FLASH = 1 };
enum { CALC_DISPLAY_X = 12, CALC_DISPLAY_Y = 10, CALC_DISPLAY_W = CALC_W - 24, CALC_DISPLAY_H = 44 };

static MenuItem calc_edit_menu[] = {
    { "Copy", CALC_COPY, 0, "Ctrl+C", NULL },
    { "Paste", CALC_PASTE, 0, "Ctrl+V", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem calc_help_menu[] = {
    { "About Calculator", CALC_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const Menu calc_menus[] = { { "Edit", calc_edit_menu }, { "Help", calc_help_menu }, { NULL, NULL } };

static const char CALC_DIVIDE_LABEL[] = { (char)CH_DIVIDE, 0 };
static const char CALC_TIMES_LABEL[] = { (char)CH_TIMES, 0 };
static const char CALC_BACK_LABEL[] = { (char)CH_LEFT, 0 };

/* The keys, row by row, and what each one does: 'n' changes the sign, 'b' takes back a digit. */
static const char *const CALC_LABELS[CALC_KEYS] = {
    "C", "+/-", "%", CALC_DIVIDE_LABEL, "7", "8", "9", CALC_TIMES_LABEL, "4", "5",
    "6", "-",   "1", "2",               "3", "+", "0", ".",              CALC_BACK_LABEL, "=",
};
static const char CALC_ACTIONS[CALC_KEYS] = { 'C', 'n', '%', '/', '7', '8', '9', '*', '4', '5',
                                              '6', '-', '1', '2', '3', '+', '0', '.', 'b', '=' };

typedef struct {
    Window *win;
    double total;            /* the left operand */
    char entry[24];          /* what is being typed, or the last result */
    int operation;           /* the pending one: + - * / or 0 */
    int fresh;               /* the next digit starts a new number */
    int waiting;             /* the number shown is the left operand: the right one is still to come */
    int error;
    int flash;               /* the key a key press shows pushed in for a moment, or -1 */
    ButtonTracker buttons;
} Calculator;

static Rect calc_key_rect(int index) {
    int row = index / 4, column = index % 4;
    return rect(12 + column * (CALC_PAD_W + CALC_PAD_GAP), CALC_PADS_TOP + row * (CALC_PAD_H + CALC_PAD_GAP),
                CALC_PAD_W, CALC_PAD_H);
}

/* ------------------------------------------------------------------ arithmetic */

static void calc_show(Calculator *c, double value) {
    if (!isfinite(value)) {
        copy_text(c->entry, sizeof c->entry, "Error");
        c->error = 1;
        os_beep();
        return;
    }
    if (value == 0) value = 0;    /* no "-0" */
    snprintf(c->entry, sizeof c->entry, "%.10g", value);
}

static double calc_apply(double left, int operation, double right) {
    switch (operation) {
    case '+': return left + right;
    case '-': return left - right;
    case '*': return left * right;
    case '/': return right == 0 ? NAN : left / right;
    }
    return right;
}

static void calc_clear(Calculator *c) {
    c->total = 0;
    c->operation = 0;
    c->fresh = c->waiting = 0;
    c->error = 0;
    copy_text(c->entry, sizeof c->entry, "0");
}

/* Starts typing a new number in place of what is shown, when the last key finished one. */
static void calc_begin_number(Calculator *c) {
    if (!c->fresh) return;
    copy_text(c->entry, sizeof c->entry, "0");
    c->fresh = c->waiting = 0;
}

static void calc_press(Calculator *c, int action) {
    if (c->error && action != 'C') {
        os_beep();
        return;
    }
    double value = atof(c->entry);
    if (action >= '0' && action <= '9') {
        calc_begin_number(c);
        size_t length = strlen(c->entry);
        if (strcmp(c->entry, "0") == 0 || strcmp(c->entry, "-0") == 0) c->entry[--length] = 0;
        if (length < CALC_DIGITS) {
            c->entry[length] = (char)action;
            c->entry[length + 1] = 0;
        }
    } else if (action == '.') {
        calc_begin_number(c);
        if (!strchr(c->entry, '.') && strlen(c->entry) < CALC_DIGITS) strcat(c->entry, ".");
    } else if (action == 'b') {
        size_t length = strlen(c->entry);
        if (c->fresh) return;
        if (length > 1 && !(length == 2 && c->entry[0] == '-')) c->entry[length - 1] = 0;
        else copy_text(c->entry, sizeof c->entry, "0");
    } else if (action == 'n') {
        /* what is being typed changes its sign as written, so that "0." becomes "-0." */
        size_t length = strlen(c->entry);
        if (c->fresh) calc_show(c, -value);
        else if (c->entry[0] == '-') memmove(c->entry, c->entry + 1, length);
        else if (strcmp(c->entry, "0") != 0 && length + 1 < sizeof c->entry) {
            memmove(c->entry + 1, c->entry, length + 1);
            c->entry[0] = '-';
        }
        c->waiting = 0;
    } else if (action == '%') {
        calc_show(c, c->operation ? c->total * value / 100 : value / 100);
        c->fresh = 1;
        c->waiting = 0;
    } else if (action == 'C') {
        calc_clear(c);
    } else {
        /* an operation, or =: finish the one pending first */
        if (c->operation && !c->waiting) {
            value = calc_apply(c->total, c->operation, value);
            calc_show(c, value);
            if (c->error) return;
        } else {
            calc_show(c, value);
        }
        c->total = value;
        c->operation = action == '=' ? 0 : action;
        c->fresh = c->waiting = 1;
    }
}

/* ------------------------------------------------------------------ drawing */

static void calc_draw_display(Calculator *c) {
    os_window_draw(c->win);
    draw_field(CALC_DISPLAY_X, CALC_DISPLAY_Y, CALC_DISPLAY_W, CALC_DISPLAY_H, 0xe8f4e0);
    int right = CALC_DISPLAY_X + CALC_DISPLAY_W - 9;
    if (c->operation) {
        /* the left operand and the operation waiting for the right one, small above the number */
        char pending[40];
        int symbol = c->operation == '*' ? CH_TIMES : c->operation == '/' ? CH_DIVIDE : c->operation;
        snprintf(pending, sizeof pending, "%.10g %c", c->total, symbol);
        draw_text(right - text_width(pending) + 1, CALC_DISPLAY_Y + 6, pending, SHADOW);
    }
    draw_text_bold(right - text_width(c->entry), CALC_DISPLAY_Y + 24, c->entry, c->error ? 0xb00000 : BLACK);
    os_window_update(c->win, CALC_DISPLAY_X, CALC_DISPLAY_Y, CALC_DISPLAY_W, CALC_DISPLAY_H);
}

static void calc_draw_key(Calculator *c, int index) {
    if (index < 0 || index >= CALC_KEYS) return;
    os_window_draw(c->win);
    Rect key = calc_key_rect(index);
    int down = button_down(&c->buttons, index) || c->flash == index;
    draw_button(key.x, key.y, key.w, key.h, "", down);
    char action = CALC_ACTIONS[index];
    uint32_t colour = action >= '0' && action <= '9' ? NAVY : action == 'C' || action == 'b' ? 0xb00000 : BLACK;
    const char *label = CALC_LABELS[index];
    draw_text_bold(key.x + (key.w - text_width(label)) / 2 + down, key.y + 10 + down, label, colour);
    os_window_update(c->win, key.x, key.y, key.w, key.h);
}

static void calc_draw_all(Calculator *c) {
    os_window_draw(c->win);
    fill_rect(0, 0, CALC_W, CALC_H, FACE);
    calc_draw_display(c);
    for (int i = 0; i < CALC_KEYS; i++) calc_draw_key(c, i);
    os_window_update_all(c->win);
}

/* ------------------------------------------------------------------ the clipboard */

static void calc_copy(Calculator *c) {
    if (c->error) {
        os_beep();
        return;
    }
    os_clipboard_set_text(c->entry, (int)strlen(c->entry));
}

/* Takes a number from the clipboard's text, such as "1,234.5" or " -7 ", as if typed. */
static void calc_paste(Calculator *c) {
    int length;
    char *text = os_clipboard_text(&length);
    if (!text) {
        os_beep();
        return;
    }
    char number[32];
    int kept = 0;
    for (int i = 0; i < length && kept < (int)sizeof number - 1; i++)
        if (!isspace((unsigned char)text[i]) && text[i] != ',') number[kept++] = text[i];
    number[kept] = 0;
    os_free(text);
    char *end;
    double value = strtod(number, &end);
    if (!kept || *end || !isfinite(value)) {
        os_beep();
        return;
    }
    if (c->error) calc_clear(c);
    calc_show(c, value);
    c->waiting = 0;
    /* a short number keeps the digits it was written with, so that Backspace takes them back */
    if (kept <= CALC_DIGITS && strspn(number, "-0123456789.") == (size_t)kept)
        copy_text(c->entry, sizeof c->entry, number);
    c->fresh = 0;
}

/* ------------------------------------------------------------------ messages */

static void calc_command(Calculator *c, int id) {
    switch (id) {
    case CALC_COPY: calc_copy(c); return;
    case CALC_PASTE: calc_paste(c); break;
    case CALC_ABOUT:
        os_message_box(c->win, "About Calculator",
                       "Calculator for asmOS.\nType with the keys as well: digits, + - * / %, Enter for the "
                       "result, Backspace, and Escape to clear.",
                       MB_OK, MB_INFO);
        return;
    }
    calc_draw_display(c);
}

static void calc_key_message(Calculator *c, const Message *m) {
    if (m->mods & MOD_CTRL) {
        if (m->key == KEY_C) calc_command(c, CALC_COPY);
        if (m->key == KEY_V) calc_command(c, CALC_PASTE);
        return;
    }
    int action = 0;
    if (m->ch && strchr("0123456789.+-*/%=", m->ch)) action = m->ch;
    else if (m->ch == ',') action = '.';
    else if (m->ch == 'x' || m->ch == 'X') action = '*';
    else if (m->key == KEY_ENTER || m->key == KEY_KPENTER) action = '=';
    else if (m->key == KEY_BACKSPACE) action = 'b';
    else if (m->key == KEY_ESC || m->key == KEY_DELETE) action = 'C';
    else if (m->key == KEY_F9) action = 'n';
    if (!action) return;
    calc_press(c, action);
    calc_draw_display(c);
    /* show the key the keyboard pressed pushed in, until the timer lets it up */
    int before = c->flash;
    c->flash = -1;
    for (int i = 0; i < CALC_KEYS; i++)
        if (CALC_ACTIONS[i] == action) c->flash = i;
    calc_draw_key(c, before);
    calc_draw_key(c, c->flash);
    os_set_timer(c->win, CALC_FLASH, 120);
}

static void calc_mouse(Calculator *c, const Message *m) {
    Rect keys[CALC_KEYS];
    for (int i = 0; i < CALC_KEYS; i++) keys[i] = calc_key_rect(i);
    int before = c->buttons.pressed, changed;
    int clicked = track_buttons(&c->buttons, keys, CALC_KEYS, m, &changed);
    if (changed) {
        calc_draw_key(c, before);
        if (c->buttons.pressed != before) calc_draw_key(c, c->buttons.pressed);
    }
    if (clicked >= 0) {
        calc_press(c, CALC_ACTIONS[clicked]);
        calc_draw_display(c);
    }
}

void calculator_main(const char *argument) {
    (void)argument;
    Calculator *c = os_alloc(sizeof *c);
    if (!c) os_exit(1);
    c->win = os_window_create("Calculator", &ICON_CALCULATOR, CALC_W, CALC_H, 0);
    if (!c->win) os_exit(1);
    os_window_set_menu(c->win, calc_menus);
    c->buttons.pressed = -1;
    c->flash = -1;
    calc_clear(c);
    calc_draw_all(c);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_KEY: calc_key_message(c, &m); break;
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: calc_mouse(c, &m); break;
        case MSG_COMMAND: calc_command(c, m.id); break;
        case MSG_TIMER: {
            int flashed = c->flash;
            c->flash = -1;
            os_set_timer(c->win, CALC_FLASH, 0);
            calc_draw_key(c, flashed);
            break;
        }
        case MSG_RESIZE: calc_draw_all(c); break;
        default: os_default_message(&m);
        }
    }
}
