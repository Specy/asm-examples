/*
 * About asmOS: the system's name and what it is, and what it is doing now: the processes, the
 * memory they hold, the uptime, the screen and how busy the processor is, counted again every
 * second.
 */

enum { ABOUT_W = 420, ABOUT_H = 268, ABOUT_TICK = 1, ABOUT_INFO_Y = 94, ABOUT_INFO_ROW = 17, ABOUT_ROWS = 6 };
enum { ABOUT_MAX_PROCESSES = 32 };

typedef struct {
    Window *win;
    ButtonTracker buttons;
    ProcessInfo list[ABOUT_MAX_PROCESSES];
} About;

static Rect about_ok(void) { return rect(ABOUT_W - 92, ABOUT_H - 34, 80, 24); }

/* A size in bytes as people read it: "812 bytes", "34 KB", "1.5 MB". */
static void about_bytes(char *text, size_t size, long bytes) {
    if (bytes < 1024) snprintf(text, size, "%ld bytes", bytes);
    else if (bytes < 1024 * 1024) snprintf(text, size, "%ld KB", (bytes + 512) / 1024);
    else snprintf(text, size, "%ld.%ld MB", bytes / (1024 * 1024), bytes % (1024 * 1024) * 10 / (1024 * 1024));
}

/* The lines of what the system is doing, each a label and a value. */
static void about_draw_info(About *about) {
    int count = min_int(os_processes(about->list, ABOUT_MAX_PROCESSES), ABOUT_MAX_PROCESSES);
    long memory = 0;
    int windows = 0;
    for (int i = 0; i < count; i++) {
        memory += about->list[i].memory;
        windows += about->list[i].windows;
    }
    int busy;
    long instructions;
    os_cpu_load(&busy, &instructions);
    long seconds = os_now_ms() / 1000;
    char values[ABOUT_ROWS][64], amount[32];
    snprintf(values[0], sizeof values[0], "%d running, with %d window%s open", count, windows,
             windows == 1 ? "" : "s");
    about_bytes(amount, sizeof amount, memory);
    snprintf(values[1], sizeof values[1], "%s held by the processes", amount);
    snprintf(values[2], sizeof values[2], "%ld:%02ld:%02ld", seconds / 3600, seconds / 60 % 60, seconds % 60);
    snprintf(values[3], sizeof values[3], "%d x %d pixels, 32-bit colour", screen_w, screen_h);
    snprintf(values[4], sizeof values[4], "x86-64, emulated by Blink");
    snprintf(values[5], sizeof values[5], "%d%% busy, %ld.%ld million instructions/s", busy, instructions / 1000000,
             instructions / 100000 % 10);
    static const char *const labels[ABOUT_ROWS] = { "Processes:", "Memory:", "Uptime:", "Screen:", "Processor:",
                                                    "Load:" };
    fill_rect(14, ABOUT_INFO_Y, ABOUT_W - 28, ABOUT_ROWS * ABOUT_INFO_ROW, FACE);
    for (int i = 0; i < ABOUT_ROWS; i++) {
        draw_text(16, ABOUT_INFO_Y + 2 + i * ABOUT_INFO_ROW, labels[i], SHADOW);
        draw_text(104, ABOUT_INFO_Y + 2 + i * ABOUT_INFO_ROW, values[i], BLACK);
    }
}

static void about_draw_ok(About *about) {
    Rect ok = about_ok();
    fill_rect(ok.x - 1, ok.y - 1, ok.w + 2, ok.h + 2, FACE);
    draw_button(ok.x, ok.y, ok.w, ok.h, "OK", button_down(&about->buttons, 0));
    if (os_window_focused(about->win)) frame_rect(ok.x - 1, ok.y - 1, ok.w + 2, ok.h + 2, BLACK);
}

static void about_draw_all(About *about) {
    os_window_draw(about->win);
    fill_rect(0, 0, ABOUT_W, ABOUT_H, FACE);
    draw_icon(&ICON_CHIP, 10, 6, 4);
    draw_text_bold(86, 14, "asmOS 2.0", NAVY);
    static const char *const description[] = {
        "A small desktop operating system in C,",
        "with its pixel loops in x86 assembly.",
        "Each application is its own process.",
    };
    for (int i = 0; i < 3; i++) draw_text(86, 34 + i * 14, description[i], BLACK);
    hline(12, 84, ABOUT_W - 24, SHADOW);
    hline(12, 85, ABOUT_W - 24, WHITE);
    about_draw_info(about);
    hline(12, ABOUT_INFO_Y + ABOUT_ROWS * ABOUT_INFO_ROW + 4, ABOUT_W - 24, SHADOW);
    hline(12, ABOUT_INFO_Y + ABOUT_ROWS * ABOUT_INFO_ROW + 5, ABOUT_W - 24, WHITE);
    draw_text(16, ABOUT_H - 36, "One Linux program on the emulated", SHADOW);
    draw_text(16, ABOUT_H - 22, "machine; its files are the Project's.", SHADOW);
    about_draw_ok(about);
    os_window_update_all(about->win);
}

void about_main(const char *argument) {
    (void)argument;
    About *about = os_alloc(sizeof *about);
    if (!about) os_exit(1);
    about->buttons.pressed = -1;
    about->win = os_window_create("About asmOS", &ICON_COMPUTER, ABOUT_W, ABOUT_H, WIN_CENTER | WIN_NO_MINIMIZE);
    if (!about->win) os_exit(1);
    about_draw_all(about);
    os_set_timer(about->win, ABOUT_TICK, 1000);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_MOUSE_DOWN: case MSG_MOUSE_MOVE: case MSG_MOUSE_UP: {
            Rect ok = about_ok();
            int changed, clicked = track_buttons(&about->buttons, &ok, 1, &m, &changed);
            if (clicked == 0) os_exit(0);
            if (changed) {
                os_window_draw(about->win);
                about_draw_ok(about);
                os_window_update(about->win, ok.x - 1, ok.y - 1, ok.w + 2, ok.h + 2);
            }
            break;
        }
        case MSG_KEY:
            if (m.key == KEY_ENTER || m.key == KEY_KPENTER || m.key == KEY_ESC || m.key == KEY_SPACE) os_exit(0);
            break;
        case MSG_TIMER:
            os_window_draw(about->win);
            about_draw_info(about);
            os_window_update(about->win, 14, ABOUT_INFO_Y, ABOUT_W - 28, ABOUT_ROWS * ABOUT_INFO_ROW);
            break;
        case MSG_FOCUS: case MSG_BLUR: case MSG_SETTINGS: about_draw_all(about); break;
        case MSG_CLOSE: os_exit(0);
        default: os_default_message(&m);
        }
    }
}
