/*
 * The system's dialogs: the message box, the input box, the Open and Save As dialog, and the Run
 * box of the Start menu. Each is a window of the system's, drawn and driven by the system process,
 * which a process asks for with an os_ call and then waits on: its other windows take no input
 * until the dialog answers. A text answer, such as a chosen path, is written into the process's
 * buffer before it is woken.
 */

enum { DIALOG_MESSAGE, DIALOG_INPUT, DIALOG_FILE, DIALOG_RUN };
enum { MAX_LINES = 12, MAX_ENTRIES = 128, LIST_ROW = 18 };

typedef struct {
    Rect r;
    const char *label;
    int answer;
} DialogButton;

typedef struct {
    int kind;
    int pid;                  /* the process waiting; 0 for the Run box */
    char *answer_text;        /* where a text answer goes, in that process's memory */
    int answer_size;
    char lines[MAX_LINES][64];
    int line_count;
    int icon;
    DialogButton buttons[3];
    int button_count;
    int pressed;              /* the button held down, or -1 */
    char text[128];           /* the input field's, or the file name's */
    int caret;
    /* the file dialog */
    int save;
    char dir[128];
    char extension[12];
    DirEntry entries[MAX_ENTRIES];
    int entry_count, selected, top;
} Dialog;

static Window *dialog_create(int pid, const char *title, int w, int h, void (*paint)(Window *),
                             void (*on_mouse)(Window *, const Message *),
                             void (*on_key)(Window *, const Message *)) {
    Window *win = window_new(0, title, NULL, w, h, WIN_CENTER | WIN_NO_MINIMIZE);
    if (!win) return NULL;
    win->modal_pid = pid;
    win->paint = paint;
    win->on_mouse = on_mouse;
    win->on_key = on_key;
    win->data = calloc(1, sizeof(Dialog));
    if (!win->data) {
        window_destroy(win);
        return NULL;
    }
    return win;
}

/* Draws a system window into its picture and shows it. */
static void dialog_repaint(Window *win) {
    GfxContext saved = gfx_save();
    gfx_target(win->pixels, win->pw, win->ph, win->pw);
    fill_rect(0, 0, win->pw, win->ph, FACE);
    win->paint(win);
    gfx_restore(saved);
    invalidate(client_rect(win));
}

/* Gives the waiting process its answer, and its text, then closes the dialog. */
static void dialog_finish(Window *win, int answer) {
    Dialog *dialog = win->data;
    if (dialog->answer_text && answer == ID_OK)
        snprintf(dialog->answer_text, dialog->answer_size, "%s", dialog->text);
    int pid = dialog->pid;
    window_destroy(win);
    if (pid > 0) answer_process(pid, answer);
}

static void draw_dialog_buttons(Dialog *dialog) {
    for (int i = 0; i < dialog->button_count; i++) {
        DialogButton *button = &dialog->buttons[i];
        draw_button(button->r.x, button->r.y, button->r.w, button->r.h, button->label, dialog->pressed == i);
        if (i == 0) frame_rect(button->r.x - 1, button->r.y - 1, button->r.w + 2, button->r.h + 2, BLACK);
    }
}

/* A press on a button and its release on the same one: its answer, or 0. */
static int dialog_buttons_mouse(Window *win, const Message *message) {
    Dialog *dialog = win->data;
    int at = -1;
    for (int i = 0; i < dialog->button_count; i++)
        if (rect_contains(dialog->buttons[i].r, message->x, message->y)) at = i;
    if (message->type == MSG_MOUSE_DOWN && message->button == BUTTON_LEFT && at >= 0) {
        dialog->pressed = at;
        dialog_repaint(win);
    } else if (message->type == MSG_MOUSE_UP && dialog->pressed >= 0) {
        int clicked = at == dialog->pressed ? dialog->buttons[at].answer : 0;
        dialog->pressed = -1;
        if (clicked) return clicked;
        dialog_repaint(win);
    }
    return 0;
}

static void layout_buttons(Dialog *dialog, int width, int y, const char *const *labels, const int *answers,
                           int count) {
    int button_w = 72, gap = 8, x = (width - count * button_w - (count - 1) * gap) / 2;
    for (int i = 0; i < count; i++) {
        dialog->buttons[i].r = rect(x + i * (button_w + gap), y, button_w, 23);
        dialog->buttons[i].label = labels[i];
        dialog->buttons[i].answer = answers[i];
    }
    dialog->button_count = count;
    dialog->pressed = -1;
}

/* ---- typing in a field: the dialog's text, its caret */

static int field_key(Dialog *dialog, const Message *message) {
    int length = (int)strlen(dialog->text);
    if (message->key == KEY_BACKSPACE && dialog->caret > 0) {
        memmove(dialog->text + dialog->caret - 1, dialog->text + dialog->caret, length - dialog->caret + 1);
        dialog->caret--;
    } else if (message->key == KEY_DELETE && dialog->caret < length) {
        memmove(dialog->text + dialog->caret, dialog->text + dialog->caret + 1, length - dialog->caret);
    } else if (message->key == KEY_LEFT && dialog->caret > 0) {
        dialog->caret--;
    } else if (message->key == KEY_RIGHT && dialog->caret < length) {
        dialog->caret++;
    } else if (message->key == KEY_HOME) {
        dialog->caret = 0;
    } else if (message->key == KEY_END) {
        dialog->caret = length;
    } else if (message->ch >= 32 && message->ch < 127 && length < (int)sizeof dialog->text - 1) {
        memmove(dialog->text + dialog->caret + 1, dialog->text + dialog->caret, length - dialog->caret + 1);
        dialog->text[dialog->caret++] = (char)message->ch;
    } else {
        return 0;
    }
    return 1;
}

static void draw_text_field(int x, int y, int w, Dialog *dialog) {
    draw_field(x, y, w, 22, WHITE);
    int visible = (w - 10) / FONT_W, skip = max_int(0, dialog->caret - visible);
    draw_text_n(x + 5, y + 5, dialog->text + skip, visible, BLACK);
    fill_rect(x + 5 + (dialog->caret - skip) * FONT_W, y + 4, 1, 15, BLACK);
}

/* ------------------------------------------------------------------ the message box */

static void message_paint(Window *win) {
    Dialog *dialog = win->data;
    const Icon *icon = dialog->icon == MB_WARNING ? &ICON_WARNING : dialog->icon == MB_QUESTION ? &ICON_QUESTION
                                                                                                  : &ICON_INFO;
    draw_icon(icon, 14, 14, 2);
    for (int i = 0; i < dialog->line_count; i++) draw_text(60, 18 + i * 16, dialog->lines[i], BLACK);
    draw_dialog_buttons(dialog);
}

static void message_mouse(Window *win, const Message *message) {
    int answer = dialog_buttons_mouse(win, message);
    if (answer) dialog_finish(win, answer);
}

static void message_key(Window *win, const Message *message) {
    Dialog *dialog = win->data;
    if (message->key == KEY_ENTER || message->key == KEY_SPACE)
        dialog_finish(win, dialog->buttons[0].answer);
    else if (message->key == KEY_ESC)
        dialog_finish(win, dialog->buttons[dialog->button_count - 1].answer);
}

/* Breaks text into lines of at most `width` characters, at spaces where it can. */
static int wrap_text(const char *text, char lines[][64], int max_lines, int width) {
    int count = 0;
    while (*text && count < max_lines) {
        int length = 0, last_space = -1;
        while (text[length] && text[length] != '\n' && length < width) {
            if (text[length] == ' ') last_space = length;
            length++;
        }
        int cut = length;
        if (text[length] && text[length] != '\n' && last_space > 0) cut = last_space;
        snprintf(lines[count++], 64, "%.*s", cut, text);
        text += cut;
        while (*text == ' ') text++;
        if (*text == '\n') text++;
    }
    return count;
}

static Window *message_box_open(int pid, const char *title, const char *text, int buttons, int kind) {
    char lines[MAX_LINES][64];
    int count = wrap_text(text, lines, MAX_LINES, 44), widest = 0;
    for (int i = 0; i < count; i++) widest = max_int(widest, text_width(lines[i]));
    static const char *const labels[4][3] = {
        { "OK" }, { "OK", "Cancel" }, { "Yes", "No" }, { "Yes", "No", "Cancel" }
    };
    static const int answers[4][3] = {
        { ID_OK }, { ID_OK, ID_CANCEL }, { ID_YES, ID_NO }, { ID_YES, ID_NO, ID_CANCEL }
    };
    static const int counts[4] = { 1, 2, 2, 3 };
    int w = max_int(widest + 80, counts[buttons] * 80 + 40), h = max_int(count * 16, 36) + 70;
    Window *win = dialog_create(pid, title, w, h, message_paint, message_mouse, message_key);
    if (!win) return NULL;
    Dialog *dialog = win->data;
    dialog->kind = DIALOG_MESSAGE;
    dialog->pid = pid;
    dialog->icon = kind;
    memcpy(dialog->lines, lines, sizeof lines);
    dialog->line_count = count;
    layout_buttons(dialog, w, h - 34, labels[buttons], answers[buttons], counts[buttons]);
    dialog_repaint(win);
    return win;
}

int os_message_box(Window *owner, const char *title, const char *text, int buttons, int kind) {
    (void)owner;
    lock();
    if (!message_box_open(current->pid, title, text, clamp_int(buttons, 0, 3), kind)) {
        unlock();
        return ID_CANCEL;
    }
    if (current == SYSTEM) {
        /* the system cannot wait for itself: its message boxes only tell */
        kernel_lock--;
        return ID_OK;
    }
    if (os_settings()->sounds && kind != MB_QUESTION) os_beep();
    return wait_for_answer();
}

/* ------------------------------------------------------------------ the input box and the Run box */

static void input_paint(Window *win) {
    Dialog *dialog = win->data;
    if (dialog->kind == DIALOG_RUN) draw_icon(&ICON_CHIP, 12, 12, 2);
    int left = dialog->kind == DIALOG_RUN ? 56 : 14;
    for (int i = 0; i < dialog->line_count; i++) draw_text(left, 14 + i * 15, dialog->lines[i], BLACK);
    draw_text_field(left, 22 + dialog->line_count * 15, win->pw - left - 14, dialog);
    draw_dialog_buttons(dialog);
}

static void run_command(const char *text);

static void input_answer(Window *win, int answer) {
    Dialog *dialog = win->data;
    if (dialog->kind == DIALOG_RUN) {
        char command[128];
        snprintf(command, sizeof command, "%s", dialog->text);
        window_destroy(win);
        if (answer == ID_OK && command[0]) run_command(command);
        return;
    }
    dialog_finish(win, answer);
}

static void input_mouse(Window *win, const Message *message) {
    int answer = dialog_buttons_mouse(win, message);
    if (answer) input_answer(win, answer);
}

static void input_key(Window *win, const Message *message) {
    if (message->key == KEY_ENTER) input_answer(win, ID_OK);
    else if (message->key == KEY_ESC) input_answer(win, ID_CANCEL);
    else if (field_key(win->data, message)) dialog_repaint(win);
}

static Window *input_box_open(int pid, int kind, const char *title, const char *prompt, const char *text) {
    char lines[MAX_LINES][64];
    int count = wrap_text(prompt, lines, 4, 40);
    int w = kind == DIALOG_RUN ? 380 : 340, h = 70 + count * 15 + 30;
    Window *win = dialog_create(pid, title, w, h, input_paint, input_mouse, input_key);
    if (!win) return NULL;
    Dialog *dialog = win->data;
    dialog->kind = kind;
    dialog->pid = pid;
    memcpy(dialog->lines, lines, sizeof lines);
    dialog->line_count = count;
    snprintf(dialog->text, sizeof dialog->text, "%s", text);
    dialog->caret = (int)strlen(dialog->text);
    static const char *const labels[2] = { "OK", "Cancel" };
    static const int answers[2] = { ID_OK, ID_CANCEL };
    layout_buttons(dialog, w, h - 32, labels, answers, 2);
    dialog_repaint(win);
    return win;
}

int os_input_box(Window *owner, const char *title, const char *prompt, char *text, int size) {
    (void)owner;
    lock();
    Window *win = input_box_open(current->pid, DIALOG_INPUT, title, prompt, text);
    if (!win || current == SYSTEM) {
        unlock();
        return 0;
    }
    Dialog *dialog = win->data;
    dialog->answer_text = text;
    dialog->answer_size = size;
    return wait_for_answer() == ID_OK;
}

void dialog_run(void) {
    input_box_open(0, DIALOG_RUN, "Run", "Type the name of a program, or of a file in the Project, and asmOS will open it.",
               "");
}

/* What the Run box opens: an application by name, or a file with its application. */
static void run_command(const char *text) {
    int app = os_app_named(text);
    if (app < 0)
        for (int i = 0; i < APP_COUNT && app < 0; i++) {
            const char *a = text, *b = APPS[i].name;
            while (*a && *b && (*a | 32) == (*b | 32)) a++, b++;
            if (!*a && !*b) app = i;
        }
    if (app >= 0) {
        os_spawn(app, NULL);
        return;
    }
    int kind = os_stat(text, NULL);
    if (kind == 1) {
        os_spawn(os_app_for_file(text), text);
    } else if (kind == 2) {
        os_spawn(APP_FILES, text);
    } else {
        char message[160];
        snprintf(message, sizeof message, "Cannot find '%.100s'. Check the name and try again.", text);
        message_box_open(0, "Run", message, MB_OK, MB_WARNING);
    }
}

/* ------------------------------------------------------------------ the Open and Save As dialog */

enum { FILE_W = 400, FILE_H = 290 };

static Rect file_list_rect(void) { return rect(10, 38, FILE_W - 20, 172); }
static Rect file_up_button(void) { return rect(FILE_W - 72, 8, 62, 22); }
static int file_rows(void) { return (file_list_rect().h - 4) / LIST_ROW; }

static int matches_extension(Dialog *dialog, const DirEntry *entry) {
    if (entry->is_dir || !dialog->extension[0]) return 1;
    return has_extension(entry->name, dialog->extension);
}

static void file_read_dir(Dialog *dialog) {
    DirEntry all[MAX_ENTRIES];
    int count = os_list_dir(dialog->dir, all, MAX_ENTRIES);
    dialog->entry_count = 0;
    for (int i = 0; i < count; i++)
        if (matches_extension(dialog, &all[i])) dialog->entries[dialog->entry_count++] = all[i];
    dialog->selected = -1;
    dialog->top = 0;
}

static void file_paint(Window *win) {
    Dialog *dialog = win->data;
    draw_text(10, 13, "Look in:", BLACK);
    draw_field(80, 8, FILE_W - 160, 22, WHITE);
    char where[160];
    snprintf(where, sizeof where, "/project%s%s", dialog->dir[0] ? "/" : "", dialog->dir);
    draw_icon(&ICON_FOLDER, 85, 11, 1);
    draw_text(105, 13, where, BLACK);
    Rect up = file_up_button();
    draw_button(up.x, up.y, up.w, up.h, "Up", 0);
    Rect list = file_list_rect();
    draw_field(list.x, list.y, list.w, list.h, WHITE);
    GfxContext saved = gfx_save();
    gfx_clip(rect(list.x + 2, list.y + 2, list.w - 4, list.h - 4));
    for (int row = 0; row < file_rows() && dialog->top + row < dialog->entry_count; row++) {
        int index = dialog->top + row;
        DirEntry *entry = &dialog->entries[index];
        int y = list.y + 2 + row * LIST_ROW;
        draw_icon(file_icon(entry->name, entry->is_dir), list.x + 6, y + 1, 1);
        int chosen = index == dialog->selected;
        if (chosen) fill_rect(list.x + 26, y + 1, text_width(entry->name) + 4, LIST_ROW - 2, os_theme_colour(THEME_SELECTION));
        draw_text(list.x + 28, y + 3, entry->name, chosen ? WHITE : BLACK);
        if (!entry->is_dir) {
            char size[24];
            snprintf(size, sizeof size, entry->size < 10000 ? "%ld B" : "%ld KB",
                     entry->size < 10000 ? entry->size : (entry->size + 1023) / 1024);
            draw_text(list.x + list.w - 10 - text_width(size), y + 3, size, SHADOW);
        }
    }
    if (!dialog->entry_count) draw_text(list.x + 28, list.y + 6, "(empty)", SHADOW);
    gfx_restore(saved);
    draw_text(10, FILE_H - 66, "File name:", BLACK);
    draw_text_field(96, FILE_H - 71, FILE_W - 106, dialog);
    draw_dialog_buttons(dialog);
}

static void file_enter(Window *win, const char *name) {
    Dialog *dialog = win->data;
    if (!strcmp(name, "..")) {
        char *slash = strrchr(dialog->dir, '/');
        if (slash) *slash = 0;
        else dialog->dir[0] = 0;
    } else {
        char dir[256];
        snprintf(dir, sizeof dir, "%s%s%s", dialog->dir, dialog->dir[0] ? "/" : "", name);
        snprintf(dialog->dir, sizeof dialog->dir, "%.127s", dir);
    }
    file_read_dir(dialog);
    dialog_repaint(win);
}

/* OK: a folder's name goes into it; a file's is the answer, checked as an Open or a Save needs. */
static void file_accept(Window *win) {
    Dialog *dialog = win->data;
    if (!dialog->text[0]) {
        os_beep();
        return;
    }
    char path[300];
    snprintf(path, sizeof path, "%s%s%s", dialog->dir, dialog->dir[0] ? "/" : "", dialog->text);
    int kind = os_stat(path, NULL);
    if (kind == 2) {
        dialog->text[0] = 0;
        dialog->caret = 0;
        file_enter(win, os_base_name(path));
        return;
    }
    if (dialog->save && dialog->extension[0] && !strchr(dialog->text, '.') &&
        strlen(path) + strlen(dialog->extension) < sizeof path)
        strcat(path, dialog->extension);
    if (!dialog->save && kind != 1) {
        os_beep();
        return;
    }
    snprintf(dialog->text, sizeof dialog->text, "%.127s", path);
    dialog_finish(win, ID_OK);
}

static void file_mouse(Window *win, const Message *message) {
    Dialog *dialog = win->data;
    int answer = dialog_buttons_mouse(win, message);
    if (answer == ID_OK) {
        file_accept(win);
        return;
    }
    if (answer) {
        dialog_finish(win, answer);
        return;
    }
    Rect list = file_list_rect();
    if (message->type == MSG_MOUSE_WHEEL && rect_contains(list, message->x, message->y)) {
        dialog->top = clamp_int(dialog->top - message->wheel * 3, 0, max_int(0, dialog->entry_count - file_rows()));
        dialog_repaint(win);
        return;
    }
    if (message->type != MSG_MOUSE_DOWN || message->button != BUTTON_LEFT) return;
    if (rect_contains(file_up_button(), message->x, message->y)) {
        file_enter(win, "..");
        return;
    }
    if (!rect_contains(list, message->x, message->y)) return;
    int index = dialog->top + (message->y - list.y - 2) / LIST_ROW;
    if (index >= dialog->entry_count) return;
    DirEntry *entry = &dialog->entries[index];
    dialog->selected = index;
    if (!entry->is_dir) {
        snprintf(dialog->text, sizeof dialog->text, "%s", entry->name);
        dialog->caret = (int)strlen(dialog->text);
    }
    if (message->clicks == 2) {
        if (entry->is_dir) file_enter(win, entry->name);
        else file_accept(win);
        return;
    }
    dialog_repaint(win);
}

static void file_key(Window *win, const Message *message) {
    Dialog *dialog = win->data;
    if (message->key == KEY_ENTER) {
        file_accept(win);
    } else if (message->key == KEY_ESC) {
        dialog_finish(win, ID_CANCEL);
    } else if ((message->key == KEY_UP || message->key == KEY_DOWN) && dialog->entry_count) {
        int index = clamp_int(dialog->selected + (message->key == KEY_UP ? -1 : 1), 0, dialog->entry_count - 1);
        dialog->selected = index;
        if (index < dialog->top) dialog->top = index;
        if (index >= dialog->top + file_rows()) dialog->top = index - file_rows() + 1;
        snprintf(dialog->text, sizeof dialog->text, "%s", dialog->entries[index].name);
        dialog->caret = (int)strlen(dialog->text);
        dialog_repaint(win);
    } else if (message->key == KEY_BACKSPACE && !dialog->text[0]) {
        file_enter(win, "..");
    } else if (field_key(dialog, message)) {
        dialog_repaint(win);
    }
}

int os_file_dialog(Window *owner, int save, const char *title, const char *extension, char *path, int size) {
    (void)owner;
    lock();
    Window *win = current == SYSTEM ? NULL
                                    : dialog_create(current->pid, title, FILE_W, FILE_H, file_paint, file_mouse, file_key);
    if (!win) {
        unlock();
        return 0;
    }
    Dialog *dialog = win->data;
    dialog->kind = DIALOG_FILE;
    dialog->pid = current->pid;
    dialog->save = save;
    dialog->answer_text = path;
    dialog->answer_size = size;
    snprintf(dialog->extension, sizeof dialog->extension, "%s", extension ? extension : "");
    /* start in the folder of the path given, with its name in the field */
    if (path && path[0]) {
        if (os_stat(path, NULL) == 2) {
            snprintf(dialog->dir, sizeof dialog->dir, "%s", path);
        } else {
            const char *slash = strrchr(path, '/');
            if (slash) snprintf(dialog->dir, sizeof dialog->dir, "%.*s", (int)(slash - path), path);
            snprintf(dialog->text, sizeof dialog->text, "%s", os_base_name(path));
        }
    }
    dialog->caret = (int)strlen(dialog->text);
    file_read_dir(dialog);
    static const char *const open_labels[2] = { "Open", "Cancel" }, *const save_labels[2] = { "Save", "Cancel" };
    static const int answers[2] = { ID_OK, ID_CANCEL };
    layout_buttons(dialog, FILE_W, FILE_H - 36, save ? save_labels : open_labels, answers, 2);
    dialog->buttons[0].r.x = FILE_W - 2 * 80 - 10;
    dialog->buttons[1].r.x = FILE_W - 80 - 10;
    dialog_repaint(win);
    return wait_for_answer() == ID_OK;
}
