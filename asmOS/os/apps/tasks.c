/*
 * Task Manager: the processes asmOS runs, with what each costs, counted again every second. End
 * Process stops one, as `kill` does in the Terminal, after asking. The Performance tab draws how
 * busy the processor was over the last minute, as a graph that scrolls to the left.
 *
 * A process's CPU is its share of the instructions all of them ran in the last second, times how
 * busy the processor was: the rest of the time it waited for input, which costs nothing.
 */

enum { TASKS_MAX = 32, TASKS_HISTORY = 60, TASKS_ROW_H = 18, TASKS_HEADER_H = 18, TASKS_TAB_H = 20,
       TASKS_STATUS_H = 20, TASKS_SCROLL_W = 16 };
enum { TASKS_NEW = 1, TASKS_EXIT, TASKS_SHOW_PROCESSES, TASKS_SHOW_PERFORMANCE, TASKS_REFRESH, TASKS_END,
       TASKS_ABOUT };
enum { TASKS_TICK = 1 };
enum { TASKS_PROCESSES, TASKS_PERFORMANCE };
enum { TASKS_GRAPH_BACK = 0x000000, TASKS_GRAPH_GRID = 0x008040, TASKS_GRAPH_LINE = 0x00ff00,
       TASKS_METER_OFF = 0x00502a };

static const MenuItem tasks_file_template[] = {
    { "New Task...", TASKS_NEW, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit Task Manager", TASKS_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem tasks_view_template[] = {
    { "Processes", TASKS_SHOW_PROCESSES, MENU_CHECKED, NULL, NULL },
    { "Performance", TASKS_SHOW_PERFORMANCE, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Refresh Now", TASKS_REFRESH, 0, "F5", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem tasks_process_template[] = {
    { "End Process", TASKS_END, 0, "Del", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem tasks_help_template[] = {
    { "About Task Manager", TASKS_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem tasks_context_menu[] = {
    { "End Process", TASKS_END, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Refresh Now", TASKS_REFRESH, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

typedef struct {
    Window *win;
    int w, h;                 /* the client area */
    int tab;
    ProcessInfo list[TASKS_MAX];
    int count;
    int selected;             /* the pid of the process selected, or -1 */
    int top;                  /* the first row shown */
    int busy;
    long instructions;
    int history[TASKS_HISTORY];   /* the busy percent, oldest first */
    int samples;              /* how many seconds the history has */
    int scrolled;             /* seconds counted, which moves the graph's grid */
    ScrollBar scroll;
    ButtonTracker buttons;
    /* the menus are this window's own copies, since View's checks change */
    MenuItem file_menu[4], view_menu[5], process_menu[2], help_menu[2];
    Menu menus[5];
} TaskManager;

/* ------------------------------------------------------------------ layout */

static Rect tasks_tab_rect(int tab) {
    return tab == TASKS_PROCESSES ? rect(6, 4, 86, TASKS_TAB_H) : rect(92, 4, 102, TASKS_TAB_H);
}
static Rect tasks_panel(TaskManager *t) {
    return rect(4, 4 + TASKS_TAB_H, t->w - 8, t->h - 4 - TASKS_TAB_H - TASKS_STATUS_H - 4);
}
static Rect tasks_end_button(TaskManager *t) {
    Rect panel = tasks_panel(t);
    return rect(panel.x + panel.w - 10 - 100, panel.y + panel.h - 10 - 24, 100, 24);
}
/* The list's field, its header included. */
static Rect tasks_list_rect(TaskManager *t) {
    Rect panel = tasks_panel(t);
    return rect(panel.x + 10, panel.y + 10, panel.w - 20, panel.h - 20 - 24 - 8);
}
static int tasks_rows(TaskManager *t) {
    return max_int(1, (tasks_list_rect(t).h - 4 - TASKS_HEADER_H) / TASKS_ROW_H);
}
/* The columns' left edges, from the list's right edge less the scroll bar: Name, PID, State, CPU,
 * Memory. */
static void tasks_columns(TaskManager *t, int *x) {
    Rect list = tasks_list_rect(t);
    int right = list.x + list.w - 2 - TASKS_SCROLL_W;
    x[5] = right;
    x[4] = right - 76;
    x[3] = x[4] - 48;
    x[2] = x[3] - 74;
    x[1] = x[2] - 44;
    x[0] = list.x + 2;
}

static ProcessInfo *tasks_selected_info(TaskManager *t) {
    for (int i = 0; i < t->count; i++)
        if (t->list[i].pid == t->selected) return &t->list[i];
    return NULL;
}
static int tasks_selected_index(TaskManager *t) {
    ProcessInfo *info = tasks_selected_info(t);
    return info ? (int)(info - t->list) : -1;
}

/* A size in bytes as people read it: "812 B", "34 KB", "1.5 MB". */
static void tasks_bytes(char *text, size_t size, long bytes) {
    if (bytes < 1024) snprintf(text, size, "%ld B", bytes);
    else if (bytes < 1024 * 1024) snprintf(text, size, "%ld KB", (bytes + 512) / 1024);
    else snprintf(text, size, "%ld.%ld MB", bytes / (1024 * 1024), bytes % (1024 * 1024) * 10 / (1024 * 1024));
}

/* A process's share of the processor in the last second, in percent. */
static int tasks_cpu_percent(TaskManager *t, const ProcessInfo *info) {
    if (t->instructions <= 0) return 0;
    return (int)((info->cpu * t->busy + t->instructions / 2) / t->instructions);
}

static long tasks_memory_total(TaskManager *t) {
    long memory = 0;
    for (int i = 0; i < t->count; i++) memory += t->list[i].memory;
    return memory;
}

/* ------------------------------------------------------------------ the numbers */

static void tasks_measure(TaskManager *t) {
    t->count = min_int(os_processes(t->list, TASKS_MAX), TASKS_MAX);
    os_cpu_load(&t->busy, &t->instructions);
    if (t->selected >= 0 && !tasks_selected_info(t)) t->selected = -1;
    t->top = clamp_int(t->top, 0, max_int(0, t->count - tasks_rows(t)));
}

/* Once a second: the numbers again, and the busy percent onto the history. */
static void tasks_sample(TaskManager *t) {
    tasks_measure(t);
    if (t->samples == TASKS_HISTORY) {
        memmove(t->history, t->history + 1, (TASKS_HISTORY - 1) * sizeof t->history[0]);
        t->samples--;
    }
    t->history[t->samples++] = t->busy;
    t->scrolled++;
}

/* ------------------------------------------------------------------ drawing */

/* An etched frame with a caption in its top line, as dialogs group their parts. */
static void tasks_group(int x, int y, int w, int h, const char *caption) {
    frame_rect(x + 1, y + 1, w - 1, h - 1, WHITE);
    frame_rect(x, y, w - 1, h - 1, SHADOW);
    int width = text_width(caption);
    fill_rect(x + 8, y - 6, width + 6, FONT_HEIGHT, FACE);
    draw_text(x + 11, y - 6, caption, BLACK);
}

static void tasks_draw_tabs(TaskManager *t) {
    static const char *const labels[2] = { "Processes", "Performance" };
    fill_rect(0, 0, t->w, 4 + TASKS_TAB_H, FACE);
    for (int tab = 0; tab < 2; tab++) {
        Rect r = tasks_tab_rect(tab);
        int chosen = tab == t->tab;
        /* the chosen tab stands 2 pixels taller and joins the panel under it */
        if (!chosen) r = rect(r.x + 2, r.y + 2, r.w - 4, r.h - 2);
        else r = rect(r.x - 2, r.y, r.w + 4, r.h + 1);
        hline(r.x + 2, r.y, r.w - 4, WHITE);
        vline(r.x, r.y + 2, r.h - 2, WHITE);
        put_pixel(r.x + 1, r.y + 1, WHITE);
        vline(r.x + r.w - 1, r.y + 2, r.h - 2, BLACK);
        vline(r.x + r.w - 2, r.y + 1, r.h - 1, SHADOW);
        draw_text(r.x + (r.w - text_width(labels[tab])) / 2, r.y + 4, labels[tab], BLACK);
    }
}

/* The panel's frame, white above and left and dark below and right, open under the chosen tab. */
static void tasks_draw_panel_frame(TaskManager *t) {
    Rect panel = tasks_panel(t);
    fill_rect(panel.x, panel.y, panel.w, panel.h, FACE);
    hline(panel.x, panel.y, panel.w, WHITE);
    vline(panel.x, panel.y, panel.h, WHITE);
    hline(panel.x, panel.y + panel.h - 1, panel.w, BLACK);
    vline(panel.x + panel.w - 1, panel.y, panel.h, BLACK);
    hline(panel.x + 1, panel.y + panel.h - 2, panel.w - 2, SHADOW);
    vline(panel.x + panel.w - 2, panel.y + 1, panel.h - 2, SHADOW);
    Rect tab = tasks_tab_rect(t->tab);
    hline(tab.x - 1, panel.y, tab.w + 2, FACE);
}

static void tasks_draw_row(TaskManager *t, int row, const int *x) {
    Rect list = tasks_list_rect(t);
    int y = list.y + 2 + TASKS_HEADER_H + row * TASKS_ROW_H, index = t->top + row;
    fill_rect(list.x + 2, y, x[5] - list.x - 2, TASKS_ROW_H, WHITE);
    if (index >= t->count) return;
    ProcessInfo *info = &t->list[index];
    int chosen = info->pid == t->selected;
    uint32_t ink = chosen ? WHITE : BLACK;
    if (chosen) fill_rect(list.x + 2, y, x[5] - list.x - 2, TASKS_ROW_H, os_theme_colour(THEME_SELECTION));
    int app = os_app_named(info->name);
    draw_icon(info->pid == 0 ? &ICON_COMPUTER : app >= 0 ? APPS[app].icon : &ICON_FILE, x[0] + 3, y + 1, 1);
    gfx_clip(rect(list.x + 2, list.y + 2, x[1] - list.x - 6, list.h - 4));
    draw_text(x[0] + 24, y + 3, info->name, ink);
    gfx_clip(rect(0, 0, t->w, t->h));
    static const char *const states[] = { "Ready", "Running", "Waiting", "Sleeping", "Ended" };
    char text[24];
    snprintf(text, sizeof text, "%d", info->pid);
    draw_text(x[2] - 8 - text_width(text), y + 3, text, ink);
    draw_text(x[2] + 6, y + 3, states[clamp_int(info->state, 0, 4)], ink);
    snprintf(text, sizeof text, "%d%%", tasks_cpu_percent(t, info));
    draw_text(x[4] - 8 - text_width(text), y + 3, text, ink);
    tasks_bytes(text, sizeof text, info->memory);
    draw_text(x[5] - 8 - text_width(text), y + 3, text, ink);
}

static void tasks_draw_list(TaskManager *t) {
    Rect list = tasks_list_rect(t);
    int x[6];
    tasks_columns(t, x);
    draw_field(list.x, list.y, list.w, list.h, WHITE);
    static const char *const headers[5] = { "Name", "PID", "State", "CPU", "Memory" };
    for (int c = 0; c < 5; c++) {
        draw_button(x[c], list.y + 2, x[c + 1] - x[c], TASKS_HEADER_H, "", 0);
        int width = text_width(headers[c]);
        /* the numbers' headers are right-aligned over them */
        int left = c == 0 || c == 2 ? x[c] + 6 : x[c + 1] - 8 - width;
        draw_text(left, list.y + 4, headers[c], BLACK);
    }
    fill_rect(x[5], list.y + 2, TASKS_SCROLL_W, TASKS_HEADER_H, FACE);
    gfx_clip(rect(list.x + 2, list.y + 2, list.w - 4, list.h - 4));
    for (int row = 0; row < tasks_rows(t); row++) tasks_draw_row(t, row, x);
    int below = list.y + 2 + TASKS_HEADER_H + tasks_rows(t) * TASKS_ROW_H;
    fill_rect(list.x + 2, below, x[5] - list.x - 2, list.y + list.h - 2 - below, WHITE);
    gfx_clip(rect(0, 0, t->w, t->h));
    t->scroll.r = rect(x[5], list.y + 2 + TASKS_HEADER_H, TASKS_SCROLL_W, list.h - 4 - TASKS_HEADER_H);
    t->scroll.total = t->count;
    t->scroll.visible = tasks_rows(t);
    t->scroll.top = t->top;
    draw_scroll_bar(&t->scroll);
}

static void tasks_draw_end_button(TaskManager *t) {
    Rect end = tasks_end_button(t);
    ProcessInfo *info = tasks_selected_info(t);
    int usable = info && info->pid != 0;
    draw_button(end.x, end.y, end.w, end.h, "", button_down(&t->buttons, 0));
    int shift = button_down(&t->buttons, 0);
    int x = end.x + (end.w - text_width("End Process")) / 2 + shift, y = end.y + 6 + shift;
    if (!usable) draw_text(x + 1, y + 1, "End Process", WHITE);
    draw_text(x, y, "End Process", usable ? BLACK : SHADOW);
}

/* The meter: a column of green bars, lit from the bottom as far as the percent. */
static void tasks_draw_meter(int x, int y, int w, int h, int percent) {
    fill_rect(x, y, w, h, TASKS_GRAPH_BACK);
    char text[8];
    snprintf(text, sizeof text, "%d %%", percent);
    draw_text(x + (w - text_width(text)) / 2, y + h - FONT_HEIGHT - 3, text, TASKS_GRAPH_LINE);
    int top = y + 6, bottom = y + h - FONT_HEIGHT - 8, bars = (bottom - top) / 3;
    int lit = (percent * bars + 50) / 100;
    for (int i = 0; i < bars; i++) {
        uint32_t colour = i < lit ? TASKS_GRAPH_LINE : TASKS_METER_OFF;
        int row = bottom - 2 - i * 3;
        fill_rect(x + 8, row, w / 2 - 9, 2, colour);
        fill_rect(x + w / 2 + 1, row, w / 2 - 9, 2, colour);
    }
}

/* The history: a grid that moves left a step a second, and the busy percent drawn across it. */
static void tasks_draw_graph(TaskManager *t, int x, int y, int w, int h) {
    fill_rect(x, y, w, h, TASKS_GRAPH_BACK);
    gfx_clip(rect(x, y, w, h));
    for (int gy = y + h - 1; gy >= y; gy -= 12) hline(x, gy, w, TASKS_GRAPH_GRID);
    int step_x = max_int(1, (w - 1) / (TASKS_HISTORY - 1));
    int offset = t->scrolled * step_x % 12;
    for (int gx = x + w - 1 - offset; gx >= x; gx -= 12) vline(gx, y, h, TASKS_GRAPH_GRID);
    int right = x + w - 1;
    for (int i = t->samples - 1; i > 0; i--) {
        int x1 = right - (t->samples - 1 - i) * (w - 1) / (TASKS_HISTORY - 1);
        int x0 = right - (t->samples - i) * (w - 1) / (TASKS_HISTORY - 1);
        int y1 = y + h - 2 - t->history[i] * (h - 4) / 100;
        int y0 = y + h - 2 - t->history[i - 1] * (h - 4) / 100;
        draw_line(x0, y0, x1, y1, TASKS_GRAPH_LINE);
        draw_line(x0, y0 - 1, x1, y1 - 1, TASKS_GRAPH_LINE);
    }
    gfx_clip(rect(0, 0, t->w, t->h));
}

static void tasks_draw_performance(TaskManager *t) {
    Rect panel = tasks_panel(t);
    int left = panel.x + 12, top = panel.y + 16;
    int totals_h = 70, graphs_h = max_int(60, panel.h - 16 - 12 - totals_h - 16);
    tasks_group(left, top, 92, graphs_h, "CPU Usage");
    draw_field(left + 10, top + 14, 72, graphs_h - 24, TASKS_GRAPH_BACK);
    tasks_draw_meter(left + 12, top + 16, 68, graphs_h - 28, t->busy);
    int gx = left + 104, gw = panel.x + panel.w - 12 - gx;
    tasks_group(gx, top, gw, graphs_h, "CPU Usage History");
    draw_field(gx + 10, top + 14, gw - 20, graphs_h - 24, TASKS_GRAPH_BACK);
    tasks_draw_graph(t, gx + 12, top + 16, gw - 24, graphs_h - 28);
    int ty = top + graphs_h + 14, tw = panel.x + panel.w - 12 - left;
    tasks_group(left, ty, tw, totals_h, "Totals");
    int windows = 0;
    for (int i = 0; i < t->count; i++) windows += t->list[i].windows;
    char lines[4][40], amount[24];
    snprintf(lines[0], sizeof lines[0], "%d", t->count);
    snprintf(lines[1], sizeof lines[1], "%d", windows);
    tasks_bytes(amount, sizeof amount, tasks_memory_total(t));
    snprintf(lines[2], sizeof lines[2], "%s", amount);
    snprintf(lines[3], sizeof lines[3], "%ld.%ld M/s", t->instructions / 1000000, t->instructions / 100000 % 10);
    static const char *const labels[4] = { "Processes", "Windows", "Memory", "Instructions" };
    int column_w = (tw - 24) / 2;
    for (int i = 0; i < 4; i++) {
        int lx = left + 12 + (i / 2) * column_w, ly = ty + 14 + (i % 2) * 20;
        draw_text(lx, ly, labels[i], BLACK);
        draw_text(lx + column_w - 12 - text_width(lines[i]), ly, lines[i], BLACK);
    }
}

static void tasks_draw_status(TaskManager *t) {
    int y = t->h - TASKS_STATUS_H;
    fill_rect(0, y, t->w, TASKS_STATUS_H, FACE);
    char parts[3][40], amount[24];
    snprintf(parts[0], sizeof parts[0], "Processes: %d", t->count);
    snprintf(parts[1], sizeof parts[1], "CPU Usage: %d%%", t->busy);
    tasks_bytes(amount, sizeof amount, tasks_memory_total(t));
    snprintf(parts[2], sizeof parts[2], "Memory: %s", amount);
    int widths[3] = { 116, 136, t->w - 2 - 116 - 136 - 4 }, x = 2;
    for (int i = 0; i < 3; i++) {
        if (widths[i] < 20) break;
        bevel_thin_sunken(x, y + 2, widths[i] - 2, TASKS_STATUS_H - 4);
        gfx_clip(rect(x + 2, y, widths[i] - 6, TASKS_STATUS_H));
        draw_text(x + 6, y + 4, parts[i], BLACK);
        gfx_clip(rect(0, 0, t->w, t->h));
        x += widths[i];
    }
}

/* The panel's contents, the part that changes every second. */
static void tasks_draw_contents(TaskManager *t) {
    if (t->tab == TASKS_PROCESSES) {
        tasks_draw_list(t);
        tasks_draw_end_button(t);
    } else {
        tasks_draw_performance(t);
    }
}

static void tasks_draw_all(TaskManager *t) {
    os_window_draw(t->win);
    tasks_draw_tabs(t);
    tasks_draw_panel_frame(t);
    tasks_draw_contents(t);
    tasks_draw_status(t);
    os_window_update_all(t->win);
}

/* After the numbers changed: the panel's inside and the status bar. */
static void tasks_draw_numbers(TaskManager *t) {
    os_window_draw(t->win);
    Rect panel = tasks_panel(t);
    if (t->tab == TASKS_PERFORMANCE) fill_rect(panel.x + 2, panel.y + 2, panel.w - 4, panel.h - 4, FACE);
    tasks_draw_contents(t);
    tasks_draw_status(t);
    os_window_update(t->win, panel.x, panel.y, panel.w, panel.h);
    os_window_update(t->win, 0, t->h - TASKS_STATUS_H, t->w, TASKS_STATUS_H);
}

/* ------------------------------------------------------------------ commands */

static void tasks_choose_tab(TaskManager *t, int tab) {
    t->tab = tab;
    t->view_menu[0].flags = tab == TASKS_PROCESSES ? MENU_CHECKED : 0;
    t->view_menu[1].flags = tab == TASKS_PERFORMANCE ? MENU_CHECKED : 0;
}

static void tasks_show(TaskManager *t, int tab) {
    tasks_choose_tab(t, tab);
    tasks_draw_all(t);
}

/* Keeps the selected row in sight. */
static void tasks_follow(TaskManager *t) {
    int index = tasks_selected_index(t);
    if (index < 0) return;
    if (index < t->top) t->top = index;
    if (index >= t->top + tasks_rows(t)) t->top = index - tasks_rows(t) + 1;
}

static void tasks_end_process(TaskManager *t) {
    ProcessInfo *info = tasks_selected_info(t);
    if (!info) {
        os_message_box(t->win, "Task Manager", "Select the process to end first.", MB_OK, MB_INFO);
        return;
    }
    if (info->pid == 0) {
        os_message_box(t->win, "Task Manager",
                       "The System process cannot be ended: it is asmOS itself. Shut Down ends it.", MB_OK,
                       MB_WARNING);
        return;
    }
    int pid = info->pid;
    char question[160];
    snprintf(question, sizeof question,
             "Do you want to end %.31s (PID %d)?\nIts windows close, and what it has not saved is lost.",
             info->name, pid);
    if (os_message_box(t->win, "End Process", question, MB_YES_NO, MB_WARNING) != ID_YES) return;
    if (os_kill(pid) != 0)
        os_message_box(t->win, "Task Manager", "That process has already ended.", MB_OK, MB_INFO);
    tasks_measure(t);
    tasks_draw_all(t);
}

/* New Task: an application by name, as the Run box takes it, or a file to open. */
static void tasks_new(TaskManager *t) {
    char text[128] = "";
    if (!os_input_box(t->win, "Create New Task",
                      "Type the name of an application, such as notepad, or of a file:", text, sizeof text))
        return;
    char *name = text;
    while (*name == ' ') name++;
    for (int end = (int)strlen(name); end > 0 && name[end - 1] == ' '; end--) name[end - 1] = 0;
    if (!name[0]) return;
    int app = os_app_named(name), pid = -1;
    if (app >= 0) pid = os_spawn(app, NULL);
    else if (os_stat(name, NULL) == 1) pid = os_spawn(os_app_for_file(name), name);
    else if (os_stat(name, NULL) == 2) pid = os_spawn(APP_FILES, name);
    if (pid < 0) {
        char message[180];
        snprintf(message, sizeof message,
                 "asmOS cannot find '%.100s'. Type an application's name or a file's path.", name);
        os_message_box(t->win, "Create New Task", message, MB_OK, MB_WARNING);
        return;
    }
    t->selected = pid;
    tasks_measure(t);
    tasks_follow(t);
    tasks_draw_all(t);
}

static void tasks_command(TaskManager *t, int id) {
    switch (id) {
    case TASKS_NEW: tasks_new(t); break;
    case TASKS_EXIT: os_exit(0);
    case TASKS_SHOW_PROCESSES: tasks_show(t, TASKS_PROCESSES); break;
    case TASKS_SHOW_PERFORMANCE: tasks_show(t, TASKS_PERFORMANCE); break;
    case TASKS_REFRESH:
        tasks_measure(t);
        tasks_draw_numbers(t);
        break;
    case TASKS_END: tasks_end_process(t); break;
    case TASKS_ABOUT:
        os_message_box(t->win, "About Task Manager",
                       "Task Manager for asmOS.\nIt counts the instructions each process runs, every second.",
                       MB_OK, MB_INFO);
        break;
    }
}

static void tasks_key(TaskManager *t, const Message *m) {
    if (m->key == KEY_TAB && (m->mods & MOD_CTRL)) {
        tasks_show(t, !t->tab);
        return;
    }
    if (m->key == KEY_F5) {
        tasks_command(t, TASKS_REFRESH);
        return;
    }
    if (t->tab != TASKS_PROCESSES || !t->count) return;
    if (m->key == KEY_DELETE) {
        tasks_end_process(t);
        return;
    }
    int index = tasks_selected_index(t);
    switch (m->key) {
    case KEY_UP: index = index < 0 ? 0 : max_int(0, index - 1); break;
    case KEY_DOWN: index = index < 0 ? 0 : min_int(t->count - 1, index + 1); break;
    case KEY_PAGEUP: index = max_int(0, index - tasks_rows(t)); break;
    case KEY_PAGEDOWN: index = min_int(t->count - 1, max_int(0, index) + tasks_rows(t)); break;
    case KEY_HOME: index = 0; break;
    case KEY_END: index = t->count - 1; break;
    default: return;
    }
    t->selected = t->list[index].pid;
    tasks_follow(t);
    tasks_draw_numbers(t);
}

static void tasks_mouse(TaskManager *t, const Message *m) {
    if (m->type == MSG_MOUSE_DOWN && m->button == BUTTON_LEFT) {
        for (int tab = 0; tab < 2; tab++)
            if (tab != t->tab && rect_contains(tasks_tab_rect(tab), m->x, m->y)) {
                tasks_show(t, tab);
                return;
            }
    }
    if (t->tab != TASKS_PROCESSES) return;
    if (scroll_bar_message(&t->scroll, m)) {
        t->top = t->scroll.top;
        tasks_draw_numbers(t);
        return;
    }
    if (m->type == MSG_MOUSE_WHEEL) {
        t->top = clamp_int(t->top - m->wheel * 3, 0, max_int(0, t->count - tasks_rows(t)));
        tasks_draw_numbers(t);
        return;
    }
    Rect end = tasks_end_button(t);
    int changed, clicked = track_buttons(&t->buttons, &end, 1, m, &changed);
    if (changed) {
        os_window_draw(t->win);
        tasks_draw_end_button(t);
        os_window_update(t->win, end.x, end.y, end.w, end.h);
    }
    if (clicked == 0) {
        tasks_end_process(t);
        return;
    }
    if (m->type != MSG_MOUSE_DOWN || t->buttons.pressed >= 0) return;
    Rect list = tasks_list_rect(t);
    Rect rows = rect(list.x + 2, list.y + 2 + TASKS_HEADER_H, t->scroll.r.x - list.x - 2,
                     tasks_rows(t) * TASKS_ROW_H);
    if (!rect_contains(rows, m->x, m->y)) return;
    int index = t->top + (m->y - rows.y) / TASKS_ROW_H;
    t->selected = index < t->count ? t->list[index].pid : -1;
    tasks_draw_numbers(t);
    if (m->button == BUTTON_RIGHT && t->selected >= 0) {
        int id = os_popup_menu(t->win, m->x, m->y, tasks_context_menu);
        if (id) tasks_command(t, id);
    }
}

static void tasks_relayout(TaskManager *t) {
    os_window_pixels(t->win, &t->w, &t->h);
    t->top = clamp_int(t->top, 0, max_int(0, t->count - tasks_rows(t)));
    tasks_follow(t);
    tasks_draw_all(t);
}

void tasks_main(const char *argument) {
    TaskManager *t = os_alloc(sizeof *t);
    if (!t) os_exit(1);
    memcpy(t->file_menu, tasks_file_template, sizeof t->file_menu);
    memcpy(t->view_menu, tasks_view_template, sizeof t->view_menu);
    memcpy(t->process_menu, tasks_process_template, sizeof t->process_menu);
    memcpy(t->help_menu, tasks_help_template, sizeof t->help_menu);
    t->menus[0] = (Menu){ "File", t->file_menu };
    t->menus[1] = (Menu){ "View", t->view_menu };
    t->menus[2] = (Menu){ "Process", t->process_menu };
    t->menus[3] = (Menu){ "Help", t->help_menu };
    t->menus[4] = (Menu){ NULL, NULL };
    t->selected = -1;
    t->buttons.pressed = -1;
    t->win = os_window_create("Task Manager", &ICON_TASKS, 420, 330, WIN_RESIZABLE);
    if (!t->win) os_exit(1);
    os_window_set_menu(t->win, t->menus);
    os_window_min_size(t->win, 340, 230);
    tasks_sample(t);
    /* `tasks performance` in the Terminal opens the other tab */
    if (argument && !strcmp(argument, "performance")) tasks_choose_tab(t, TASKS_PERFORMANCE);
    tasks_relayout(t);
    os_set_timer(t->win, TASKS_TICK, 1000);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_KEY: tasks_key(t, &m); break;
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: case MSG_MOUSE_WHEEL:
            tasks_mouse(t, &m);
            break;
        case MSG_COMMAND: tasks_command(t, m.id); break;
        case MSG_TIMER:
            tasks_sample(t);
            tasks_draw_numbers(t);
            break;
        case MSG_RESIZE: tasks_relayout(t); break;
        case MSG_SETTINGS: tasks_draw_all(t); break;
        default: os_default_message(&m);
        }
    }
}
