/*
 * Terminal: a small shell over the Project. It lists, prints, copies and writes files, starts
 * applications and lists or ends processes, and works out arithmetic; `help` lists the commands.
 * Paths are relative to the current folder, which the prompt shows, and /project is the Project's
 * root. After `> FILE` a command's output goes into the file, after `>> FILE` onto its end, and
 * `A | B` gives A's output to B, which reads it in place of a file.
 *
 * What was written is kept as a ring of lines, wrapped at the window's width only when drawn, so
 * the columns follow the window as it is resized.
 */
#include <stdarg.h>

enum { TERM_LINES = 500, TERM_LINE_MAX = 200, TERM_INPUT = 200, TERM_HISTORY = 32, TERM_WORDS = 24,
       TERM_DIR_MAX = 256, TERM_DEPTH = 10 };
enum { TERM_LINE_H = 14, TERM_PAD = 4, TERM_SCROLL_W = 16 };
enum { TERM_COPY = 1, TERM_PASTE, TERM_CLEAR, TERM_HELP, TERM_ABOUT };
enum { TERM_BLINK = 1 };
enum { TERM_TEXT = 0xc0c0c0, TERM_WHITE = 0xffffff, TERM_GREEN = 0x40e040, TERM_YELLOW = 0xf0e060,
       TERM_BLUE = 0x6090ff, TERM_RED = 0xff6060, TERM_CYAN = 0x60d0d0 };

static MenuItem term_edit_menu[] = {
    { "Copy", TERM_COPY, 0, NULL, NULL },
    { "Paste", TERM_PASTE, 0, "Ctrl+V", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Clear", TERM_CLEAR, 0, "Ctrl+L", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem term_help_menu[] = {
    { "Commands", TERM_HELP, 0, NULL, NULL },
    { "About Terminal", TERM_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const Menu term_menus[] = {
    { "Edit", term_edit_menu }, { "Help", term_help_menu }, { NULL, NULL },
};
static const MenuItem term_context_menu[] = {
    { "Copy", TERM_COPY, 0, NULL, NULL },
    { "Paste", TERM_PASTE, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Clear", TERM_CLEAR, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

/* A line written: its first `split` characters in `head`, the rest in `colour`, as the prompt of
 * a command typed, or the folders at the start of a row of ls. */
typedef struct {
    char text[TERM_LINE_MAX + 1];
    int length, split;
    uint32_t head, colour;
} TermLine;

typedef struct {
    Window *win;
    int w, h;                       /* the client area */
    TermLine lines[TERM_LINES];     /* a ring of the lines written */
    int first, count;
    char input[TERM_INPUT + 1];     /* the line being typed, and the caret in it */
    int length, caret;
    char history[TERM_HISTORY][TERM_INPUT + 1];
    int history_count, history_at;
    char draft[TERM_INPUT + 1];     /* what was typed before Up went back in the history */
    char cwd[128];                  /* the current folder from the Project's root, "" for the root */
    int back;                       /* rows scrolled back from the end */
    int drawn_total;                /* the rows there were at the last whole drawing */
    int blink;
    ScrollBar scroll;
    char *capture;                  /* a command's output on its way into a file or a pipe, or NULL */
    int capture_length, capture_size, capture_failed;
    const char *piped;              /* the output of the command before |, or NULL */
    int piped_length;
} Terminal;

static const char *const TERM_FORTUNES[] = {
    "Real programmers count from zero.",
    "There are 10 kinds of people: those who read binary and those who don't.",
    "The best code is no code at all.",
    "Premature optimization is the root of all evil. -- Knuth",
    "It works on my machine. This is the machine.",
    "mov eax, 1  ; the loneliest number",
    "Every window here is a process. Try ps, then kill one.",
    "A rep stosd fills a whole row of pixels in one instruction.",
};
static const char *const TERM_DAYS[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *const TERM_MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                           "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char *const TERM_STATES[] = { "ready", "running", "waiting", "sleeping", "ended" };

/* ------------------------------------------------------------------ writing */

/* Adds a line to the ring, the oldest going when it is full. Tabs become spaces and characters
 * the font has no picture for become dots. */
static void term_add_line(Terminal *t, const char *text, int length, int split, uint32_t head,
                          uint32_t colour) {
    int slot = (t->first + t->count) % TERM_LINES;
    if (t->count == TERM_LINES) t->first = (t->first + 1) % TERM_LINES;
    else t->count++;
    TermLine *line = &t->lines[slot];
    int kept = 0;
    for (int i = 0; i < length && kept < TERM_LINE_MAX; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\r') continue;
        if (c == '\t') {
            do line->text[kept++] = ' ';
            while (kept % 4 && kept < TERM_LINE_MAX);
        } else {
            line->text[kept++] = c < 32 || c > 126 ? '.' : (char)c;
        }
    }
    line->text[kept] = 0;
    line->length = kept;
    line->split = min_int(split, kept);
    line->head = head;
    line->colour = colour;
}

/* Puts bytes on the end of the output going into a file. */
static void term_capture_bytes(Terminal *t, const char *bytes, int count) {
    if (t->capture_length + count + 1 > t->capture_size) {
        int size = max_int(t->capture_size * 2, t->capture_length + count + 256);
        char *grown = os_realloc(t->capture, size);
        if (!grown) {
            t->capture_failed = 1;
            return;
        }
        t->capture = grown;
        t->capture_size = size;
    }
    memcpy(t->capture + t->capture_length, bytes, count);
    t->capture_length += count;
}

/* Writes text, a line for each of its lines, with its first `split` characters in `head`: onto
 * the screen, or into the file the output goes to. */
static void term_write_split(Terminal *t, int split, uint32_t head, uint32_t colour, const char *text) {
    os_yield_point();       /* a command writes as it goes: a long one gets here often */
    if (t->capture) {
        term_capture_bytes(t, text, (int)strlen(text));
        term_capture_bytes(t, "\n", 1);
        return;
    }
    for (;;) {
        const char *end = strchr(text, '\n');
        int length = end ? (int)(end - text) : (int)strlen(text);
        while (length > TERM_LINE_MAX) {
            term_add_line(t, text, TERM_LINE_MAX, split, head, colour);
            text += TERM_LINE_MAX;
            length -= TERM_LINE_MAX;
            split = 0;
        }
        term_add_line(t, text, length, split, head, colour);
        if (!end) break;
        text = end + 1;
        split = 0;
        os_yield_point();
    }
}

static void term_write(Terminal *t, uint32_t colour, const char *text) {
    term_write_split(t, 0, colour, colour, text);
}

static void term_printf(Terminal *t, uint32_t colour, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
static void term_printf(Terminal *t, uint32_t colour, const char *format, ...) {
    char text[512];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof text, format, arguments);
    va_end(arguments);
    term_write(t, colour, text);
}

/* An error goes onto the screen in red, even while the output goes into a file. */
static void term_fail(Terminal *t, const char *format, ...) __attribute__((format(printf, 2, 3)));
static void term_fail(Terminal *t, const char *format, ...) {
    char text[512];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof text, format, arguments);
    va_end(arguments);
    char *capture = t->capture;
    t->capture = NULL;
    term_write(t, TERM_RED, text);
    t->capture = capture;
}

/* ------------------------------------------------------------------ the layout */

static int term_columns(const Terminal *t) {
    return max_int(8, (t->w - TERM_SCROLL_W - 2 * TERM_PAD) / FONT_WIDTH);
}
static int term_rows(const Terminal *t) { return max_int(1, (t->h - 2 * TERM_PAD) / TERM_LINE_H); }
static int term_line_rows(const Terminal *t, int length) {
    return max_int(1, (length + term_columns(t) - 1) / term_columns(t));
}

static int term_prompt(const Terminal *t, char *prompt, int size) {
    return snprintf(prompt, size, "/project%s%s$ ", t->cwd[0] ? "/" : "", t->cwd);
}

/* The prompt with what is typed takes a cell more for the caret at its end. */
static int term_prompt_rows(const Terminal *t) {
    char prompt[160];
    return term_line_rows(t, term_prompt(t, prompt, sizeof prompt) + t->length + 1);
}

static int term_total_rows(const Terminal *t) {
    int total = term_prompt_rows(t);
    for (int i = 0; i < t->count; i++) total += term_line_rows(t, t->lines[(t->first + i) % TERM_LINES].length);
    return total;
}

static int term_most_back(const Terminal *t, int total) { return max_int(0, total - term_rows(t)); }

/* The first row shown: the rows end with the prompt's, `back` rows above the end. */
static int term_top(const Terminal *t, int total) {
    return max_int(0, total - term_rows(t) - clamp_int(t->back, 0, term_most_back(t, total)));
}

/* ------------------------------------------------------------------ drawing */

/* Draws characters `from` to `to` of a line that begins at row `row`, wrapping at the width. */
static void term_draw_part(const Terminal *t, int row, int top, const char *text, int from, int to,
                           uint32_t colour) {
    int columns = term_columns(t), rows = term_rows(t);
    while (from < to) {
        int r = row + from / columns, column = from % columns;
        int count = min_int(to - from, columns - column);
        int x = TERM_PAD + column * FONT_WIDTH, y = TERM_PAD + (r - top) * TERM_LINE_H;
        if (r >= top && r < top + rows) draw_text_n(x, y, text + from, count, colour);
        from += count;
    }
}

static void term_draw_prompt_line(const Terminal *t, int row, int top) {
    char line[TERM_INPUT + 168];
    int prompt = term_prompt(t, line, sizeof line - TERM_INPUT - 1);
    memcpy(line + prompt, t->input, t->length + 1);
    term_draw_part(t, row, top, line, 0, prompt, TERM_GREEN);
    term_draw_part(t, row, top, line, prompt, prompt + t->length, TERM_WHITE);
    if (!t->blink || !os_window_focused(t->win)) return;
    int at = prompt + t->caret, r = row + at / term_columns(t);
    if (r < top || r >= top + term_rows(t)) return;
    int x = TERM_PAD + at % term_columns(t) * FONT_WIDTH, y = TERM_PAD + (r - top) * TERM_LINE_H;
    fill_rect(x, y, FONT_WIDTH, TERM_LINE_H, TERM_TEXT);
    if (t->caret < t->length) draw_char(x, y, (unsigned char)t->input[t->caret], BLACK);
}

static void term_draw_scroll(Terminal *t, int total, int top) {
    t->scroll.r = rect(t->w - TERM_SCROLL_W, 0, TERM_SCROLL_W, t->h);
    t->scroll.total = total;
    t->scroll.visible = term_rows(t);
    t->scroll.top = top;
    draw_scroll_bar(&t->scroll);
}

static void term_draw_all(Terminal *t) {
    os_window_draw(t->win);
    int total = term_total_rows(t), top = term_top(t, total), rows = term_rows(t), row = 0;
    t->back = total - rows - top > 0 ? total - rows - top : 0;
    t->drawn_total = total;
    fill_rect(0, 0, t->w - TERM_SCROLL_W, t->h, BLACK);
    gfx_clip(rect(0, 0, t->w - TERM_SCROLL_W, t->h));
    for (int i = 0; i < t->count && row < top + rows; i++) {
        const TermLine *line = &t->lines[(t->first + i) % TERM_LINES];
        int count = term_line_rows(t, line->length);
        if (row + count > top) {
            term_draw_part(t, row, top, line->text, 0, line->split, line->head);
            term_draw_part(t, row, top, line->text, line->split, line->length, line->colour);
        }
        row += count;
    }
    term_draw_prompt_line(t, row, top);
    gfx_clip(rect(0, 0, t->w, t->h));
    term_draw_scroll(t, total, top);
    os_window_update_all(t->win);
}

/* Draws the prompt's rows again alone, as typing and the caret's blink need, unless the rows
 * changed meanwhile and everything moves. */
static void term_draw_prompt(Terminal *t) {
    int total = term_total_rows(t);
    if (total != t->drawn_total) {
        term_draw_all(t);
        return;
    }
    int top = term_top(t, total), rows = term_rows(t), row = total - term_prompt_rows(t);
    int from = max_int(row, top), to = min_int(total, top + rows);
    if (from >= to) return;
    os_window_draw(t->win);
    int y = TERM_PAD + (from - top) * TERM_LINE_H, h = (to - from) * TERM_LINE_H;
    fill_rect(0, y, t->w - TERM_SCROLL_W, h, BLACK);
    gfx_clip(rect(0, y, t->w - TERM_SCROLL_W, h));
    term_draw_prompt_line(t, row, top);
    gfx_clip(rect(0, 0, t->w, t->h));
    os_window_update(t->win, 0, y, t->w - TERM_SCROLL_W, h);
}

static void term_scroll_by(Terminal *t, int rows) {
    int total = term_total_rows(t);
    int back = clamp_int(t->back + rows, 0, term_most_back(t, total));
    if (back == t->back) return;
    t->back = back;
    term_draw_all(t);
}

/* ------------------------------------------------------------------ paths */

/* The path from the Project's root that `name` means from the current folder, "" for the root
 * itself: a name that starts with / starts at the root (and so does /project), and . and .. are
 * the folder and its parent. */
static void term_resolve(const Terminal *t, const char *name, char *path, int size) {
    char work[256];
    work[0] = 0;
    if (name[0] == '/') {
        name++;
        if (strncmp(name, "project", 7) == 0 && (name[7] == '/' || name[7] == 0)) name += 7;
    } else if (name[0] == '~' && (name[1] == '/' || name[1] == 0)) {
        name++;
    } else {
        copy_text(work, sizeof work, t->cwd);
    }
    int length = (int)strlen(work);
    while (*name) {
        while (*name == '/') name++;
        const char *end = name;
        while (*end && *end != '/') end++;
        int count = (int)(end - name);
        if (count == 2 && name[0] == '.' && name[1] == '.') {
            char *slash = strrchr(work, '/');
            length = slash ? (int)(slash - work) : 0;
            work[length] = 0;
        } else if (count > 0 && !(count == 1 && name[0] == '.') && length + count + 2 < (int)sizeof work) {
            if (length) work[length++] = '/';
            memcpy(work + length, name, count);
            length += count;
            work[length] = 0;
        }
        name = end;
    }
    copy_text(path, size, work);
}

/* What the os_ calls take for a path from the root: "." for the root itself. */
static const char *term_fs(const char *path) { return path[0] ? path : "."; }

static void term_join(char *path, int size, const char *folder, const char *name) {
    snprintf(path, size, "%s%s%s", folder, folder[0] ? "/" : "", name);
}

/* A file's path from the root, or 0 after saying why there is no such file. */
static int term_file(Terminal *t, const char *command, const char *name, char *path, int size) {
    term_resolve(t, name, path, size);
    int kind = os_stat(term_fs(path), NULL);
    if (kind == 1) return 1;
    term_fail(t, "%s: %s: %s", command, name, kind == 2 ? "is a folder" : "no such file");
    return 0;
}

/* A file's bytes, or with no file named (NULL) the output of the command before |, followed by a
 * 0: memory to os_free, or NULL after saying why not. */
static char *term_read_input(Terminal *t, const char *command, const char *name, long *size) {
    char path[160];
    if (!name) {
        char *copy = os_alloc(t->piped_length + 1);
        if (!copy) {
            term_fail(t, "%s: out of memory", command);
            return NULL;
        }
        memcpy(copy, t->piped, t->piped_length);
        copy[t->piped_length] = 0;
        *size = t->piped_length;
        return copy;
    }
    if (!term_file(t, command, name, path, sizeof path)) return NULL;
    char *data = os_read_file(path, size);
    if (!data) term_fail(t, "%s: cannot read %s", command, name);
    return data;
}

/* The file named, or with none the output of the command before |, or NULL after saying that a
 * file is missing. */
static int term_input_name(Terminal *t, int argc, char **argv, int index, const char **name) {
    *name = index < argc ? argv[index] : NULL;
    if (*name || t->piped) return 1;
    term_fail(t, "%s: which file?", argv[0]);
    return 0;
}

/* Reads a text file, or what came through |, and cuts it into lines, each ended by a 0 in place of
 * its newline. Answers how many, with the memory to os_free in `data` and `lines`, or -1 after
 * saying why not. */
static int term_read_lines(Terminal *t, const char *command, const char *name, char **data, char ***lines) {
    long size;
    *data = term_read_input(t, command, name, &size);
    if (!*data) return -1;
    int count = 0;
    for (const char *at = *data, *end = *data + size; (at = memchr(at, '\n', end - at)); at++) {
        count++;
        os_yield_point();
    }
    count += size > 0 && (*data)[size - 1] != '\n';
    *lines = os_alloc((count + 1) * sizeof **lines);
    if (!*lines) {
        os_free(*data);
        return -1;
    }
    char *at = *data;
    for (int i = 0; i < count; i++) {
        (*lines)[i] = at;
        char *end = strchr(at, '\n');
        if (!end) end = at + strlen(at);
        if (end > at && end[-1] == '\r') end[-1] = 0;
        *end = 0;
        at = end + 1;
        os_yield_point();
    }
    return count;
}

static void term_free_lines(char *data, char **lines) {
    os_free(lines);
    os_free(data);
}

/* The number of an option such as -n 5 or -5, by default `otherwise`; `index` moves past it. */
static int term_count_option(int argc, char **argv, int *index, int otherwise) {
    if (*index < argc && !strcmp(argv[*index], "-n") && *index + 1 < argc) {
        *index += 2;
        return max_int(0, atoi(argv[*index - 1]));
    }
    if (*index < argc && argv[*index][0] == '-' && isdigit((unsigned char)argv[*index][1]))
        return max_int(0, atoi(argv[(*index)++] + 1));
    return otherwise;
}

static void term_size_text(char *text, int size, long bytes) {
    if (bytes < 10000) snprintf(text, size, "%ld B", bytes);
    else if (bytes < 10000L * 1024) snprintf(text, size, "%ld KB", (bytes + 1023) / 1024);
    else snprintf(text, size, "%ld MB", (bytes + 1024L * 1024 - 1) / (1024L * 1024));
}

/* ------------------------------------------------------------------ calc: arithmetic */

typedef struct {
    const char *at;
    int error;
} TermParser;

static double term_sum(TermParser *p);
static double term_unary(TermParser *p);

static void term_space(TermParser *p) {
    while (*p->at == ' ') p->at++;
}

/* A number (decimal, 0x hexadecimal or 0b binary), a constant, a function of a value in
 * brackets, or a sum in brackets. */
static double term_atom(TermParser *p) {
    term_space(p);
    const char *at = p->at;
    if (*at == '(') {
        p->at++;
        double value = term_sum(p);
        term_space(p);
        if (*p->at == ')') p->at++;
        else p->error = 1;
        return value;
    }
    if (at[0] == '0' && (at[1] == 'x' || at[1] == 'X' || at[1] == 'b' || at[1] == 'B')) {
        char *end;
        double value = (double)strtoll(at + 2, &end, at[1] == 'x' || at[1] == 'X' ? 16 : 2);
        if (end == at + 2) p->error = 1;
        p->at = end;
        return value;
    }
    if (isdigit((unsigned char)*at) || *at == '.') {
        char *end;
        double value = strtod(at, &end);
        if (end == at) p->error = 1;
        p->at = end;
        return value;
    }
    char name[8];
    int length = 0;
    while (isalpha((unsigned char)p->at[length]) && length < 7) name[length] = p->at[length], length++;
    name[length] = 0;
    p->at += length;
    if (!strcmp(name, "pi")) return 3.14159265358979323846;
    if (!strcmp(name, "e")) return 2.71828182845904523536;
    static const char *const functions[] = { "sqrt", "sin", "cos", "tan", "abs", "ln", "log", "exp", "floor",
                                             "round" };
    for (int i = 0; i < (int)(sizeof functions / sizeof functions[0]); i++) {
        if (strcmp(name, functions[i])) continue;
        double x = term_atom(p);
        switch (i) {
        case 0: return sqrt(x);
        case 1: return sin(x);
        case 2: return cos(x);
        case 3: return tan(x);
        case 4: return fabs(x);
        case 5: return log(x);
        case 6: return log10(x);
        case 7: return exp(x);
        case 8: return floor(x);
        default: return round(x);
        }
    }
    p->error = 1;
    return 0;
}

/* A power binds tighter than a sign before it, and to the right: -2^2 is -4, 2^3^2 is 512. */
static double term_power(TermParser *p) {
    double value = term_atom(p);
    term_space(p);
    if (*p->at != '^') return value;
    p->at++;
    return pow(value, term_unary(p));
}

static double term_unary(TermParser *p) {
    term_space(p);
    if (*p->at == '-') {
        p->at++;
        return -term_unary(p);
    }
    if (*p->at == '+') p->at++;
    return term_power(p);
}

static double term_product(TermParser *p) {
    double value = term_unary(p);
    for (;;) {
        term_space(p);
        char op = *p->at;
        if (op != '*' && op != '/' && op != '%') return value;
        p->at++;
        double right = term_unary(p);
        value = op == '*' ? value * right : op == '/' ? value / right : fmod(value, right);
    }
}

static double term_sum(TermParser *p) {
    double value = term_product(p);
    for (;;) {
        term_space(p);
        char op = *p->at;
        if (op != '+' && op != '-') return value;
        p->at++;
        double right = term_product(p);
        value = op == '+' ? value + right : value - right;
    }
}

/* ------------------------------------------------------------------ the commands */

/* Whether this machine's file system can do what a command needs (FS_ of os.h): not every one
 * lists folders, makes them, deletes or renames. If not, the command says so and does nothing. */
static int term_can(Terminal *t, int feature, const char *command, const char *what) {
    if (os_file_system() & feature) return 1;
    term_fail(t, "%s: this machine cannot %s", command, what);
    return 0;
}

static void term_clear(Terminal *t) {
    t->count = 0;
    t->first = 0;
    t->back = 0;
}

static void term_cmd_ls(Terminal *t, int argc, char **argv) {
    int long_form = 0;
    const char *name = "";
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') long_form |= strchr(argv[i], 'l') != NULL;
        else name = argv[i];
    }
    char path[160];
    long size;
    term_resolve(t, name, path, sizeof path);
    int kind = os_stat(term_fs(path), &size);
    if (kind == 0) {
        term_fail(t, "ls: %s: no such file or folder", name);
        return;
    }
    if (kind == 1) {
        if (long_form) term_printf(t, TERM_TEXT, "%10ld  %s", size, name);
        else term_write(t, TERM_TEXT, name);
        return;
    }
    if (!term_can(t, FS_LIST, "ls", "list a folder")) return;
    DirEntry *entries = os_alloc(TERM_DIR_MAX * sizeof *entries);
    int count = entries ? os_list_dir(path, entries, TERM_DIR_MAX) : -1;
    if (count < 0) {
        term_fail(t, "ls: cannot read %s", name[0] ? name : ".");
    } else if (long_form || t->capture) {
        long bytes = 0;
        int folders = 0;
        for (int i = 0; i < count; i++) {
            DirEntry *entry = &entries[i];
            if (t->capture) term_write(t, TERM_TEXT, entry->name);
            else if (entry->is_dir) term_printf(t, TERM_BLUE, "     <DIR>  %s/", entry->name);
            else term_printf(t, TERM_TEXT, "%10ld  %s", entry->size, entry->name);
            bytes += entry->size;
            folders += entry->is_dir;
        }
        if (!t->capture) {
            char total[24];
            term_size_text(total, sizeof total, bytes);
            int files = count - folders;
            term_printf(t, TERM_YELLOW, "%d file%s, %d folder%s, %s", files, files == 1 ? "" : "s", folders,
                        folders == 1 ? "" : "s", total);
        }
    } else {
        /* in columns, as wide as the longest name; folders come first, in blue */
        int widest = 1;
        for (int i = 0; i < count; i++)
            widest = max_int(widest, (int)strlen(entries[i].name) + entries[i].is_dir);
        int width = widest + 2, across = max_int(1, term_columns(t) / width);
        for (int i = 0; i < count; i += across) {
            char row[TERM_LINE_MAX + 1];
            int length = 0, split = 0;
            for (int k = i; k < i + across && k < count && length < TERM_LINE_MAX - width; k++) {
                const char *slash = entries[k].is_dir ? "/" : "";
                length += snprintf(row + length, sizeof row - length, "%s%s", entries[k].name, slash);
                if (entries[k].is_dir) split = length;
                while (length < (k - i + 1) * width && length < TERM_LINE_MAX) row[length++] = ' ';
                row[length] = 0;
            }
            while (length > 0 && row[length - 1] == ' ') row[--length] = 0;
            term_write_split(t, split, TERM_BLUE, TERM_TEXT, row);
        }
    }
    os_free(entries);
}

static void term_cmd_cd(Terminal *t, int argc, char **argv) {
    char path[160];
    term_resolve(t, argc > 1 ? argv[1] : "/", path, sizeof path);
    if (os_stat(term_fs(path), NULL) != 2) {
        term_fail(t, "cd: %s: no such folder", argv[1]);
        return;
    }
    copy_text(t->cwd, sizeof t->cwd, path);
}

static void term_cmd_pwd(Terminal *t, int argc, char **argv) {
    (void)argc, (void)argv;
    term_printf(t, TERM_TEXT, "/project%s%s", t->cwd[0] ? "/" : "", t->cwd);
}

static void term_cmd_cat(Terminal *t, int argc, char **argv) {
    const char *name;
    if (!term_input_name(t, argc, argv, 1, &name)) return;
    for (int i = 1; i < argc || (i == 1 && !name); i++) {
        long size;
        char *data = term_read_input(t, "cat", name ? argv[i] : NULL, &size);
        if (!data) {
            continue;
        } else if (t->capture) {
            term_capture_bytes(t, data, (int)size);
        } else if (memchr(data, 0, min_int((int)size, 4096))) {
            if (name) term_fail(t, "cat: %s is not text (try hex %s)", argv[i], argv[i]);
            else term_fail(t, "cat: that is not text");
        } else {
            if (size > 0 && data[size - 1] == '\n') data[size - 1] = 0;
            term_write(t, TERM_TEXT, data);
        }
        os_free(data);
    }
}

static void term_cmd_echo(Terminal *t, int argc, char **argv) {
    char text[TERM_INPUT + 1];
    int length = 0;
    text[0] = 0;
    for (int i = 1; i < argc; i++)
        length += snprintf(text + length, sizeof text - length, "%s%s", i > 1 ? " " : "", argv[i]);
    term_write(t, TERM_TEXT, text);
}

/* Deletes a file, or a folder with everything in it; answers 0 when all went. */
static int term_remove_tree(const char *path, int depth) {
    if (os_stat(path, NULL) != 2) return os_delete(path);
    char inner[256];
    if (depth < TERM_DEPTH) {
        DirEntry *entries = os_alloc(TERM_DIR_MAX * sizeof *entries);
        int count = entries ? os_list_dir(path, entries, TERM_DIR_MAX) : 0;
        for (int i = 0; i < count; i++) {
            os_yield_point();
            term_join(inner, sizeof inner, path, entries[i].name);
            term_remove_tree(inner, depth + 1);
        }
        os_free(entries);
    }
    return os_delete(path);
}

/* Moves a file, or a folder with everything in it, to a path that names nothing yet. os_rename
 * moves files only, so a folder goes a file at a time into a new one. */
static int term_move_tree(const char *from, const char *to, int depth) {
    if (os_rename(from, to) == 0) return 0;
    if (os_stat(from, NULL) != 2 || depth >= TERM_DEPTH || os_make_dir(to) != 0) return -1;
    char inner_from[256], inner_to[256];
    DirEntry *entries = os_alloc(TERM_DIR_MAX * sizeof *entries);
    int count = entries ? os_list_dir(from, entries, TERM_DIR_MAX) : 0;
    for (int i = 0; i < count; i++) {
        os_yield_point();
        term_join(inner_from, sizeof inner_from, from, entries[i].name);
        term_join(inner_to, sizeof inner_to, to, entries[i].name);
        term_move_tree(inner_from, inner_to, depth + 1);
    }
    os_free(entries);
    return os_delete(from);
}

/* Where cp and mv put a file: `to` itself, or the file's name in it when it is a folder. */
static void term_target(Terminal *t, const char *from, const char *name, char *to, int size) {
    char path[160];
    term_resolve(t, name, path, sizeof path);
    if (os_stat(term_fs(path), NULL) == 2) term_join(to, size, path, os_base_name(from));
    else copy_text(to, size, path);
}

static void term_cmd_cp(Terminal *t, int argc, char **argv) {
    char from[160], to[200];
    if (argc != 3) {
        term_fail(t, "cp: usage: cp FROM TO");
        return;
    }
    if (!term_file(t, "cp", argv[1], from, sizeof from)) return;
    term_target(t, from, argv[2], to, sizeof to);
    if (!strcmp(from, to)) term_fail(t, "cp: %s and %s are the same file", argv[1], argv[2]);
    else if (os_copy_file(from, to) != 0) term_fail(t, "cp: cannot write %s", argv[2]);
}

static void term_cmd_mv(Terminal *t, int argc, char **argv) {
    char from[160], to[200];
    if (argc != 3) {
        term_fail(t, "mv: usage: mv FROM TO");
        return;
    }
    term_resolve(t, argv[1], from, sizeof from);
    if (!from[0] || !os_stat(from, NULL)) {
        term_fail(t, "mv: %s: no such file or folder", argv[1]);
        return;
    }
    if (!term_can(t, FS_RENAME, "mv", "rename files")) return;
    term_target(t, from, argv[2], to, sizeof to);
    size_t length = strlen(from);
    if (os_stat(to, NULL)) term_fail(t, "mv: %s is there already", to);
    else if (!strncmp(to, from, length) && to[length] == '/')
        term_fail(t, "mv: cannot move a folder into itself");
    else if (term_move_tree(from, to, 0) != 0) term_fail(t, "mv: cannot move %s to %s", argv[1], argv[2]);
}

static void term_cmd_rm(Terminal *t, int argc, char **argv) {
    if (!term_can(t, FS_DELETE, "rm", "delete files")) return;
    int recursive = 0, any = 0;
    for (int i = 1; i < argc; i++) recursive |= !strcmp(argv[i], "-r") || !strcmp(argv[i], "-rf");
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') continue;
        char path[160];
        any = 1;
        term_resolve(t, argv[i], path, sizeof path);
        int kind = path[0] ? os_stat(path, NULL) : 0;
        if (!kind) term_fail(t, "rm: %s: no such file", argv[i]);
        else if (kind == 2 && !recursive) term_fail(t, "rm: %s is a folder (rmdir, or rm -r)", argv[i]);
        else if (term_remove_tree(path, 0) != 0) term_fail(t, "rm: cannot remove %s", argv[i]);
    }
    if (!any) term_fail(t, "rm: which file?");
}

static void term_cmd_rmdir(Terminal *t, int argc, char **argv) {
    if (!term_can(t, FS_DELETE | FS_LIST, "rmdir", "delete folders")) return;
    if (argc < 2) term_fail(t, "rmdir: which folder?");
    for (int i = 1; i < argc; i++) {
        char path[160];
        term_resolve(t, argv[i], path, sizeof path);
        DirEntry entry;
        if (!path[0] || os_stat(path, NULL) != 2) term_fail(t, "rmdir: %s: no such folder", argv[i]);
        else if (os_list_dir(path, &entry, 1) > 0) term_fail(t, "rmdir: %s is not empty", argv[i]);
        else if (term_remove_tree(path, 0) != 0) term_fail(t, "rmdir: cannot remove %s", argv[i]);
    }
}

static void term_cmd_mkdir(Terminal *t, int argc, char **argv) {
    if (!term_can(t, FS_MAKE_DIR, "mkdir", "make folders")) return;
    if (argc < 2) term_fail(t, "mkdir: which folder?");
    for (int i = 1; i < argc; i++) {
        char path[160];
        term_resolve(t, argv[i], path, sizeof path);
        if (os_stat(term_fs(path), NULL)) term_fail(t, "mkdir: %s is there already", argv[i]);
        else if (os_make_dir(path) != 0) term_fail(t, "mkdir: cannot make %s", argv[i]);
    }
}

static void term_cmd_touch(Terminal *t, int argc, char **argv) {
    if (argc < 2) term_fail(t, "touch: which file?");
    for (int i = 1; i < argc; i++) {
        char path[160];
        term_resolve(t, argv[i], path, sizeof path);
        if (!os_stat(term_fs(path), NULL) && os_write_file(path, "", 0) != 0)
            term_fail(t, "touch: cannot make %s", argv[i]);
    }
}

static void term_started(Terminal *t, int app, int pid) {
    if (pid < 0) term_fail(t, "cannot start %s: too many processes", APPS[app].name);
    else term_printf(t, TERM_TEXT, "[%d] %s", pid, APPS[app].name);
}

static void term_cmd_edit(Terminal *t, int argc, char **argv) {
    char path[160];
    if (argc > 1) term_resolve(t, argv[1], path, sizeof path);
    term_started(t, APP_NOTEPAD, os_spawn(APP_NOTEPAD, argc > 1 ? path : NULL));
}

static void term_cmd_asm(Terminal *t, int argc, char **argv) {
    char path[160];
    if (argc > 1) term_resolve(t, argv[1], path, sizeof path);
    term_started(t, APP_IDE, os_spawn(APP_IDE, argc > 1 ? path : NULL));
}

/* An application by its command or its name, in any case. */
static int term_app_named(const char *name) {
    int app = os_app_named(name);
    for (int i = 0; i < APP_COUNT && app < 0; i++) {
        const char *a = name, *b = APPS[i].name;
        while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) a++, b++;
        if (!*a && !*b) app = i;
    }
    return app;
}

static void term_cmd_open(Terminal *t, int argc, char **argv) {
    if (argc < 2) {
        term_fail(t, "open: which application or file? (help lists them)");
        return;
    }
    char path[160];
    int app = term_app_named(argv[1]);
    if (app >= 0) {
        /* the argument is a file when there is one by that name */
        const char *argument = argc > 2 ? argv[2] : NULL;
        if (argument) {
            term_resolve(t, argument, path, sizeof path);
            if (os_stat(term_fs(path), NULL)) argument = path;
        }
        term_started(t, app, os_spawn(app, argument));
        return;
    }
    term_resolve(t, argv[1], path, sizeof path);
    int kind = os_stat(term_fs(path), NULL);
    if (kind == 2) app = APP_FILES;
    else if (kind == 1) app = os_app_for_file(path);
    if (app < 0) {
        term_fail(t, "open: no application or file called '%s'", argv[1]);
        return;
    }
    term_started(t, app, os_spawn(app, path));
}

static void term_cmd_ps(Terminal *t, int argc, char **argv) {
    (void)argc, (void)argv;
    ProcessInfo list[40];
    int count = min_int(os_processes(list, 40), 40), busy;
    long all;
    os_cpu_load(&busy, &all);
    term_write(t, TERM_YELLOW, "  PID  NAME             STATE      CPU  MEMORY");
    for (int i = 0; i < count; i++) {
        ProcessInfo *p = &list[i];
        long percent = all > 0 ? p->cpu * busy / all : 0;
        term_printf(t, TERM_TEXT, "%5d  %-16.16s %-9s %3ld%%  %ld KB", p->pid, p->name,
                    TERM_STATES[clamp_int(p->state, 0, PROC_ENDED)], percent, (p->memory + 1023) / 1024);
    }
}

static void term_cmd_kill(Terminal *t, int argc, char **argv) {
    if (argc < 2) term_fail(t, "kill: which process? (ps lists them)");
    for (int i = 1; i < argc; i++) {
        char *end;
        long pid = strtol(argv[i], &end, 10);
        if (*end || end == argv[i]) term_fail(t, "kill: %s is not a process number", argv[i]);
        else if (pid == 0) term_fail(t, "kill: process 0 is the system itself; try shutdown");
        else if (os_kill((int)pid) != 0) term_fail(t, "kill: no process %ld", pid);
    }
}

static void term_cmd_calc(Terminal *t, int argc, char **argv) {
    char expression[TERM_INPUT + 1];
    int length = 0;
    expression[0] = 0;
    for (int i = 1; i < argc; i++)
        length += snprintf(expression + length, sizeof expression - length, "%s ", argv[i]);
    TermParser parser = { expression, 0 };
    double value = term_sum(&parser);
    term_space(&parser);
    if (argc < 2 || parser.error || *parser.at) {
        term_fail(t, "calc: cannot read that; try calc (1 + 2) * 3 ^ 2 / 0x10");
    } else if (value == floor(value) && fabs(value) < 1e15) {
        long long whole = (long long)value;
        if (whole >= 0) term_printf(t, TERM_GREEN, "%lld  (0x%llX)", whole, (unsigned long long)whole);
        else term_printf(t, TERM_GREEN, "%lld", whole);
    } else {
        term_printf(t, TERM_GREEN, "%.12g", value);
    }
}

static void term_cmd_date(Terminal *t, int argc, char **argv) {
    (void)argc, (void)argv;
    DateTime now;
    os_time(&now);
    term_printf(t, TERM_TEXT, "%s %s %2d %02d:%02d:%02d UTC %d", TERM_DAYS[clamp_int(now.weekday, 0, 6)],
                TERM_MONTHS[clamp_int(now.month - 1, 0, 11)], now.day, now.hour, now.minute, now.second,
                now.year);
}

static void term_cmd_uptime(Terminal *t, int argc, char **argv) {
    (void)argc, (void)argv;
    ProcessInfo list[1];
    long seconds = os_now_ms() / 1000;
    int processes = os_processes(list, 1), busy;
    long all;
    os_cpu_load(&busy, &all);
    term_printf(t, TERM_TEXT, "up %ld:%02ld:%02ld, %d processes, %d%% busy, %ld instructions a second",
                seconds / 3600, seconds / 60 % 60, seconds % 60, processes, busy, all);
}

static void term_cmd_whoami(Terminal *t, int argc, char **argv) {
    (void)argc, (void)argv;
    term_write(t, TERM_TEXT, "root");
}

static void term_cmd_uname(Terminal *t, int argc, char **argv) {
    (void)argv;
    if (argc > 1) term_printf(t, TERM_TEXT, "asmOS 1.0 %s", os_machine(MACHINE_UNAME));
    else term_write(t, TERM_TEXT, "asmOS");
}

static void term_cmd_fortune(Terminal *t, int argc, char **argv) {
    (void)argc, (void)argv;
    int count = (int)(sizeof TERM_FORTUNES / sizeof TERM_FORTUNES[0]);
    term_write(t, TERM_YELLOW, TERM_FORTUNES[(rand() + os_now_ms()) % count]);
}

static void term_cmd_beep(Terminal *t, int argc, char **argv) {
    (void)t, (void)argc, (void)argv;
    os_beep();
}

static void term_cmd_clear(Terminal *t, int argc, char **argv) {
    (void)argc, (void)argv;
    term_clear(t);
}

static void term_cmd_history(Terminal *t, int argc, char **argv) {
    (void)argc, (void)argv;
    for (int i = max_int(0, t->history_count - TERM_HISTORY); i < t->history_count; i++)
        term_printf(t, TERM_TEXT, "%4d  %s", i + 1, t->history[i % TERM_HISTORY]);
}

static void term_cmd_exit(Terminal *t, int argc, char **argv) {
    (void)t, (void)argc, (void)argv;
    os_exit(0);
}

static void term_cmd_shutdown(Terminal *t, int argc, char **argv) {
    (void)argc, (void)argv;
    term_write(t, TERM_YELLOW, "Shutting down...");
    os_shutdown();
}

static void term_cmd_head(Terminal *t, int argc, char **argv) {
    int index = 1, count = term_count_option(argc, argv, &index, 10);
    const char *name;
    if (!term_input_name(t, argc, argv, index, &name)) return;
    char *data, **lines;
    int total = term_read_lines(t, argv[0], name, &data, &lines);
    if (total < 0) return;
    int from = strcmp(argv[0], "tail") ? 0 : max_int(0, total - count);
    for (int i = from; i < total && i < from + count; i++) term_write(t, TERM_TEXT, lines[i]);
    term_free_lines(data, lines);
}

static void term_cmd_wc(Terminal *t, int argc, char **argv) {
    const char *name;
    if (!term_input_name(t, argc, argv, 1, &name)) return;
    for (int i = 1; i < argc || (i == 1 && !name); i++) {
        long size;
        char *data = term_read_input(t, "wc", name ? argv[i] : NULL, &size);
        if (!data) continue;
        long lines = 0, words = 0;
        for (long block = 0; block < size; block += 65536) {
            os_yield_point();       /* every 64 KB of a large file */
            for (long k = block; k < size && k < block + 65536; k++) {
                lines += data[k] == '\n';
                words += !isspace((unsigned char)data[k]) && (k == 0 || isspace((unsigned char)data[k - 1]));
            }
        }
        os_free(data);
        term_printf(t, TERM_TEXT, "%6ld %6ld %7ld  %s", lines, words, size, name ? argv[i] : "");
    }
}

/* Whether `text` holds `part`, ignoring case if asked. */
static int term_contains(const char *text, const char *part, int ignore_case) {
    if (!ignore_case) return strstr(text, part) != NULL;
    for (; *text; text++) {
        int i = 0;
        while (part[i] && tolower((unsigned char)text[i]) == tolower((unsigned char)part[i])) i++;
        if (!part[i]) return 1;
    }
    return !part[0];
}

static void term_cmd_grep(Terminal *t, int argc, char **argv) {
    int index = 1, ignore_case = 0;
    if (index < argc && !strcmp(argv[index], "-i")) ignore_case = 1, index++;
    if (argc - index < (t->piped ? 1 : 2)) {
        term_fail(t, "grep: usage: grep [-i] TEXT FILE...");
        return;
    }
    const char *pattern = argv[index++];
    int several = argc - index > 1, found = 0, piped = index == argc;
    for (; index < argc || piped; index++, piped = 0) {
        char *data, **lines;
        int total = term_read_lines(t, "grep", piped ? NULL : argv[index], &data, &lines);
        for (int i = 0; i < total; i++) {
            os_yield_point();
            if (!term_contains(lines[i], pattern, ignore_case)) continue;
            char where[96], text[TERM_LINE_MAX + 100];
            if (several) snprintf(where, sizeof where, "%s:%d: ", argv[index], i + 1);
            else snprintf(where, sizeof where, "%d: ", i + 1);
            snprintf(text, sizeof text, "%s%s", where, lines[i]);
            term_write_split(t, (int)strlen(where), TERM_CYAN, TERM_TEXT, text);
            found++;
        }
        if (total >= 0) term_free_lines(data, lines);
    }
    if (!found && !t->capture) term_write(t, TERM_TEXT, "(no line has it)");
}

/* ---- find, tree and du: a walk through a folder and the folders in it */

enum { TERM_FIND, TERM_TREE, TERM_DU };

/* Walks the folder `path`, shown as `shown`; answers the bytes of the files in it. */
static long term_walk(Terminal *t, int mode, const char *path, const char *shown, const char *indent,
                      const char *part, int depth, int *files, int *folders) {
    if (depth >= TERM_DEPTH) return 0;
    DirEntry *entries = os_alloc(TERM_DIR_MAX * sizeof *entries);
    int count = entries ? os_list_dir(term_fs(path), entries, TERM_DIR_MAX) : 0;
    long bytes = 0;
    for (int i = 0; i < count; i++) {
        os_yield_point();
        DirEntry *entry = &entries[i];
        char inner[256], inner_shown[256], text[TERM_LINE_MAX + 1] = "";
        term_join(inner, sizeof inner, path, entry->name);
        snprintf(inner_shown, sizeof inner_shown, "%s/%s", shown, entry->name);
        *files += !entry->is_dir;
        *folders += entry->is_dir;
        if (mode == TERM_FIND && (!part || term_contains(entry->name, part, 1)))
            term_write(t, entry->is_dir ? TERM_BLUE : TERM_TEXT, inner_shown);
        if (mode == TERM_TREE) {
            int last = i == count - 1;
            int split = snprintf(text, sizeof text, "%s%s", indent, last ? "`-- " : "|-- ");
            snprintf(text + split, sizeof text - split, "%s%s", entry->name, entry->is_dir ? "/" : "");
            term_write_split(t, split, TERM_TEXT, entry->is_dir ? TERM_BLUE : TERM_TEXT, text);
            snprintf(text, sizeof text, "%s%s", indent, last ? "    " : "|   ");
        }
        if (entry->is_dir) {
            long inside = term_walk(t, mode, inner, inner_shown, text, part, depth + 1, files, folders);
            if (mode == TERM_DU) {
                char size[24];
                term_size_text(size, sizeof size, inside);
                term_printf(t, TERM_TEXT, "%8s  %s", size, inner_shown);
            }
            bytes += inside;
        } else {
            bytes += entry->size;
        }
    }
    os_free(entries);
    return bytes;
}

static void term_cmd_walk(Terminal *t, int argc, char **argv) {
    int mode = !strcmp(argv[0], "find") ? TERM_FIND : !strcmp(argv[0], "tree") ? TERM_TREE : TERM_DU;
    const char *name = argc > 1 ? argv[1] : ".", *part = argc > 2 ? argv[2] : NULL;
    char path[160];
    term_resolve(t, name, path, sizeof path);
    if (mode == TERM_FIND && argc == 2 && os_stat(term_fs(path), NULL) != 2) {
        /* find NAME looks for it from the current folder */
        part = name;
        name = ".";
        term_resolve(t, name, path, sizeof path);
    }
    if (os_stat(term_fs(path), NULL) != 2) {
        term_fail(t, "%s: %s: no such folder", argv[0], name);
        return;
    }
    if (!term_can(t, FS_LIST, argv[0], "list folders")) return;
    if (mode == TERM_TREE) term_write(t, TERM_BLUE, name);
    int files = 0, folders = 0;
    long bytes = term_walk(t, mode, path, name, "", part, 0, &files, &folders);
    char size[24];
    term_size_text(size, sizeof size, bytes);
    if (mode == TERM_DU)
        term_printf(t, TERM_YELLOW, "%8s  %s, %d files in %d folders", size, name, files, folders);
    if (mode == TERM_TREE) term_printf(t, TERM_YELLOW, "%d folders, %d files, %s", folders, files, size);
}

static void term_cmd_stat(Terminal *t, int argc, char **argv) {
    if (argc < 2) {
        term_fail(t, "stat: which file?");
        return;
    }
    char path[160], size_text[24];
    long size;
    term_resolve(t, argv[1], path, sizeof path);
    int kind = os_stat(term_fs(path), &size);
    if (!kind) {
        term_fail(t, "stat: %s: no such file or folder", argv[1]);
        return;
    }
    term_printf(t, TERM_TEXT, "  Path: /project%s%s", path[0] ? "/" : "", path);
    if (kind == 2) {
        DirEntry *entries = os_alloc(TERM_DIR_MAX * sizeof *entries);
        int count = entries ? os_list_dir(path, entries, TERM_DIR_MAX) : 0;
        os_free(entries);
        term_printf(t, TERM_TEXT, "  Type: folder, %d item%s", count, count == 1 ? "" : "s");
        return;
    }
    int app = os_app_for_file(path);
    term_size_text(size_text, sizeof size_text, size);
    term_printf(t, TERM_TEXT, "  Type: file\n  Size: %ld bytes (%s)\n  Opens with: %s", size, size_text,
                app >= 0 ? APPS[app].name : "nothing");
}

static int term_line_order(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* sort, uniq and rev: a file's lines sorted, without repeats, or each turned round. */
static void term_cmd_lines(Terminal *t, int argc, char **argv) {
    const char *name;
    if (!term_input_name(t, argc, argv, 1, &name)) return;
    char *data, **lines;
    int total = term_read_lines(t, argv[0], name, &data, &lines);
    if (total < 0) return;
    if (!strcmp(argv[0], "sort")) qsort(lines, total, sizeof *lines, term_line_order);
    for (int i = 0; i < total; i++) {
        if (!strcmp(argv[0], "uniq") && i > 0 && !strcmp(lines[i], lines[i - 1])) continue;
        if (!strcmp(argv[0], "rev")) {
            int length = (int)strlen(lines[i]);
            for (int a = 0, b = length - 1; a < b; a++, b--) {
                char c = lines[i][a];
                lines[i][a] = lines[i][b];
                lines[i][b] = c;
            }
        }
        term_write(t, TERM_TEXT, lines[i]);
    }
    term_free_lines(data, lines);
}

static void term_cmd_seq(Terminal *t, int argc, char **argv) {
    if (argc < 2) {
        term_fail(t, "seq: usage: seq [FROM] TO");
        return;
    }
    long from = argc > 2 ? atol(argv[1]) : 1, to = atol(argv[argc > 2 ? 2 : 1]);
    long step = from <= to ? 1 : -1, count = 0;
    /* the screen keeps only the last lines anyway, and a file takes no more than 100000 */
    if (!t->capture && labs(to - from) >= TERM_LINES) from = to - step * (TERM_LINES - 1);
    for (long n = from; count < 100000; n += step, count++) {
        term_printf(t, TERM_TEXT, "%ld", n);
        if (n == to) break;
    }
}

static void term_cmd_sleep(Terminal *t, int argc, char **argv) {
    double seconds = argc > 1 ? strtod(argv[1], NULL) : 1;
    term_draw_all(t);
    os_sleep(seconds > 0 ? (int)(fmin(seconds, 60) * 1000) : 0);
}

static void term_cmd_help(Terminal *t, int argc, char **argv);

typedef struct {
    const char *name, *usage, *help;
    void (*run)(Terminal *t, int argc, char **argv);
} TermCommand;

static const TermCommand term_commands[] = {
    { "help", "help", "this list", term_cmd_help },
    { "ls", "ls [-l] [DIR]", "the files in a folder; -l with sizes", term_cmd_ls },
    { "cd", "cd [DIR]", "go into a folder; .. is the one above", term_cmd_cd },
    { "pwd", "pwd", "the current folder", term_cmd_pwd },
    { "cat", "cat FILE...", "print files", term_cmd_cat },
    { "echo", "echo TEXT", "print text (echo hi > FILE writes it)", term_cmd_echo },
    { "cp", "cp FROM TO", "copy a file", term_cmd_cp },
    { "mv", "mv FROM TO", "move or rename a file or folder", term_cmd_mv },
    { "rm", "rm [-r] FILE...", "delete files; -r folders too", term_cmd_rm },
    { "mkdir", "mkdir DIR...", "make folders", term_cmd_mkdir },
    { "rmdir", "rmdir DIR...", "delete empty folders", term_cmd_rmdir },
    { "touch", "touch FILE...", "make empty files", term_cmd_touch },
    { "head", "head [-n N] FILE", "the first lines of a file", term_cmd_head },
    { "tail", "tail [-n N] FILE", "the last lines of a file", term_cmd_head },
    { "wc", "wc FILE...", "count lines, words and bytes", term_cmd_wc },
    { "grep", "grep [-i] TEXT FILE...", "the lines that have TEXT", term_cmd_grep },
    { "find", "find [DIR] [NAME]", "every file below, or those named so", term_cmd_walk },
    { "tree", "tree [DIR]", "the folders below, as a tree", term_cmd_walk },
    { "du", "du [DIR]", "the space the folders take", term_cmd_walk },
    { "stat", "stat FILE", "a file's size and application", term_cmd_stat },
    { "sort", "sort FILE", "a file's lines in order", term_cmd_lines },
    { "uniq", "uniq FILE", "a file's lines without repeats", term_cmd_lines },
    { "rev", "rev FILE", "each line back to front", term_cmd_lines },
    { "seq", "seq [FROM] TO", "count", term_cmd_seq },
    { "edit", "edit FILE", "edit a file in Notepad", term_cmd_edit },
    { "asm", "asm [FILE]", "write and run RISC-V assembly", term_cmd_asm },
    { "open", "open APP [ARG]", "start an application, or open a file", term_cmd_open },
    { "ps", "ps", "the processes running", term_cmd_ps },
    { "kill", "kill PID", "end a process", term_cmd_kill },
    { "calc", "calc EXPRESSION", "work out 2 * (3 + 4) ^ 2 / 0x10", term_cmd_calc },
    { "sleep", "sleep SECONDS", "wait", term_cmd_sleep },
    { "date", "date", "the date and time, UTC", term_cmd_date },
    { "uptime", "uptime", "how long asmOS has run", term_cmd_uptime },
    { "whoami", "whoami", "who you are", term_cmd_whoami },
    { "uname", "uname [-a]", "the system's name", term_cmd_uname },
    { "fortune", "fortune", "a saying", term_cmd_fortune },
    { "beep", "beep", "a beep", term_cmd_beep },
    { "history", "history", "the commands typed", term_cmd_history },
    { "clear", "clear", "clear the screen (Ctrl+L)", term_cmd_clear },
    { "exit", "exit", "close this Terminal", term_cmd_exit },
    { "shutdown", "shutdown", "turn asmOS off", term_cmd_shutdown },
};
enum { TERM_COMMAND_COUNT = sizeof term_commands / sizeof term_commands[0] };

static void term_cmd_help(Terminal *t, int argc, char **argv) {
    (void)argc, (void)argv;
    term_write(t, TERM_YELLOW, "Commands (paths are relative to the current folder):");
    for (int i = 0; i < TERM_COMMAND_COUNT; i++) {
        char text[128];
        int split = snprintf(text, sizeof text, "  %-24s", term_commands[i].usage);
        snprintf(text + split, sizeof text - split, "%s", term_commands[i].help);
        term_write_split(t, split, TERM_WHITE, TERM_TEXT, text);
    }
    term_write(t, TERM_YELLOW, "COMMAND > FILE writes the output into FILE; >> FILE adds to it.");
    term_write(t, TERM_YELLOW, "A | B gives A's output to B, in place of a file: ls | sort.");
    term_write(t, TERM_YELLOW, "Up, Down: history. Tab: complete a name. Ctrl+C: cancel.");
}

/* ------------------------------------------------------------------ running a line */

/* A command line cut into words: spaces part them, quotes keep spaces in one, and > or >> names
 * the file the output goes to. */
typedef struct {
    char buffer[TERM_INPUT + TERM_WORDS + 2];
    char *words[TERM_WORDS];
    int count;
    char *into;
    int append, missing;
} TermWords;

static void term_split(const char *line, TermWords *c) {
    char *write = c->buffer;
    int redirect = 0;
    c->count = 0;
    c->into = NULL;
    c->append = c->missing = 0;
    for (;;) {
        while (*line == ' ') line++;
        if (!*line) break;
        if (*line == '>') {
            line++;
            if (*line == '>') line++, c->append = 1;
            redirect = 1;
            continue;
        }
        char *word = write;
        char quote = 0;
        while (*line && (quote || (*line != ' ' && *line != '>'))) {
            if (quote && *line == quote) quote = 0, line++;
            else if (!quote && (*line == '"' || *line == '\'')) quote = *line++;
            else *write++ = *line++;
        }
        *write++ = 0;
        if (redirect) {
            c->into = word;
            redirect = 0;
        } else if (c->count < TERM_WORDS) {
            c->words[c->count++] = word;
        }
    }
    c->missing = redirect;
}

/* Writes the output a command made into the file after > or >>. */
static void term_finish_capture(Terminal *t, const TermWords *c, const char *path) {
    char *data = t->capture;
    long size = t->capture_length;
    t->capture = NULL;
    if (t->capture_failed) {
        term_fail(t, "%s: out of memory", c->into);
    } else if (c->append && os_stat(path, NULL) == 1) {
        long old_size;
        char *old = os_read_file(path, &old_size);
        char *both = old ? os_alloc(old_size + size + 1) : NULL;
        if (both) {
            memcpy(both, old, old_size);
            memcpy(both + old_size, data, size);
            if (os_write_file(path, both, old_size + size) != 0) term_fail(t, "cannot write %s", c->into);
        } else {
            term_fail(t, "cannot read %s", c->into);
        }
        os_free(both);
        os_free(old);
    } else if (os_write_file(path, data ? data : "", size) != 0) {
        term_fail(t, "cannot write %s", c->into);
    }
    os_free(data);
}

/* Runs one command of a line. With `piped` its output goes to the next command, answered in
 * `piped` (memory to os_free). Answers 0 when it could not run, which ends the line. */
static int term_run_one(Terminal *t, const char *line, char **piped, int *piped_length) {
    TermWords c;
    term_split(line, &c);
    if (!c.count) {
        if (piped || t->piped) term_fail(t, "which command, next to |?");
        return 0;
    }
    char path[160] = "";
    if (c.missing || (c.into && !c.into[0])) {
        term_fail(t, "%s: which file after >?", c.words[0]);
        return 0;
    }
    if (c.into) {
        term_resolve(t, c.into, path, sizeof path);
        if (!path[0] || os_stat(path, NULL) == 2) {
            term_fail(t, "%s: %s is a folder", c.words[0], c.into);
            return 0;
        }
    }
    for (int i = 0; i < TERM_COMMAND_COUNT; i++) {
        if (strcmp(c.words[0], term_commands[i].name)) continue;
        if (c.into || piped) {
            t->capture = os_alloc(256);
            t->capture_size = t->capture ? 256 : 0;
            t->capture_length = 0;
            t->capture_failed = !t->capture;
            if (t->capture_failed) {
                term_fail(t, "%s: out of memory", c.words[0]);
                return 0;
            }
        }
        term_commands[i].run(t, c.count, c.words);
        if (c.into) {
            term_finish_capture(t, &c, path);
        } else if (piped) {
            *piped = t->capture;
            *piped_length = t->capture_length;
            t->capture = NULL;
        }
        return 1;
    }
    int app = term_app_named(c.words[0]);
    if (app >= 0)
        term_printf(t, TERM_RED, "%s: command not found (open %s starts the application)", c.words[0],
                    c.words[0]);
    else
        term_printf(t, TERM_RED, "%s: command not found (try help)", c.words[0]);
    return 0;
}

/* Runs a line: its commands, parted by | outside quotes, one after the other, each reading what
 * the one before it wrote. */
static void term_run(Terminal *t, const char *line) {
    char text[TERM_INPUT + 1];
    char *parts[TERM_WORDS];
    int count = 1;
    char quote = 0;
    copy_text(text, sizeof text, line);
    parts[0] = text;
    for (char *at = text; *at; at++) {
        if (quote) {
            if (*at == quote) quote = 0;
        } else if (*at == '"' || *at == '\'') {
            quote = *at;
        } else if (*at == '|' && count < TERM_WORDS) {
            *at = 0;
            parts[count++] = at + 1;
        }
    }
    if (count == 1) {
        term_run_one(t, text, NULL, NULL);
        return;
    }
    char *piped = NULL;
    int piped_length = 0;
    for (int i = 0; i < count; i++) {
        char *out = NULL;
        int out_length = 0;
        t->piped = i ? (piped ? piped : "") : NULL;
        t->piped_length = piped_length;
        os_yield_point();   /* between the commands of a pipe */
        int ran = term_run_one(t, parts[i], i + 1 < count ? &out : NULL, &out_length);
        os_free(piped);
        piped = out;
        piped_length = out_length;
        if (!ran) break;
    }
    t->piped = NULL;
    os_free(piped);
}

/* ------------------------------------------------------------------ the line being typed */

static void term_insert(Terminal *t, const char *text, int count) {
    count = min_int(count, TERM_INPUT - t->length);
    if (count <= 0) return;
    memmove(t->input + t->caret + count, t->input + t->caret, t->length - t->caret + 1);
    memcpy(t->input + t->caret, text, count);
    t->length += count;
    t->caret += count;
}

static void term_set_input(Terminal *t, const char *text) {
    copy_text(t->input, sizeof t->input, text);
    t->length = t->caret = (int)strlen(t->input);
}

/* A path for the line: from the root, in quotes when it has spaces. */
static void term_insert_path(Terminal *t, const char *path) {
    char text[200];
    int quote = strchr(path, ' ') != NULL;
    const char *mark = quote ? "\"" : "";
    snprintf(text, sizeof text, "%s/project%s%s%s ", mark, path[0] ? "/" : "", path, mark);
    term_insert(t, text, (int)strlen(text));
}

static void term_paste(Terminal *t) {
    int length;
    char *text = os_clipboard_text(&length);
    if (!text) {
        os_beep();
        return;
    }
    if (!strncmp(text, "file:", 5)) {
        term_insert_path(t, text + 5);
    } else {
        for (int i = 0; i < length; i++) {
            char c = text[i] == '\n' || text[i] == '\t' ? ' ' : text[i];
            if (c >= 32 && c < 127) term_insert(t, &c, 1);
        }
    }
    os_free(text);
}

/* The text shown, onto the clipboard: a line wrapped over rows comes back as one. */
static void term_copy(Terminal *t) {
    int total = term_total_rows(t), top = term_top(t, total), rows = term_rows(t), columns = term_columns(t);
    int row = 0;
    char *text = os_alloc((size_t)(rows + 2) * (columns + 1) + TERM_INPUT + 200);
    if (!text) return;
    int length = 0;
    for (int i = 0; i <= t->count; i++) {
        char prompt_line[TERM_INPUT + 168];
        const char *line;
        int count;
        if (i < t->count) {
            line = t->lines[(t->first + i) % TERM_LINES].text;
            count = t->lines[(t->first + i) % TERM_LINES].length;
        } else {
            count = term_prompt(t, prompt_line, sizeof prompt_line - TERM_INPUT - 1);
            memcpy(prompt_line + count, t->input, t->length + 1);
            count += t->length;
            line = prompt_line;
        }
        int line_rows = term_line_rows(t, count), shown = 0;
        for (int part = 0; part < line_rows; part++) {
            if (row + part < top || row + part >= top + rows) continue;
            int from = part * columns, n = max_int(0, min_int(columns, count - from));
            memcpy(text + length, line + from, n);
            length += n;
            shown = 1;
        }
        if (shown) text[length++] = '\n';
        row += line_rows;
    }
    os_clipboard_set_text(text, length);
    os_free(text);
}

/* Tab: the name before the caret made as long as the files (or the commands, for the first word)
 * that begin with it agree; when that adds nothing, they are listed. */
static void term_complete(Terminal *t) {
    if (t->caret != t->length) return;
    int start = t->length;
    while (start > 0 && t->input[start - 1] != ' ') start--;
    const char *word = t->input + start;
    char matches[64][64];
    int count = 0, dirs[64];
    const char *prefix = word;
    char folder[160];
    if (start == 0 || strspn(t->input, " ") == (size_t)start) {
        for (int i = 0; i < TERM_COMMAND_COUNT && count < 64; i++)
            if (!strncmp(term_commands[i].name, word, strlen(word))) {
                copy_text(matches[count], sizeof matches[count], term_commands[i].name);
                dirs[count++] = 0;
            }
    } else {
        const char *slash = strrchr(word, '/');
        char dir_part[160];
        snprintf(dir_part, sizeof dir_part, "%.*s", slash ? (int)(slash - word) + 1 : 0, word);
        prefix = slash ? slash + 1 : word;
        term_resolve(t, dir_part[0] ? dir_part : ".", folder, sizeof folder);
        DirEntry *entries = os_alloc(TERM_DIR_MAX * sizeof *entries);
        int listed = entries ? os_list_dir(folder, entries, TERM_DIR_MAX) : 0;
        for (int i = 0; i < listed && count < 64; i++)
            if (!strncmp(entries[i].name, prefix, strlen(prefix))) {
                copy_text(matches[count], sizeof matches[count], entries[i].name);
                dirs[count++] = entries[i].is_dir;
            }
        os_free(entries);
    }
    if (!count) {
        os_beep();
        return;
    }
    int common = (int)strlen(matches[0]);
    for (int i = 1; i < count; i++) {
        int k = 0;
        while (k < common && matches[i][k] == matches[0][k]) k++;
        common = k;
    }
    int typed = (int)strlen(prefix);
    if (count == 1) {
        term_insert(t, matches[0] + typed, common - typed);
        term_insert(t, dirs[0] ? "/" : " ", 1);
    } else if (common > typed) {
        term_insert(t, matches[0] + typed, common - typed);
    } else {
        char prompt[160], line[TERM_INPUT + 168], list[TERM_LINE_MAX + 1];
        int split = term_prompt(t, prompt, sizeof prompt);
        snprintf(line, sizeof line, "%s%s", prompt, t->input);
        term_write_split(t, split, TERM_GREEN, TERM_WHITE, line);
        int length = 0;
        for (int i = 0; i < count && length < TERM_LINE_MAX - 2; i++)
            length += snprintf(list + length, sizeof list - length, "%s%s  ", matches[i], dirs[i] ? "/" : "");
        term_write(t, TERM_TEXT, list);
    }
}

static void term_enter(Terminal *t) {
    char line[TERM_INPUT + 168], command[TERM_INPUT + 1];
    int split = term_prompt(t, line, sizeof line - TERM_INPUT - 1);
    memcpy(line + split, t->input, t->length + 1);
    term_write_split(t, split, TERM_GREEN, TERM_WHITE, line);
    const char *last = t->history_count ? t->history[(t->history_count - 1) % TERM_HISTORY] : "";
    if (t->length && strcmp(last, t->input)) {
        copy_text(t->history[t->history_count % TERM_HISTORY], TERM_INPUT + 1, t->input);
        t->history_count++;
    }
    t->history_at = t->history_count;
    copy_text(command, sizeof command, t->input);
    term_set_input(t, "");
    t->back = 0;
    term_run(t, command);
    t->back = 0;
    term_draw_all(t);
}

static void term_history(Terminal *t, int step) {
    int oldest = max_int(0, t->history_count - TERM_HISTORY), at = t->history_at + step;
    if (at < oldest || at > t->history_count) return;
    if (t->history_at == t->history_count) copy_text(t->draft, sizeof t->draft, t->input);
    t->history_at = at;
    term_set_input(t, at == t->history_count ? t->draft : t->history[at % TERM_HISTORY]);
}

static void term_command(Terminal *t, int id) {
    switch (id) {
    case TERM_COPY: term_copy(t); return;
    case TERM_PASTE: term_paste(t); break;
    case TERM_CLEAR: term_clear(t); break;
    case TERM_HELP: term_cmd_help(t, 0, NULL); break;
    case TERM_ABOUT:
        os_message_box(t->win, "About Terminal",
                       "Terminal for asmOS.\nA shell over the Project's files: type help for its commands.",
                       MB_OK, MB_INFO);
        return;
    }
    t->back = 0;
    t->blink = 1;
    term_draw_all(t);
}

static void term_key(Terminal *t, const Message *m) {
    int rows = term_rows(t);
    if (m->mods & MOD_CTRL) {
        if (m->key == KEY_C) {
            char line[TERM_INPUT + 168];
            int split = term_prompt(t, line, sizeof line - TERM_INPUT - 4);
            snprintf(line + split, sizeof line - split, "%s^C", t->input);
            term_write_split(t, split, TERM_GREEN, TERM_WHITE, line);
            term_set_input(t, "");
            t->history_at = t->history_count;
        } else if (m->key == KEY_L) {
            term_clear(t);
        } else if (m->key == KEY_V) {
            term_paste(t);
        } else if (m->key == KEY_A) {
            t->caret = 0;
        } else if (m->key == KEY_E) {
            t->caret = t->length;
        } else if (m->key == KEY_U) {
            term_set_input(t, "");
        } else {
            return;
        }
        t->back = 0;
        t->blink = 1;
        term_draw_all(t);
        return;
    }
    switch (m->key) {
    case KEY_ENTER: case KEY_KPENTER: t->blink = 1; term_enter(t); return;
    case KEY_PAGEUP: term_scroll_by(t, rows - 1); return;
    case KEY_PAGEDOWN: term_scroll_by(t, -(rows - 1)); return;
    case KEY_UP: term_history(t, -1); break;
    case KEY_DOWN: term_history(t, 1); break;
    case KEY_LEFT: t->caret = max_int(0, t->caret - 1); break;
    case KEY_RIGHT: t->caret = min_int(t->length, t->caret + 1); break;
    case KEY_HOME: t->caret = 0; break;
    case KEY_END: t->caret = t->length; break;
    case KEY_TAB: term_complete(t); break;
    case KEY_BACKSPACE:
        if (t->caret == 0) return;
        memmove(t->input + t->caret - 1, t->input + t->caret, t->length - t->caret + 1);
        t->caret--;
        t->length--;
        break;
    case KEY_DELETE:
        if (t->caret == t->length) return;
        memmove(t->input + t->caret, t->input + t->caret + 1, t->length - t->caret);
        t->length--;
        break;
    default:
        if (m->ch < 32 || m->ch >= 127) return;
        char c = (char)m->ch;
        term_insert(t, &c, 1);
    }
    t->blink = 1;
    if (t->back) {
        t->back = 0;
        term_draw_all(t);
    } else {
        term_draw_prompt(t);
    }
}

static void term_mouse(Terminal *t, const Message *m) {
    if (scroll_bar_message(&t->scroll, m)) {
        int total = term_total_rows(t), most = term_most_back(t, total);
        int back = clamp_int(most - t->scroll.top, 0, most);
        if (back != t->back) {
            t->back = back;
            term_draw_all(t);
        } else {
            os_window_draw(t->win);
            draw_scroll_bar(&t->scroll);
            os_window_update(t->win, t->scroll.r.x, t->scroll.r.y, t->scroll.r.w, t->scroll.r.h);
        }
        return;
    }
    if (m->type == MSG_MOUSE_WHEEL) {
        term_scroll_by(t, m->wheel * 3);
    } else if (m->type == MSG_MOUSE_DOWN && m->button == BUTTON_RIGHT && m->x < t->w - TERM_SCROLL_W) {
        int id = os_popup_menu(t->win, m->x, m->y, term_context_menu);
        if (id) term_command(t, id);
    }
}

void terminal_main(const char *argument) {
    Terminal *t = os_alloc(sizeof *t);
    if (!t) os_exit(1);
    t->win = os_window_create("Terminal", &ICON_TERMINAL, 536, 290, WIN_RESIZABLE);
    if (!t->win) os_exit(1);
    os_window_set_menu(t->win, term_menus);
    os_window_min_size(t->win, 240, 100);
    os_window_pixels(t->win, &t->w, &t->h);
    t->blink = 1;
    /* started on a folder, as Files can, the shell begins in it */
    if (argument && argument[0]) {
        char path[160];
        term_resolve(t, argument, path, sizeof path);
        if (os_stat(term_fs(path), NULL) == 2) copy_text(t->cwd, sizeof t->cwd, path);
    }
    term_write(t, TERM_YELLOW, "asmOS shell. Type help for the commands.");
    term_draw_all(t);
    os_set_timer(t->win, TERM_BLINK, 530);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_KEY: term_key(t, &m); break;
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: case MSG_MOUSE_WHEEL:
            term_mouse(t, &m);
            break;
        case MSG_COMMAND: term_command(t, m.id); break;
        case MSG_TIMER:
            t->blink = !t->blink;
            if (os_window_focused(t->win)) term_draw_prompt(t);
            break;
        case MSG_RESIZE:
            os_window_pixels(t->win, &t->w, &t->h);
            term_draw_all(t);
            break;
        case MSG_FOCUS: case MSG_BLUR: case MSG_SETTINGS: term_draw_prompt(t); break;
        case MSG_DROP:
            term_insert_path(t, m.text);
            t->back = 0;
            term_draw_all(t);
            break;
        default: os_default_message(&m);
        }
    }
}
