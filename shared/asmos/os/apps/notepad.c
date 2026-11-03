/*
 * Notepad: plain text, one file a window. The Project is the working directory, so Save writes a
 * File you can open in the editor, and Open, a file dropped on the window, or `edit` in the
 * Terminal opens one. Text is selected with the mouse or Shift and the arrows, and cut, copied and
 * pasted through the clipboard the other applications share.
 */

enum { NOTE_CAPACITY = 65536, NOTE_LINE_H = 14, NOTE_STATUS_H = 20, NOTE_SCROLL_W = 16 };
enum { NOTE_NEW = 1, NOTE_OPEN, NOTE_SAVE, NOTE_SAVE_AS, NOTE_EXIT, NOTE_CUT, NOTE_COPY, NOTE_PASTE,
       NOTE_DELETE, NOTE_SELECT_ALL, NOTE_TIME_DATE, NOTE_ABOUT };
enum { NOTE_BLINK = 1 };

static MenuItem note_file_menu[] = {
    { "New", NOTE_NEW, 0, NULL, NULL },
    { "Open...", NOTE_OPEN, 0, NULL, NULL },
    { "Save", NOTE_SAVE, 0, "Ctrl+S", NULL },
    { "Save As...", NOTE_SAVE_AS, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", NOTE_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem note_edit_menu[] = {
    { "Cut", NOTE_CUT, 0, "Ctrl+X", NULL },
    { "Copy", NOTE_COPY, 0, "Ctrl+C", NULL },
    { "Paste", NOTE_PASTE, 0, "Ctrl+V", NULL },
    { "Delete", NOTE_DELETE, 0, "Del", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Select All", NOTE_SELECT_ALL, 0, "Ctrl+A", NULL },
    { "Time/Date", NOTE_TIME_DATE, 0, "F5", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem note_help_menu[] = {
    { "About Notepad", NOTE_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const Menu note_menus[] = {
    { "File", note_file_menu }, { "Edit", note_edit_menu }, { "Help", note_help_menu }, { NULL, NULL },
};
static const MenuItem note_context_menu[] = {
    { "Cut", NOTE_CUT, 0, NULL, NULL },
    { "Copy", NOTE_COPY, 0, NULL, NULL },
    { "Paste", NOTE_PASTE, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Select All", NOTE_SELECT_ALL, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

typedef struct {
    Window *win;
    char *text;
    int length;
    int cursor, anchor;       /* the selection is between them */
    int top, left;            /* the first line and column shown */
    int goal;                 /* the column up and down keep to */
    int blink, modified, selecting;
    char path[128];
    char message[64];
    ScrollBar scroll;
    int w, h;                 /* the client area */
} Notepad;

static Rect note_area(Notepad *n) { return rect(0, 0, n->w - NOTE_SCROLL_W, n->h - NOTE_STATUS_H); }
static int note_rows(Notepad *n) { return max_int(1, (note_area(n).h - 6) / NOTE_LINE_H); }
static int note_columns(Notepad *n) { return max_int(1, (note_area(n).w - 8) / FONT_WIDTH); }

static int line_start(const Notepad *n, int index) {
    while (index > 0 && n->text[index - 1] != '\n') index--;
    return index;
}
static int line_end(const Notepad *n, int index) {
    while (index < n->length && n->text[index] != '\n') index++;
    return index;
}
static int line_number(const Notepad *n, int index) {
    int line = 0;
    for (int i = 0; i < index; i++) line += n->text[i] == '\n';
    return line;
}
static int index_of_line(const Notepad *n, int line) {
    int index = 0;
    while (line > 0 && index < n->length)
        if (n->text[index++] == '\n') line--;
    return index;
}
static int index_at(const Notepad *n, int line, int column) {
    int start = index_of_line(n, line), end = line_end(n, start);
    return start + min_int(column, end - start);
}
static int sel_from(const Notepad *n) { return min_int(n->cursor, n->anchor); }
static int sel_to(const Notepad *n) { return max_int(n->cursor, n->anchor); }

/* ------------------------------------------------------------------ drawing */

static void note_draw_line(Notepad *n, int row, int index) {
    Rect area = note_area(n);
    int y = 3 + row * NOTE_LINE_H;
    fill_rect(2, y, area.w - 4, NOTE_LINE_H, WHITE);
    if (index > n->length) return;
    int end = line_end(n, index), shown = note_columns(n) + 1;
    int from = index + n->left, count = max_int(0, min_int(end - from, shown));
    if (count) draw_text_n(4, y + 1, n->text + from, count, BLACK);
    int a = max_int(sel_from(n), from), b = min_int(sel_to(n), from + count + (end < n->length));
    if (a < b) {
        int x = 4 + (a - from) * FONT_WIDTH;
        fill_rect(x, y, (b - a) * FONT_WIDTH, NOTE_LINE_H, os_theme_colour(THEME_SELECTION));
        draw_text_n(x, y + 1, n->text + a, max_int(0, min_int(b, end) - a), WHITE);
    }
    if (n->blink && n->anchor == n->cursor && os_window_focused(n->win) && n->cursor >= index && n->cursor <= end) {
        int column = n->cursor - index - n->left;
        if (column >= 0 && column <= note_columns(n)) fill_rect(4 + column * FONT_WIDTH - 1, y, 2, NOTE_LINE_H, BLACK);
    }
}

static void note_draw_status(Notepad *n) {
    int y = n->h - NOTE_STATUS_H;
    fill_rect(0, y, n->w, NOTE_STATUS_H, FACE);
    char status[48];
    snprintf(status, sizeof status, "Ln %d, Col %d", line_number(n, n->cursor) + 1,
             n->cursor - line_start(n, n->cursor) + 1);
    bevel_thin_sunken(2, y + 2, 120, NOTE_STATUS_H - 4);
    bevel_thin_sunken(126, y + 2, n->w - 128, NOTE_STATUS_H - 4);
    draw_text(8, y + 4, status, BLACK);
    draw_text(132, y + 4, n->message, BLACK);
}

static void note_draw_scroll(Notepad *n) {
    n->scroll.r = rect(n->w - NOTE_SCROLL_W, 0, NOTE_SCROLL_W, n->h - NOTE_STATUS_H);
    n->scroll.total = line_number(n, n->length) + 1;
    n->scroll.visible = note_rows(n);
    n->scroll.top = n->top;
    draw_scroll_bar(&n->scroll);
}

static void note_draw_all(Notepad *n) {
    os_window_draw(n->win);
    Rect area = note_area(n);
    bevel_sunken(area.x, area.y, area.w, area.h);
    fill_rect(2, 2, area.w - 4, area.h - 4, WHITE);
    gfx_clip(rect(2, 2, area.w - 4, area.h - 4));
    int index = index_of_line(n, n->top);
    for (int row = 0; row < note_rows(n); row++) {
        note_draw_line(n, row, index);
        int end = line_end(n, index);
        if (end >= n->length) {
            for (row++; row < note_rows(n); row++) note_draw_line(n, row, n->length + 1);
            break;
        }
        index = end + 1;
    }
    gfx_clip(rect(0, 0, n->w, n->h));
    note_draw_scroll(n);
    note_draw_status(n);
    os_window_update_all(n->win);
}

/* Redraws the lines from `first` to `last` (text lines, not rows) and the status bar. */
static void note_draw_lines(Notepad *n, int first, int last) {
    os_window_draw(n->win);
    Rect area = note_area(n);
    gfx_clip(rect(2, 2, area.w - 4, area.h - 4));
    first = max_int(first, n->top);
    last = min_int(last, n->top + note_rows(n) - 1);
    for (int line = first; line <= last; line++) note_draw_line(n, line - n->top, index_of_line(n, line));
    gfx_clip(rect(0, 0, n->w, n->h));
    note_draw_status(n);
    if (first <= last)
        os_window_update(n->win, 0, 3 + (first - n->top) * NOTE_LINE_H, area.w, (last - first + 1) * NOTE_LINE_H);
    os_window_update(n->win, 0, n->h - NOTE_STATUS_H, n->w, NOTE_STATUS_H);
}

static void note_title(Notepad *n) {
    char title[96];
    snprintf(title, sizeof title, "%s%s - Notepad", n->modified ? "*" : "",
             n->path[0] ? os_base_name(n->path) : "Untitled");
    os_window_set_title(n->win, title);
}

/* Scrolls so that the cursor shows; answers whether anything scrolled. */
static int note_follow(Notepad *n) {
    int line = line_number(n, n->cursor), column = n->cursor - line_start(n, n->cursor);
    int top = n->top, left = n->left;
    if (line < n->top) n->top = line;
    if (line >= n->top + note_rows(n)) n->top = line - note_rows(n) + 1;
    if (column < n->left) n->left = max_int(0, column - 8);
    if (column >= n->left + note_columns(n)) n->left = column - note_columns(n) + 8;
    return top != n->top || left != n->left;
}

/* ------------------------------------------------------------------ editing */

static void note_delete_range(Notepad *n, int from, int to) {
    if (from >= to) return;
    memmove(n->text + from, n->text + to, n->length - to);
    n->length -= to - from;
    n->text[n->length] = 0;
    n->cursor = n->anchor = from;
    n->modified = 1;
}

static void note_insert(Notepad *n, const char *text, int count) {
    note_delete_range(n, sel_from(n), sel_to(n));
    if (n->length + count >= NOTE_CAPACITY) {
        os_beep();
        return;
    }
    memmove(n->text + n->cursor + count, n->text + n->cursor, n->length - n->cursor);
    memcpy(n->text + n->cursor, text, count);
    n->length += count;
    n->cursor += count;
    n->anchor = n->cursor;
    n->text[n->length] = 0;
    n->modified = 1;
}

static void note_copy(Notepad *n) {
    if (sel_from(n) < sel_to(n)) os_clipboard_set_text(n->text + sel_from(n), sel_to(n) - sel_from(n));
}

static void note_paste(Notepad *n) {
    int length;
    char *text = os_clipboard_text(&length);
    if (!text) {
        os_beep();
        return;
    }
    int kept = 0;
    for (int i = 0; i < length; i++)
        if (text[i] != '\r') text[kept++] = text[i];
    note_insert(n, text, kept);
    os_free(text);
}

/* ------------------------------------------------------------------ files */

static int note_load(Notepad *n, const char *path) {
    long size;
    char *data = os_read_file(path, &size);
    if (!data) {
        snprintf(n->message, sizeof n->message, "Cannot open %.40s", os_base_name(path));
        return 0;
    }
    int kept = 0;
    for (long i = 0; i < size && kept < NOTE_CAPACITY - 1; i++)
        if (data[i] != '\r') n->text[kept++] = data[i];
    n->text[kept] = 0;
    n->length = kept;
    os_free(data);
    copy_text(n->path, sizeof n->path, path);
    n->cursor = n->anchor = n->top = n->left = n->goal = 0;
    n->modified = 0;
    snprintf(n->message, sizeof n->message, "Opened %.32s, %d bytes", os_base_name(path), kept);
    return 1;
}

/* Saves under the file's name, or asks for one; answers 1 when saved. */
static int note_save(Notepad *n, int ask) {
    if (ask || !n->path[0]) {
        char path[128];
        copy_text(path, sizeof path, n->path[0] ? n->path : "Untitled.txt");
        if (!os_file_dialog(n->win, 1, "Save As", ".txt", path, sizeof path)) return 0;
        copy_text(n->path, sizeof n->path, path);
    }
    if (os_write_file(n->path, n->text, n->length) != 0) {
        os_message_box(n->win, "Notepad", "The file could not be saved.", MB_OK, MB_WARNING);
        return 0;
    }
    n->modified = 0;
    snprintf(n->message, sizeof n->message, "Saved %.32s, %d bytes", os_base_name(n->path), n->length);
    note_title(n);
    return 1;
}

/* Before the text goes: answers 1 to go on, after saving if asked to. */
static int note_confirm(Notepad *n) {
    if (!n->modified) return 1;
    char question[160];
    snprintf(question, sizeof question, "Do you want to save the changes to %.60s?",
             n->path[0] ? os_base_name(n->path) : "Untitled");
    int answer = os_message_box(n->win, "Notepad", question, MB_YES_NO_CANCEL, MB_QUESTION);
    if (answer == ID_YES) return note_save(n, 0);
    return answer == ID_NO;
}

static void note_open_path(Notepad *n, const char *path) {
    if (!note_confirm(n)) return;
    note_load(n, path);
    note_title(n);
    note_draw_all(n);
}

/* ------------------------------------------------------------------ commands and keys */

static void note_command(Notepad *n, int id) {
    switch (id) {
    case NOTE_NEW:
        if (!note_confirm(n)) return;
        n->length = n->cursor = n->anchor = n->top = n->left = 0;
        n->text[0] = 0;
        n->path[0] = 0;
        n->modified = 0;
        copy_text(n->message, sizeof n->message, "New file");
        break;
    case NOTE_OPEN: {
        char path[128];
        copy_text(path, sizeof path, n->path);
        if (os_file_dialog(n->win, 0, "Open", NULL, path, sizeof path)) note_open_path(n, path);
        return;
    }
    case NOTE_SAVE: note_save(n, 0); break;
    case NOTE_SAVE_AS: note_save(n, 1); break;
    case NOTE_EXIT:
        if (note_confirm(n)) os_exit(0);
        return;
    case NOTE_CUT:
        note_copy(n);
        note_delete_range(n, sel_from(n), sel_to(n));
        break;
    case NOTE_COPY: note_copy(n); break;
    case NOTE_PASTE: note_paste(n); break;
    case NOTE_DELETE:
        if (sel_from(n) < sel_to(n)) note_delete_range(n, sel_from(n), sel_to(n));
        else note_delete_range(n, n->cursor, min_int(n->length, n->cursor + 1));
        break;
    case NOTE_SELECT_ALL:
        n->anchor = 0;
        n->cursor = n->length;
        break;
    case NOTE_TIME_DATE: {
        DateTime now;
        os_time(&now);
        char stamp[32];
        snprintf(stamp, sizeof stamp, "%02d:%02d %d-%02d-%02d", now.hour, now.minute, now.year, now.month, now.day);
        note_insert(n, stamp, (int)strlen(stamp));
        break;
    }
    case NOTE_ABOUT:
        os_message_box(n->win, "About Notepad",
                       "Notepad for asmOS.\nIt runs as its own process: open two and both keep running.", MB_OK,
                       MB_INFO);
        return;
    }
    n->blink = 1;
    note_follow(n);
    note_title(n);
    note_draw_all(n);
}

static void note_key(Notepad *n, const Message *m) {
    int before = line_number(n, n->cursor), had_selection = n->anchor != n->cursor;
    int whole = 0, vertical = 0, moving = 0, modified = n->modified;
    if (m->mods & MOD_CTRL) {
        int command = m->key == KEY_S ? NOTE_SAVE : m->key == KEY_C ? NOTE_COPY : m->key == KEY_X ? NOTE_CUT
                      : m->key == KEY_V ? NOTE_PASTE : m->key == KEY_A ? NOTE_SELECT_ALL : m->key == KEY_O ? NOTE_OPEN : 0;
        if (command) note_command(n, command);
        return;
    }
    int start = line_start(n, n->cursor);
    switch (m->key) {
    case KEY_LEFT: n->cursor = max_int(0, n->cursor - 1); moving = 1; break;
    case KEY_RIGHT: n->cursor = min_int(n->length, n->cursor + 1); moving = 1; break;
    case KEY_HOME: n->cursor = start; moving = 1; break;
    case KEY_END: n->cursor = line_end(n, n->cursor); moving = 1; break;
    case KEY_UP: case KEY_DOWN: case KEY_PAGEUP: case KEY_PAGEDOWN: {
        int step = m->key == KEY_UP ? -1 : m->key == KEY_DOWN ? 1 : m->key == KEY_PAGEUP ? -note_rows(n) : note_rows(n);
        n->cursor = index_at(n, max_int(0, before + step), n->goal);
        moving = vertical = 1;
        break;
    }
    case KEY_BACKSPACE:
        if (had_selection) note_delete_range(n, sel_from(n), sel_to(n)), whole = 1;
        else if (n->cursor > 0) whole = n->text[n->cursor - 1] == '\n', note_delete_range(n, n->cursor - 1, n->cursor);
        break;
    case KEY_DELETE:
        whole = had_selection || (n->cursor < n->length && n->text[n->cursor] == '\n');
        note_command(n, NOTE_DELETE);
        return;
    case KEY_ENTER: case KEY_KPENTER: note_insert(n, "\n", 1); whole = 1; break;
    case KEY_TAB: note_insert(n, "    ", 4); break;
    case KEY_F5: note_command(n, NOTE_TIME_DATE); return;
    default:
        if (m->ch >= 32 && m->ch < 127) {
            char ch = (char)m->ch;
            whole = had_selection;
            note_insert(n, &ch, 1);
        } else {
            return;
        }
    }
    if (moving && !(m->mods & MOD_SHIFT)) {
        whole |= had_selection;
        n->anchor = n->cursor;
    } else if (moving) {
        whole = 1;
    }
    if (!vertical) n->goal = n->cursor - line_start(n, n->cursor);
    n->blink = 1;
    if (note_follow(n)) whole = 1;
    if (n->modified != modified) note_title(n);
    if (whole) {
        note_draw_all(n);
    } else {
        int after = line_number(n, n->cursor);
        note_draw_lines(n, min_int(before, after), max_int(before, after));
    }
}

static void note_mouse(Notepad *n, const Message *m) {
    if (scroll_bar_message(&n->scroll, m)) {
        if (n->scroll.top != n->top) {
            n->top = n->scroll.top;
            note_draw_all(n);
        } else {
            os_window_draw(n->win);
            note_draw_scroll(n);
            os_window_update(n->win, n->scroll.r.x, n->scroll.r.y, n->scroll.r.w, n->scroll.r.h);
        }
        return;
    }
    if (m->type == MSG_MOUSE_WHEEL) {
        int lines = line_number(n, n->length) + 1;
        n->top = clamp_int(n->top - m->wheel * 3, 0, max_int(0, lines - note_rows(n)));
        note_draw_all(n);
        return;
    }
    Rect area = note_area(n);
    if (m->type == MSG_MOUSE_DOWN && m->button == BUTTON_RIGHT && rect_contains(area, m->x, m->y)) {
        int id = os_popup_menu(n->win, m->x, m->y, note_context_menu);
        if (id) note_command(n, id);
        return;
    }
    if (m->type == MSG_MOUSE_UP) n->selecting = 0;
    int dragging = m->type == MSG_MOUSE_MOVE && n->selecting;
    if (!(m->type == MSG_MOUSE_DOWN && m->button == BUTTON_LEFT && rect_contains(area, m->x, m->y)) && !dragging) return;
    int line = n->top + max_int(0, (m->y - 3) / NOTE_LINE_H);
    int column = n->left + max_int(0, (m->x - 4 + FONT_WIDTH / 2) / FONT_WIDTH);
    n->cursor = index_at(n, line, column);
    if (m->type == MSG_MOUSE_DOWN) {
        n->selecting = 1;
        if (!(m->mods & MOD_SHIFT)) n->anchor = n->cursor;
        if (m->clicks == 2) {
            /* a double click selects the word */
            int a = n->cursor, b = n->cursor;
            while (a > 0 && (isalnum((unsigned char)n->text[a - 1]) || n->text[a - 1] == '_')) a--;
            while (b < n->length && (isalnum((unsigned char)n->text[b]) || n->text[b] == '_')) b++;
            n->anchor = a;
            n->cursor = b;
        }
    }
    n->goal = n->cursor - line_start(n, n->cursor);
    n->blink = 1;
    note_follow(n);
    note_draw_all(n);
}

static void note_relayout(Notepad *n) {
    os_window_pixels(n->win, &n->w, &n->h);
    note_follow(n);
    note_draw_all(n);
}

void notepad_main(const char *argument) {
    Notepad *n = os_alloc(sizeof *n);
    if (!n || !(n->text = os_alloc(NOTE_CAPACITY))) os_exit(1);
    n->win = os_window_create("Notepad", &ICON_NOTEPAD, 460, 290, WIN_RESIZABLE);
    if (!n->win) os_exit(1);
    os_window_set_menu(n->win, note_menus);
    os_window_min_size(n->win, 220, 100);
    n->blink = 1;
    copy_text(n->message, sizeof n->message, "Type, then Save (Ctrl+S)");
    if (argument && argument[0]) {
        if (!note_load(n, argument)) {
            copy_text(n->path, sizeof n->path, argument);
            snprintf(n->message, sizeof n->message, "New file %.40s", os_base_name(argument));
        }
    }
    note_title(n);
    note_relayout(n);
    os_set_timer(n->win, NOTE_BLINK, 530);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_KEY: note_key(n, &m); break;
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: case MSG_MOUSE_WHEEL: note_mouse(n, &m); break;
        case MSG_COMMAND: note_command(n, m.id); break;
        case MSG_TIMER: {
            n->blink = !n->blink;
            int line = line_number(n, n->cursor);
            note_draw_lines(n, line, line);
            break;
        }
        case MSG_RESIZE: note_relayout(n); break;
        case MSG_FOCUS: case MSG_BLUR: case MSG_SETTINGS: note_draw_all(n); break;
        case MSG_DROP: note_open_path(n, m.text); break;
        case MSG_CLOSE:
            if (note_confirm(n)) os_exit(0);
            break;
        default: os_default_message(&m);
        }
    }
}
