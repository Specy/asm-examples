/*
 * Hex Viewer: a file's bytes, sixteen to a row: the offset, the bytes in hexadecimal and the
 * characters they are. The file is the argument, one chosen with File > Open, or one dropped on
 * the window. The arrows move a cursor over the bytes, Go To jumps to an offset, and the status
 * line says what the byte under the cursor is.
 */

enum { HEX_LINE_H = 14, HEX_HEADER_H = 18, HEX_STATUS_H = 20, HEX_SCROLL_W = 16, HEX_PAD = 6 };
enum { HEX_OPEN = 1, HEX_GO_TO, HEX_EXIT };
enum { HEX_OFFSET_INK = 0x000080, HEX_FAINT = 0xa0a0a0, HEX_RULE = 0xd8d8d8 };

static MenuItem hex_file_menu[] = {
    { "Open...", HEX_OPEN, 0, "Ctrl+O", NULL },
    { "Go To...", HEX_GO_TO, 0, "Ctrl+G", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", HEX_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const Menu hex_menus[] = { { "File", hex_file_menu }, { NULL, NULL } };

typedef struct {
    Window *win;
    int w, h;                 /* the client area */
    char path[128];
    unsigned char *data;      /* the whole file, or NULL before one is open */
    long size;
    long top;                 /* the first row shown */
    long cursor;              /* the byte the cursor is on */
    ScrollBar scroll;
} HexViewer;

static const char HEX_DIGITS[] = "0123456789ABCDEF";

static int hex_rows(const HexViewer *v) {
    return max_int(1, (v->h - HEX_HEADER_H - HEX_STATUS_H - 4) / HEX_LINE_H);
}
static long hex_total_rows(const HexViewer *v) { return (v->size + 15) / 16; }
static long hex_most_top(const HexViewer *v) {
    return v->size ? max_int(0, (int)hex_total_rows(v) - hex_rows(v)) : 0;
}
static int hex_digits(const HexViewer *v) { return v->size > 0x1000000 ? 8 : 6; }
/* Where byte `i` of a row is, in characters from the left: its two digits, and its character. */
static int hex_column(const HexViewer *v, int i) { return hex_digits(v) + 2 + i * 3 + (i >= 8); }
static int hex_char_column(const HexViewer *v, int i) { return hex_digits(v) + 2 + 50 + i; }
static int hex_area_w(const HexViewer *v) { return v->w - HEX_SCROLL_W; }

/* ------------------------------------------------------------------ drawing */

static void hex_draw_header(HexViewer *v) {
    fill_rect(0, 0, hex_area_w(v), HEX_HEADER_H, FACE);
    hline(0, HEX_HEADER_H - 1, hex_area_w(v), SHADOW);
    draw_text(HEX_PAD, 3, "Offset", BLACK);
    for (int i = 0; i < 16; i++) {
        char label[3] = { '0', HEX_DIGITS[i], 0 };
        draw_text(HEX_PAD + hex_column(v, i) * FONT_WIDTH, 3, label, BLACK);
        draw_char(HEX_PAD + hex_char_column(v, i) * FONT_WIDTH, 3, HEX_DIGITS[i], BLACK);
    }
    fill_rect(v->w - HEX_SCROLL_W, 0, HEX_SCROLL_W, HEX_HEADER_H, FACE);
}

static void hex_draw_row(HexViewer *v, int r) {
    int y = HEX_HEADER_H + 2 + r * HEX_LINE_H;
    long start = (v->top + r) * 16;
    fill_rect(0, y, hex_area_w(v), HEX_LINE_H, WHITE);
    /* the rules between the offsets, the bytes and the characters */
    vline(HEX_PAD + hex_column(v, 0) * FONT_WIDTH - 9, y, HEX_LINE_H, HEX_RULE);
    vline(HEX_PAD + hex_char_column(v, 0) * FONT_WIDTH - 9, y, HEX_LINE_H, HEX_RULE);
    if (!v->data || start >= v->size) return;
    char offset[12];
    snprintf(offset, sizeof offset, "%0*lX", hex_digits(v), start);
    draw_text(HEX_PAD, y + 1, offset, HEX_OFFSET_INK);
    int focused = os_window_focused(v->win);
    uint32_t mark = focused ? os_theme_colour(THEME_SELECTION) : LIGHT;
    for (int i = 0; i < 16 && start + i < v->size; i++) {
        unsigned char byte = v->data[start + i];
        int x = HEX_PAD + hex_column(v, i) * FONT_WIDTH, cx = HEX_PAD + hex_char_column(v, i) * FONT_WIDTH;
        int here = start + i == v->cursor, printable = byte >= 32 && byte < 127;
        uint32_t ink = byte ? BLACK : HEX_FAINT, char_ink = printable ? BLACK : HEX_FAINT;
        if (here) {
            fill_rect(x - 2, y, 2 * FONT_WIDTH + 4, HEX_LINE_H, mark);
            fill_rect(cx, y, FONT_WIDTH, HEX_LINE_H, mark);
            if (focused) ink = char_ink = WHITE;
        }
        char pair[2] = { HEX_DIGITS[byte >> 4], HEX_DIGITS[byte & 15] };
        draw_text_n(x, y + 1, pair, 2, ink);
        draw_char(cx, y + 1, printable ? byte : '.', char_ink);
    }
}

static void hex_draw_status(HexViewer *v) {
    int y = v->h - HEX_STATUS_H;
    fill_rect(0, y, v->w, HEX_STATUS_H, FACE);
    char where[96], size[48];
    if (v->data && v->size) {
        unsigned char byte = v->data[v->cursor];
        snprintf(where, sizeof where, "Offset %lX (%ld): %02X = %d", v->cursor, v->cursor, byte, byte);
        size_t length = strlen(where);
        if (byte >= 32 && byte < 127) snprintf(where + length, sizeof where - length, " '%c'", byte);
    } else {
        copy_text(where, sizeof where, v->data ? "The file is empty." : "No file open.");
    }
    snprintf(size, sizeof size, "%ld bytes", v->size);
    int size_w = 130;
    bevel_thin_sunken(2, y + 2, v->w - size_w - 6, HEX_STATUS_H - 4);
    bevel_thin_sunken(v->w - size_w - 2, y + 2, size_w, HEX_STATUS_H - 4);
    gfx_clip(rect(4, y + 3, v->w - size_w - 10, HEX_STATUS_H - 6));
    draw_text(8, y + 4, where, BLACK);
    gfx_clip(rect(0, 0, v->w, v->h));
    if (v->data) draw_text(v->w - size_w + 4, y + 4, size, BLACK);
}

static void hex_draw_scroll(HexViewer *v) {
    v->scroll.r = rect(v->w - HEX_SCROLL_W, HEX_HEADER_H, HEX_SCROLL_W, v->h - HEX_HEADER_H - HEX_STATUS_H);
    v->scroll.total = (int)hex_total_rows(v);
    v->scroll.visible = hex_rows(v);
    v->scroll.top = (int)v->top;
    draw_scroll_bar(&v->scroll);
}

static void hex_draw_all(HexViewer *v) {
    os_window_draw(v->win);
    hex_draw_header(v);
    int bottom = v->h - HEX_STATUS_H;
    fill_rect(0, HEX_HEADER_H, hex_area_w(v), bottom - HEX_HEADER_H, WHITE);
    gfx_clip(rect(0, HEX_HEADER_H, hex_area_w(v), bottom - HEX_HEADER_H));
    if (v->data) {
        for (int r = 0; r < hex_rows(v); r++) hex_draw_row(v, r);
    } else {
        const char *hint = "Open a file with File > Open, or drop one here.";
        int y = HEX_HEADER_H + (bottom - HEX_HEADER_H) / 2 - 6;
        draw_text((hex_area_w(v) - text_width(hint)) / 2, y, hint, SHADOW);
    }
    gfx_clip(rect(0, 0, v->w, v->h));
    hex_draw_scroll(v);
    hex_draw_status(v);
    os_window_update_all(v->win);
}

/* Draws two rows again, the cursor's old one and its new one, and the status line. */
static void hex_draw_cursor_rows(HexViewer *v, long before) {
    os_window_draw(v->win);
    gfx_clip(rect(0, HEX_HEADER_H, hex_area_w(v), v->h - HEX_HEADER_H - HEX_STATUS_H));
    long rows[2] = { before / 16 - v->top, v->cursor / 16 - v->top };
    for (int i = 0; i < 2; i++) {
        if (rows[i] < 0 || rows[i] >= hex_rows(v)) continue;
        hex_draw_row(v, (int)rows[i]);
        os_window_update(v->win, 0, HEX_HEADER_H + 2 + (int)rows[i] * HEX_LINE_H, hex_area_w(v), HEX_LINE_H);
    }
    gfx_clip(rect(0, 0, v->w, v->h));
    hex_draw_status(v);
    os_window_update(v->win, 0, v->h - HEX_STATUS_H, v->w, HEX_STATUS_H);
}

/* ------------------------------------------------------------------ moving */

/* Puts the cursor on a byte and scrolls so that its row shows; answers whether it scrolled. */
static int hex_move(HexViewer *v, long cursor) {
    long top = v->top;
    v->cursor = v->size ? (cursor < 0 ? 0 : cursor >= v->size ? v->size - 1 : cursor) : 0;
    long row = v->cursor / 16;
    if (row < v->top) v->top = row;
    if (row >= v->top + hex_rows(v)) v->top = row - hex_rows(v) + 1;
    return top != v->top;
}

static void hex_scroll_to(HexViewer *v, long top) {
    top = top < 0 ? 0 : top > hex_most_top(v) ? hex_most_top(v) : top;
    if (top == v->top) return;
    v->top = top;
    hex_draw_all(v);
}

/* ------------------------------------------------------------------ files */

static void hex_title(HexViewer *v) {
    char title[128];
    if (v->data)
        snprintf(title, sizeof title, "%.60s (%ld bytes) - Hex Viewer", os_base_name(v->path), v->size);
    else
        copy_text(title, sizeof title, "Hex Viewer");
    os_window_set_title(v->win, title);
}

static void hex_load(HexViewer *v, const char *path) {
    long size;
    char *data = os_stat(path, NULL) == 1 ? os_read_file(path, &size) : NULL;
    if (!data) {
        char text[160];
        snprintf(text, sizeof text, "Cannot open %.100s.", path);
        os_message_box(v->win, "Hex Viewer", text, MB_OK, MB_WARNING);
        return;
    }
    os_free(v->data);
    v->data = (unsigned char *)data;
    v->size = size;
    v->top = v->cursor = 0;
    copy_text(v->path, sizeof v->path, path);
    hex_title(v);
    hex_draw_all(v);
}

static void hex_go_to(HexViewer *v) {
    if (!v->data || !v->size) return;
    char text[64], *end;
    snprintf(text, sizeof text, "%lX", v->cursor);
    if (!os_input_box(v->win, "Go To", "Go to the offset, in hexadecimal:", text, sizeof text)) return;
    const char *digits = text;
    while (*digits == ' ') digits++;
    if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) digits += 2;
    long offset = strtol(digits, &end, 16);
    while (*end == ' ') end++;
    if (end == digits || *end || offset < 0) {
        os_message_box(v->win, "Go To", "That is not a hexadecimal offset, such as 1A0.", MB_OK, MB_WARNING);
        return;
    }
    /* the offset's row goes to the top, unless it shows already */
    long row = min_int((int)(offset / 16), (int)hex_total_rows(v) - 1);
    if (row < v->top || row >= v->top + hex_rows(v)) v->top = row < hex_most_top(v) ? row : hex_most_top(v);
    hex_move(v, offset);
    hex_draw_all(v);
}

/* ------------------------------------------------------------------ commands, keys and the pointer */

static void hex_command(HexViewer *v, int id) {
    switch (id) {
    case HEX_OPEN: {
        char path[128];
        copy_text(path, sizeof path, v->path);
        if (os_file_dialog(v->win, 0, "Open", NULL, path, sizeof path)) hex_load(v, path);
        break;
    }
    case HEX_GO_TO: hex_go_to(v); break;
    case HEX_EXIT: os_exit(0);
    }
}

static void hex_key(HexViewer *v, const Message *m) {
    if (m->mods & MOD_CTRL) {
        if (m->key == KEY_O) hex_command(v, HEX_OPEN);
        else if (m->key == KEY_G) hex_command(v, HEX_GO_TO);
        return;
    }
    if (!v->data) return;
    long page = (long)(hex_rows(v) - 1) * 16, before = v->cursor, to;
    switch (m->key) {
    case KEY_LEFT: to = v->cursor - 1; break;
    case KEY_RIGHT: to = v->cursor + 1; break;
    case KEY_UP: to = v->cursor >= 16 ? v->cursor - 16 : v->cursor; break;
    case KEY_DOWN: to = v->cursor + 16 < v->size ? v->cursor + 16 : v->cursor; break;
    case KEY_PAGEUP:
        /* a page moves the view and the cursor together */
        v->top = v->top - (hex_rows(v) - 1) < 0 ? 0 : v->top - (hex_rows(v) - 1);
        to = v->cursor - page;
        break;
    case KEY_PAGEDOWN:
        v->top = v->top + (hex_rows(v) - 1) > hex_most_top(v) ? hex_most_top(v) : v->top + (hex_rows(v) - 1);
        to = v->cursor + page < v->size ? v->cursor + page : v->size - 1;
        break;
    case KEY_HOME: to = 0; break;
    case KEY_END: to = v->size - 1; break;
    default: return;
    }
    long top = v->top;
    int scrolled = m->key == KEY_PAGEUP || m->key == KEY_PAGEDOWN;
    scrolled |= hex_move(v, to);
    if (scrolled || top != v->top) hex_draw_all(v);
    else hex_draw_cursor_rows(v, before);
}

static void hex_mouse(HexViewer *v, const Message *m) {
    if (scroll_bar_message(&v->scroll, m)) {
        if (v->scroll.top != v->top) {
            v->top = v->scroll.top;
            hex_draw_all(v);
        } else {
            os_window_draw(v->win);
            draw_scroll_bar(&v->scroll);
            os_window_update(v->win, v->scroll.r.x, v->scroll.r.y, v->scroll.r.w, v->scroll.r.h);
        }
        return;
    }
    if (m->type == MSG_MOUSE_WHEEL) {
        hex_scroll_to(v, v->top - m->wheel * 3);
        return;
    }
    if (m->type != MSG_MOUSE_DOWN || m->button != BUTTON_LEFT || !v->data || m->y < HEX_HEADER_H + 2) return;
    /* a click on a byte's digits or its character puts the cursor on it */
    int r = (m->y - HEX_HEADER_H - 2) / HEX_LINE_H, column = (m->x - HEX_PAD) / FONT_WIDTH;
    if (r >= hex_rows(v)) return;
    for (int i = 0; i < 16; i++) {
        if (column == hex_column(v, i) || column == hex_column(v, i) + 1 || column == hex_char_column(v, i)) {
            long at = (v->top + r) * 16 + i, before = v->cursor;
            if (at >= v->size) return;
            hex_move(v, at);
            hex_draw_cursor_rows(v, before);
            return;
        }
    }
}

void hex_main(const char *argument) {
    HexViewer *v = os_alloc(sizeof *v);
    if (!v) os_exit(1);
    v->win = os_window_create("Hex Viewer", &ICON_HEX, 620, 320, WIN_RESIZABLE);
    if (!v->win) os_exit(1);
    os_window_set_menu(v->win, hex_menus);
    os_window_min_size(v->win, 300, 120);
    os_window_pixels(v->win, &v->w, &v->h);
    hex_draw_all(v);
    if (argument && argument[0]) hex_load(v, argument);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_KEY: hex_key(v, &m); break;
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: case MSG_MOUSE_WHEEL:
            hex_mouse(v, &m);
            break;
        case MSG_COMMAND: hex_command(v, m.id); break;
        case MSG_RESIZE:
            os_window_pixels(v->win, &v->w, &v->h);
            v->top = v->top > hex_most_top(v) ? hex_most_top(v) : v->top;
            hex_move(v, v->cursor);
            hex_draw_all(v);
            break;
        case MSG_FOCUS: case MSG_BLUR: case MSG_SETTINGS: hex_draw_all(v); break;
        case MSG_DROP: hex_load(v, m.text); break;
        default: os_default_message(&m);
        }
    }
}
