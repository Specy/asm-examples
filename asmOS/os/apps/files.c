/*
 * Files: a browser of the Project's files and folders. A double click (or Enter) opens a folder,
 * or a file with its application; the right button offers Open With, Rename, Delete and Copy. A
 * file dragged out of the list can be dropped on the desktop, which copies it into the Desktop
 * folder, or on another application's window, which opens it. Copy puts "file:" and the file's
 * path on the clipboard, and Paste copies that file into the folder shown, as dropping a file on
 * the window does.
 */

enum { FILES_MAX = 256, FILES_DEPTH = 10, FILES_ROW_H = 18, FILES_BAR_H = 30, FILES_STATUS_H = 22,
       FILES_SCROLL_W = 16, FILES_SIZE_W = 72, FILES_TYPE_W = 92 };
enum { FILES_OPEN = 1, FILES_NEW_FOLDER, FILES_RENAME, FILES_DELETE, FILES_TERMINAL, FILES_CLOSE, FILES_COPY,
       FILES_PASTE, FILES_REFRESH, FILES_UP, FILES_WITH = 100 };
enum { FILES_BUTTON_UP, FILES_BUTTON_REFRESH, FILES_BUTTONS };

static MenuItem files_file_menu[] = {
    { "Open", FILES_OPEN, 0, "Enter", NULL },
    { "New Folder", FILES_NEW_FOLDER, 0, NULL, NULL },
    { "Rename...", FILES_RENAME, 0, "F2", NULL },
    { "Delete", FILES_DELETE, 0, "Del", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Terminal Here", FILES_TERMINAL, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Close", FILES_CLOSE, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem files_edit_menu[] = {
    { "Copy", FILES_COPY, 0, "Ctrl+C", NULL },
    { "Paste", FILES_PASTE, 0, "Ctrl+V", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem files_view_menu[] = {
    { "Refresh", FILES_REFRESH, 0, "F5", NULL },
    { "Up One Folder", FILES_UP, 0, "Backspace", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const Menu files_menus[] = {
    { "File", files_file_menu }, { "Edit", files_edit_menu }, { "View", files_view_menu }, { NULL, NULL },
};
static const MenuItem files_with_menu[] = {
    { "Notepad", FILES_WITH + APP_NOTEPAD, 0, NULL, NULL },
    { "Hex Viewer", FILES_WITH + APP_HEX, 0, NULL, NULL },
    { "Assembly Editor", FILES_WITH + APP_IDE, 0, NULL, NULL },
    { "Image Viewer", FILES_WITH + APP_VIEWER, 0, NULL, NULL },
    { "Paint", FILES_WITH + APP_PAINT, 0, NULL, NULL },
    { "Music Player", FILES_WITH + APP_MUSIC, 0, NULL, NULL },
    { "Video Player", FILES_WITH + APP_VIDEO, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem files_file_context[] = {
    { "Open", FILES_OPEN, 0, NULL, NULL },
    { "Open With", 0, 0, NULL, files_with_menu },
    { "-", 0, 0, NULL, NULL },
    { "Rename...", FILES_RENAME, 0, NULL, NULL },
    { "Delete", FILES_DELETE, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Copy", FILES_COPY, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem files_folder_context[] = {
    { "Open", FILES_OPEN, 0, NULL, NULL },
    { "Terminal Here", FILES_TERMINAL, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Rename...", FILES_RENAME, 0, NULL, NULL },
    { "Delete", FILES_DELETE, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Copy", FILES_COPY, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem files_space_context[] = {
    { "New Folder", FILES_NEW_FOLDER, 0, NULL, NULL },
    { "Paste", FILES_PASTE, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Refresh", FILES_REFRESH, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

typedef struct {
    Window *win;
    int w, h;                       /* the client area */
    char dir[128];                  /* the folder shown, from the Project's root: "" for the root */
    DirEntry entries[FILES_MAX];
    int count, selected, top;
    ButtonTracker buttons;
    Rect button_rects[FILES_BUTTONS];
    ScrollBar scroll;
    int pressed, press_x, press_y;  /* a file pressed that may be dragged out: its index, or -1 */
    char note[160];                 /* what the status line says after an action */
} FileBrowser;

/* ------------------------------------------------------------------ the layout */

static Rect files_list_rect(const FileBrowser *f) {
    return rect(2, FILES_BAR_H, f->w - 4, f->h - FILES_BAR_H - FILES_STATUS_H - 2);
}
/* The rows' part of the list: under the column headings, left of the scroll bar. */
static Rect files_rows_rect(const FileBrowser *f) {
    Rect list = files_list_rect(f);
    return rect(list.x + 2, list.y + 2 + FILES_ROW_H, list.w - 4 - FILES_SCROLL_W, list.h - 4 - FILES_ROW_H);
}
static int files_rows(const FileBrowser *f) { return max_int(1, files_rows_rect(f).h / FILES_ROW_H); }
static int files_show_type(const FileBrowser *f) { return files_rows_rect(f).w >= 320; }
static int files_name_w(const FileBrowser *f) {
    return files_rows_rect(f).w - FILES_SIZE_W - (files_show_type(f) ? FILES_TYPE_W : 0);
}

/* The row at a point of the client area: an entry's index, or -1 for none. */
static int files_row_at(const FileBrowser *f, int x, int y) {
    Rect rows = files_rows_rect(f);
    if (!rect_contains(rows, x, y)) return -1;
    int index = f->top + (y - rows.y) / FILES_ROW_H;
    return index < f->count ? index : -1;
}

static void files_join(char *path, int size, const char *folder, const char *name) {
    snprintf(path, size, "%s%s%s", folder, folder[0] ? "/" : "", name);
}

static void files_warn(FileBrowser *f, const char *text) {
    os_message_box(f->win, "Files", text, MB_OK, MB_WARNING);
}

static void files_entry_path(const FileBrowser *f, int index, char *path, int size) {
    files_join(path, size, f->dir, f->entries[index].name);
}

/* The folder a path is in: "" for the root. */
static void files_parent(const char *path, char *parent, int size) {
    const char *slash = strrchr(path, '/');
    snprintf(parent, size, "%.*s", slash ? (int)(slash - path) : 0, path);
}

static const Icon *files_icon(const DirEntry *entry) {
    if (entry->is_dir) return &ICON_FOLDER;
    int app = os_app_for_file(entry->name);
    if (app == APP_VIEWER) return &ICON_PICTURE;
    if (app == APP_MUSIC) return &ICON_MUSIC;
    if (app == APP_VIDEO) return &ICON_VIDEO;
    if (app == APP_IDE) return &ICON_IDE;
    if (ends_with(entry->name, ".c") || ends_with(entry->name, ".h") || ends_with(entry->name, ".asm") ||
        ends_with(entry->name, ".s"))
        return &ICON_CHIP;
    return app == APP_NOTEPAD ? &ICON_NOTEPAD : &ICON_FILE;
}

static const char *files_type(const DirEntry *entry) {
    static const char *const types[][2] = {
        { ".txt", "Text" }, { ".md", "Markdown" }, { ".c", "C source" }, { ".h", "C header" },
        { ".asm", "Assembly" }, { ".s", "Assembly" }, { ".riscv", "RISC-V program" }, { ".bmp", "Picture" }, { ".mid", "MIDI music" },
        { ".midi", "MIDI music" }, { ".json", "JSON" }, { ".csv", "CSV table" }, { ".ini", "Settings" },
        { ".cfg", "Settings" }, { ".log", "Log" }, { ".wav", "Sound" },
        { ".wad", "DOOM data" }, { ".vid", "Video" },
    };
    if (entry->is_dir) return "Folder";
    for (int i = 0; i < (int)(sizeof types / sizeof types[0]); i++)
        if (ends_with(entry->name, types[i][0])) return types[i][1];
    return "File";
}

static void files_size_text(char *text, int size, long bytes) {
    if (bytes < 10000) snprintf(text, size, "%ld B", bytes);
    else if (bytes < 10000L * 1024) snprintf(text, size, "%ld KB", (bytes + 1023) / 1024);
    else snprintf(text, size, "%ld MB", (bytes + 1024L * 1024 - 1) / (1024L * 1024));
}

/* ------------------------------------------------------------------ drawing */

static void files_draw_toolbar(FileBrowser *f) {
    fill_rect(0, 0, f->w, FILES_BAR_H, FACE);
    static const char *const labels[FILES_BUTTONS] = { "Up", "Refresh" };
    for (int i = 0; i < FILES_BUTTONS; i++) {
        Rect r = f->button_rects[i];
        draw_button(r.x, r.y, r.w, r.h, labels[i], button_down(&f->buttons, i));
    }
    int x = 124, w = f->w - x - 4;
    draw_field(x, 4, w, 22, WHITE);
    char where[160];
    snprintf(where, sizeof where, "/project%s%s", f->dir[0] ? "/" : "", f->dir);
    int fits = max_int(1, (w - 12) / FONT_WIDTH), length = (int)strlen(where);
    draw_icon(&ICON_FOLDER, x + 3, 7, 1);
    gfx_clip(rect(x + 2, 6, w - 4, 18));
    draw_text(x + 22, 9, where + max_int(0, length - (fits - 3)), BLACK);
    gfx_clip(rect(0, 0, f->w, f->h));
}

static void files_draw_row(FileBrowser *f, int row) {
    Rect rows = files_rows_rect(f);
    int index = f->top + row, y = rows.y + row * FILES_ROW_H;
    fill_rect(rows.x, y, rows.w, FILES_ROW_H, WHITE);
    if (index >= f->count) return;
    const DirEntry *entry = &f->entries[index];
    int chosen = index == f->selected, focused = os_window_focused(f->win);
    uint32_t ink = chosen && focused ? WHITE : BLACK;
    if (chosen) fill_rect(rows.x, y, rows.w, FILES_ROW_H, focused ? os_theme_colour(THEME_SELECTION) : LIGHT);
    int name_w = files_name_w(f);
    draw_icon(files_icon(entry), rows.x + 3, y + 1, 1);
    gfx_clip(rect(rows.x, y, name_w - 4, FILES_ROW_H));
    draw_text(rows.x + 24, y + 3, entry->name, ink);
    gfx_clip(rect(rows.x, y, rows.w, FILES_ROW_H));
    if (!entry->is_dir) {
        char size[24];
        files_size_text(size, sizeof size, entry->size);
        draw_text(rows.x + name_w + FILES_SIZE_W - 8 - text_width(size), y + 3, size, ink);
    }
    if (files_show_type(f)) draw_text(rows.x + name_w + FILES_SIZE_W + 6, y + 3, files_type(entry), ink);
    gfx_clip(rect(0, 0, f->w, f->h));
}

static void files_draw_list(FileBrowser *f) {
    Rect list = files_list_rect(f), rows = files_rows_rect(f);
    draw_field(list.x, list.y, list.w, list.h, WHITE);
    /* the column headings */
    int name_w = files_name_w(f), y = list.y + 2;
    gfx_clip(rect(rows.x, y, rows.w, FILES_ROW_H));
    draw_button(rows.x, y, name_w, FILES_ROW_H, "", 0);
    draw_button(rows.x + name_w, y, FILES_SIZE_W, FILES_ROW_H, "", 0);
    draw_text(rows.x + 6, y + 3, "Name", BLACK);
    draw_text(rows.x + name_w + FILES_SIZE_W - 8 - text_width("Size"), y + 3, "Size", BLACK);
    if (files_show_type(f)) {
        draw_button(rows.x + name_w + FILES_SIZE_W, y, rows.w - name_w - FILES_SIZE_W, FILES_ROW_H, "", 0);
        draw_text(rows.x + name_w + FILES_SIZE_W + 6, y + 3, "Type", BLACK);
    }
    gfx_clip(rect(rows.x, rows.y, rows.w, rows.h));
    for (int row = 0; row < files_rows(f); row++) files_draw_row(f, row);
    if (!f->count) draw_text(rows.x + 24, rows.y + 4, "This folder is empty.", SHADOW);
    gfx_clip(rect(0, 0, f->w, f->h));
    fill_rect(rows.x + rows.w, y, FILES_SCROLL_W, FILES_ROW_H, FACE);
    f->scroll.r = rect(rows.x + rows.w, rows.y, FILES_SCROLL_W, rows.h);
    f->scroll.total = f->count;
    f->scroll.visible = files_rows(f);
    f->scroll.top = f->top;
    draw_scroll_bar(&f->scroll);
}

static void files_draw_status(FileBrowser *f) {
    int y = f->h - FILES_STATUS_H;
    fill_rect(0, y, f->w, FILES_STATUS_H, FACE);
    char count[32], info[128];
    snprintf(count, sizeof count, "%d item%s", f->count, f->count == 1 ? "" : "s");
    if (f->note[0]) {
        copy_text(info, sizeof info, f->note);
    } else if (f->selected >= 0) {
        const DirEntry *entry = &f->entries[f->selected];
        char size[24];
        files_size_text(size, sizeof size, entry->size);
        if (entry->is_dir) snprintf(info, sizeof info, "%.60s: folder", entry->name);
        else snprintf(info, sizeof info, "%.60s: %s (%ld bytes)", entry->name, size, entry->size);
    } else {
        info[0] = 0;
    }
    bevel_thin_sunken(2, y + 3, 90, FILES_STATUS_H - 5);
    bevel_thin_sunken(96, y + 3, f->w - 98, FILES_STATUS_H - 5);
    draw_text(8, y + 5, count, BLACK);
    gfx_clip(rect(98, y + 4, f->w - 102, FILES_STATUS_H - 6));
    draw_text(102, y + 5, info, BLACK);
    gfx_clip(rect(0, 0, f->w, f->h));
}

static void files_draw_all(FileBrowser *f) {
    os_window_draw(f->win);
    files_draw_toolbar(f);
    files_draw_list(f);
    files_draw_status(f);
    os_window_update_all(f->win);
}

/* The list and the status line, after the selection or the scrolling changed. */
static void files_draw_contents(FileBrowser *f) {
    os_window_draw(f->win);
    files_draw_list(f);
    files_draw_status(f);
    os_window_update(f->win, 0, FILES_BAR_H, f->w, f->h - FILES_BAR_H);
}

/* ------------------------------------------------------------------ the folder shown */

static void files_note(FileBrowser *f, const char *format, const char *name) {
    snprintf(f->note, sizeof f->note, format, name);
}

static void files_select(FileBrowser *f, int index) {
    f->selected = index;
    if (index >= 0) {
        if (index < f->top) f->top = index;
        if (index >= f->top + files_rows(f)) f->top = index - files_rows(f) + 1;
    }
    f->top = clamp_int(f->top, 0, max_int(0, f->count - files_rows(f)));
}

/* Reads the folder again, selecting the entry called `name` if there is one. */
static void files_read(FileBrowser *f, const char *name) {
    f->note[0] = 0;
    f->pressed = -1;
    f->count = os_list_dir(f->dir, f->entries, FILES_MAX);
    if (f->count < 0) {
        f->count = 0;
        files_note(f, "Cannot read %.100s", f->dir);
    }
    int index = -1;
    for (int i = 0; name && i < f->count && index < 0; i++)
        if (!strcmp(f->entries[i].name, name)) index = i;
    files_select(f, index);
}

/* Reads the folder again, keeping the selection on the same name, or at the same place. */
static void files_refresh(FileBrowser *f) {
    char name[64] = "";
    int was = f->selected;
    if (was >= 0) copy_text(name, sizeof name, f->entries[was].name);
    files_read(f, name);
    if (f->selected < 0 && was >= 0 && f->count) files_select(f, min_int(was, f->count - 1));
}

static void files_go(FileBrowser *f, const char *dir, const char *select) {
    copy_text(f->dir, sizeof f->dir, dir);
    f->top = 0;
    f->note[0] = 0;
    files_read(f, select);
    char title[160];
    snprintf(title, sizeof title, "Files - /project%s%s", dir[0] ? "/" : "", dir);
    os_window_set_title(f->win, title);
    files_draw_all(f);
}

static void files_up(FileBrowser *f) {
    if (!f->dir[0]) return;
    char parent[128], came_from[64];
    copy_text(came_from, sizeof came_from, os_base_name(f->dir));
    files_parent(f->dir, parent, sizeof parent);
    files_go(f, parent, came_from);
}

static void files_open(FileBrowser *f, int index, int app) {
    if (index < 0) return;
    char path[200];
    files_entry_path(f, index, path, sizeof path);
    if (f->entries[index].is_dir && app < 0) {
        files_go(f, path, NULL);
        return;
    }
    if (app < 0) app = os_app_for_file(path);
    if (app < 0 || os_spawn(app, path) < 0) files_warn(f, "The file could not be opened.");
}

/* ------------------------------------------------------------------ changing files */

/* Deletes a file, or a folder with everything in it; answers 0 when all went. */
static int files_remove_tree(const char *path, int depth) {
    if (os_stat(path, NULL) != 2) return os_delete(path);
    char inner[256];
    if (depth < FILES_DEPTH) {
        DirEntry *entries = os_alloc(FILES_MAX * sizeof *entries);
        int count = entries ? os_list_dir(path, entries, FILES_MAX) : 0;
        for (int i = 0; i < count; i++) {
            files_join(inner, sizeof inner, path, entries[i].name);
            files_remove_tree(inner, depth + 1);
        }
        os_free(entries);
    }
    return os_delete(path);
}

/* Moves (`copy` 0) or copies a file, or a folder with everything in it, to a path that names
 * nothing yet; answers 0 when all of it went. os_rename moves files only, so a folder goes a file
 * at a time into a new one. */
static int files_transfer(const char *from, const char *to, int copy, int depth) {
    if (os_stat(from, NULL) != 2) return copy ? os_copy_file(from, to) : os_rename(from, to);
    if (!copy && os_rename(from, to) == 0) return 0;
    if (depth >= FILES_DEPTH) return -1;
    char inner_from[256], inner_to[256];
    DirEntry *entries = os_alloc(FILES_MAX * sizeof *entries);
    int count = entries ? os_list_dir(from, entries, FILES_MAX) : 0, failed = 0;
    failed |= os_make_dir(to) != 0;
    for (int i = 0; i < count; i++) {
        files_join(inner_from, sizeof inner_from, from, entries[i].name);
        files_join(inner_to, sizeof inner_to, to, entries[i].name);
        failed |= files_transfer(inner_from, inner_to, copy, depth + 1) != 0;
    }
    os_free(entries);
    if (copy) return failed ? -1 : 0;
    return os_delete(from);
}

/* A name typed for a file or folder: one that a listing shows and a path can hold. */
static int files_valid_name(FileBrowser *f, const char *name) {
    if (name[0] && name[0] != '.' && !strchr(name, '/') && strlen(name) < 60) return 1;
    files_warn(f, "A name cannot be empty, start with a dot or hold a slash.");
    return 0;
}

/* `name` in the folder `folder`, or "name 2.ext" and so on while that is taken. */
static void files_free_name(const char *folder, const char *name, char *path, int size) {
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name) dot = name + strlen(name);
    files_join(path, size, folder, name);
    for (int number = 2; number < 1000 && os_stat(path, NULL); number++) {
        char numbered[96];
        snprintf(numbered, sizeof numbered, "%.*s %d%s", (int)(dot - name), name, number, dot);
        files_join(path, size, folder, numbered);
    }
}

/* Copies a file or folder into the folder `into`, under a name not taken there, and shows it. */
static void files_copy_into(FileBrowser *f, const char *from, const char *into) {
    char to[200], from_parent[128];
    int kind = os_stat(from, NULL);
    size_t length = strlen(from);
    files_parent(from, from_parent, sizeof from_parent);
    if (!kind) {
        files_warn(f, "That file is not there any more.");
        return;
    }
    if (kind == 2 && (!strcmp(into, from) || (!strncmp(into, from, length) && into[length] == '/'))) {
        files_warn(f, "A folder cannot be copied into itself.");
        return;
    }
    files_free_name(into, os_base_name(from), to, sizeof to);
    if (files_transfer(from, to, 1, 0) != 0)
        files_warn(f, "The copy could not be written.");
    if (!strcmp(into, f->dir)) {
        files_read(f, os_base_name(to));
        files_note(f, "Copied as %.60s", os_base_name(to));
    } else {
        files_refresh(f);
        files_note(f, "Copied into %.60s", os_base_name(into));
    }
    files_draw_all(f);
}

static void files_paste(FileBrowser *f) {
    int length;
    char *text = os_clipboard_text(&length);
    if (text && !strncmp(text, "file:", 5) && text[5]) files_copy_into(f, text + 5, f->dir);
    else os_message_box(f->win, "Files", "The clipboard holds no file. Copy one first.", MB_OK, MB_INFO);
    os_free(text);
}

static void files_new_folder(FileBrowser *f) {
    char path[200], name[64];
    files_free_name(f->dir, "New Folder", path, sizeof path);
    copy_text(name, sizeof name, os_base_name(path));
    if (!os_input_box(f->win, "New Folder", "Name of the new folder:", name, sizeof name)) return;
    if (!files_valid_name(f, name)) return;
    files_join(path, sizeof path, f->dir, name);
    if (os_stat(path, NULL)) files_warn(f, "There is something with that name already.");
    else if (os_make_dir(path) != 0) files_warn(f, "The folder could not be made.");
    files_read(f, name);
    files_draw_all(f);
}

static void files_rename(FileBrowser *f) {
    if (f->selected < 0) return;
    char from[200], to[200], name[64], prompt[96];
    copy_text(name, sizeof name, f->entries[f->selected].name);
    files_entry_path(f, f->selected, from, sizeof from);
    snprintf(prompt, sizeof prompt, "New name for %.60s:", name);
    if (!os_input_box(f->win, "Rename", prompt, name, sizeof name)) return;
    if (!strcmp(name, f->entries[f->selected].name) || !files_valid_name(f, name)) return;
    files_join(to, sizeof to, f->dir, name);
    if (os_stat(to, NULL)) files_warn(f, "There is something with that name already.");
    else if (files_transfer(from, to, 0, 0) != 0) files_warn(f, "It could not be renamed.");
    files_read(f, name);
    files_draw_all(f);
}

static void files_delete(FileBrowser *f) {
    if (f->selected < 0) return;
    const DirEntry *entry = &f->entries[f->selected];
    char path[200], question[160];
    files_entry_path(f, f->selected, path, sizeof path);
    snprintf(question, sizeof question, entry->is_dir ? "Delete the folder %.60s and everything in it?"
                                                      : "Delete %.60s?", entry->name);
    if (os_message_box(f->win, "Delete", question, MB_YES_NO, MB_QUESTION) != ID_YES) return;
    if (files_remove_tree(path, 0) != 0) files_warn(f, "It could not be deleted.");
    files_refresh(f);
    files_draw_all(f);
}

static void files_copy(FileBrowser *f) {
    if (f->selected < 0) return;
    char text[220];
    const char *name = f->entries[f->selected].name;
    int length = snprintf(text, sizeof text, "file:%s%s%s", f->dir, f->dir[0] ? "/" : "", name);
    os_clipboard_set_text(text, length);
    files_note(f, "Copied %.60s: Paste puts a copy in the folder shown", f->entries[f->selected].name);
    files_draw_contents(f);
}

/* ------------------------------------------------------------------ commands, keys and the pointer */

static void files_command(FileBrowser *f, int id) {
    if (id >= FILES_WITH && id < FILES_WITH + APP_COUNT) {
        files_open(f, f->selected, id - FILES_WITH);
        return;
    }
    switch (id) {
    case FILES_OPEN: files_open(f, f->selected, -1); break;
    case FILES_NEW_FOLDER: files_new_folder(f); break;
    case FILES_RENAME: files_rename(f); break;
    case FILES_DELETE: files_delete(f); break;
    case FILES_TERMINAL: {
        char path[200];
        if (f->selected >= 0 && f->entries[f->selected].is_dir)
            files_entry_path(f, f->selected, path, sizeof path);
        else
            copy_text(path, sizeof path, f->dir);
        os_spawn(APP_TERMINAL, path[0] ? path : NULL);
        break;
    }
    case FILES_CLOSE: os_exit(0);
    case FILES_COPY: files_copy(f); break;
    case FILES_PASTE: files_paste(f); break;
    case FILES_REFRESH:
        files_refresh(f);
        f->note[0] = 0;
        files_draw_all(f);
        break;
    case FILES_UP: files_up(f); break;
    }
}

static void files_key(FileBrowser *f, const Message *m) {
    if (m->mods & MOD_CTRL) {
        if (m->key == KEY_C) files_command(f, FILES_COPY);
        else if (m->key == KEY_V) files_command(f, FILES_PASTE);
        return;
    }
    int index = f->selected, rows = files_rows(f);
    switch (m->key) {
    case KEY_UP: index = max_int(0, index - 1); break;
    case KEY_DOWN: index = index + 1; break;
    case KEY_PAGEUP: index = max_int(0, index - (rows - 1)); break;
    case KEY_PAGEDOWN: index = index + rows - 1; break;
    case KEY_HOME: index = 0; break;
    case KEY_END: index = f->count - 1; break;
    case KEY_ENTER: case KEY_KPENTER: files_command(f, FILES_OPEN); return;
    case KEY_BACKSPACE: files_up(f); return;
    case KEY_F5: files_command(f, FILES_REFRESH); return;
    case KEY_F2: files_command(f, FILES_RENAME); return;
    case KEY_DELETE: files_command(f, FILES_DELETE); return;
    default:
        /* a letter goes to the next name that begins with it */
        if (!isalnum(m->ch) || !f->count) return;
        for (int step = 1; step <= f->count; step++) {
            int at = (max_int(f->selected, -1) + step) % f->count;
            if (tolower((unsigned char)f->entries[at].name[0]) == tolower(m->ch)) {
                index = at;
                break;
            }
        }
    }
    if (!f->count) return;
    files_select(f, clamp_int(index, 0, f->count - 1));
    f->note[0] = 0;
    files_draw_contents(f);
}

static void files_mouse(FileBrowser *f, const Message *m) {
    int changed, clicked = track_buttons(&f->buttons, f->button_rects, FILES_BUTTONS, m, &changed);
    if (changed) {
        os_window_draw(f->win);
        files_draw_toolbar(f);
        os_window_update(f->win, 0, 0, f->w, FILES_BAR_H);
    }
    if (clicked == FILES_BUTTON_UP) files_up(f);
    if (clicked == FILES_BUTTON_REFRESH) files_command(f, FILES_REFRESH);
    if (clicked >= 0 || f->buttons.pressed >= 0) return;
    if (scroll_bar_message(&f->scroll, m)) {
        f->top = f->scroll.top;
        files_draw_contents(f);
        return;
    }
    if (m->type == MSG_MOUSE_WHEEL) {
        f->top = clamp_int(f->top - m->wheel * 3, 0, max_int(0, f->count - files_rows(f)));
        files_draw_contents(f);
        return;
    }
    if (m->type == MSG_MOUSE_UP) f->pressed = -1;
    if (m->type == MSG_MOUSE_MOVE && f->pressed >= 0 && m->buttons & BUTTON_LEFT &&
        abs(m->x - f->press_x) + abs(m->y - f->press_y) > 4) {
        /* a file pressed and moved: dragged out of the list */
        char path[200];
        files_entry_path(f, f->pressed, path, sizeof path);
        f->pressed = -1;
        os_drag_file(path);
        return;
    }
    if (m->type != MSG_MOUSE_DOWN || !rect_contains(files_list_rect(f), m->x, m->y)) return;
    int index = files_row_at(f, m->x, m->y);
    files_select(f, index);
    f->note[0] = 0;
    files_draw_contents(f);
    if (m->button == BUTTON_RIGHT) {
        const MenuItem *menu = index < 0 ? files_space_context
                               : f->entries[index].is_dir ? files_folder_context : files_file_context;
        int id = os_popup_menu(f->win, m->x, m->y, menu);
        if (id) files_command(f, id);
    } else if (m->button == BUTTON_LEFT && index >= 0) {
        if (m->clicks == 2) {
            f->pressed = -1;
            files_open(f, index, -1);
        } else if (!f->entries[index].is_dir) {
            f->pressed = index;
            f->press_x = m->x;
            f->press_y = m->y;
        }
    }
}

/* A file dropped on the window is copied into the folder shown, or into the folder it was
 * dropped on; one dropped back where it came from stays as it is. */
static void files_drop(FileBrowser *f, const Message *m) {
    char into[200], parent[128];
    int index = files_row_at(f, m->x, m->y);
    if (index >= 0 && f->entries[index].is_dir) files_entry_path(f, index, into, sizeof into);
    else copy_text(into, sizeof into, f->dir);
    files_parent(m->text, parent, sizeof parent);
    if (!strcmp(parent, into) || !strcmp(m->text, into)) return;
    files_copy_into(f, m->text, into);
}

static void files_relayout(FileBrowser *f) {
    os_window_pixels(f->win, &f->w, &f->h);
    f->button_rects[FILES_BUTTON_UP] = rect(4, 4, 44, 22);
    f->button_rects[FILES_BUTTON_REFRESH] = rect(52, 4, 66, 22);
    files_select(f, f->selected);
    files_draw_all(f);
}

void files_main(const char *argument) {
    FileBrowser *f = os_alloc(sizeof *f);
    if (!f) os_exit(1);
    f->win = os_window_create("Files", &ICON_FOLDER, 440, 300, WIN_RESIZABLE);
    if (!f->win) os_exit(1);
    os_window_set_menu(f->win, files_menus);
    os_window_min_size(f->win, 260, 160);
    f->buttons.pressed = f->pressed = f->selected = -1;
    os_window_pixels(f->win, &f->w, &f->h);
    f->button_rects[FILES_BUTTON_UP] = rect(4, 4, 44, 22);
    f->button_rects[FILES_BUTTON_REFRESH] = rect(52, 4, 66, 22);
    /* the argument is a folder to show, or a file to show selected in its folder */
    char start[128] = "", select[64] = "";
    if (argument && argument[0]) {
        const char *path = argument;
        if (!strncmp(path, "/project", 8) && (path[8] == '/' || !path[8])) path += 8;
        while (*path == '/' || (path[0] == '.' && path[1] == '/')) path += *path == '/' ? 1 : 2;
        copy_text(start, sizeof start, path);
        size_t length = strlen(start);
        while (length > 0 && start[length - 1] == '/') start[--length] = 0;
        int kind = start[0] ? os_stat(start, NULL) : 2;
        if (kind == 1) {
            copy_text(select, sizeof select, os_base_name(start));
            char parent[128];
            files_parent(start, parent, sizeof parent);
            copy_text(start, sizeof start, parent);
        } else if (kind != 2) {
            start[0] = 0;
        }
    }
    files_go(f, start, select[0] ? select : NULL);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_KEY: files_key(f, &m); break;
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: case MSG_MOUSE_WHEEL:
            files_mouse(f, &m);
            break;
        case MSG_COMMAND: files_command(f, m.id); break;
        case MSG_RESIZE: files_relayout(f); break;
        case MSG_FOCUS:
            /* what other applications changed meanwhile shows */
            files_refresh(f);
            files_draw_all(f);
            break;
        case MSG_BLUR: case MSG_SETTINGS: files_draw_contents(f); break;
        case MSG_DROP: files_drop(f, &m); break;
        default: os_default_message(&m);
        }
    }
}
