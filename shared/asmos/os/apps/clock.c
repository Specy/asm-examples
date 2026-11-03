/*
 * Clock: the time on an analog face or in large digits, and a stopwatch with laps. The machine
 * keeps Coordinated Universal Time, so that is what it shows; Settings chooses 12 or 24 hours.
 *
 * The face is drawn once into a picture of its own, at the window's size, and drawn again only
 * when the size or the theme changes; every second the hands are drawn over a copy of it. The
 * window can be resized, and the face, the digits and the stopwatch grow with it.
 */

enum { CLOCK_ANALOG, CLOCK_DIGITAL, CLOCK_STOPWATCH };
enum { CLOCK_SHOW_ANALOG = 1, CLOCK_SHOW_DIGITAL, CLOCK_SHOW_STOPWATCH, CLOCK_SECONDS };
enum { CLOCK_TICK = 1, CLOCK_TEXT_H = 52, CLOCK_LAPS = 99, CLOCK_ROW_H = 16, CLOCK_BUTTON_H = 28 };
enum { CLOCK_VIEW_ITEMS = 4, CLOCK_OPTION_ITEMS = 2 };
/* The digits' display: its background, and its segments lit and unlit. */
enum { CLOCK_LCD = 0x0b170b, CLOCK_LIT = 0x50ff50, CLOCK_UNLIT = 0x15301a, CLOCK_DIM = 0x40a040 };

static const MenuItem CLOCK_VIEW_MENU[CLOCK_VIEW_ITEMS] = {
    { "Analog", CLOCK_SHOW_ANALOG, 0, NULL, NULL },
    { "Digital", CLOCK_SHOW_DIGITAL, 0, NULL, NULL },
    { "Stopwatch", CLOCK_SHOW_STOPWATCH, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem CLOCK_OPTIONS_MENU[CLOCK_OPTION_ITEMS] = {
    { "Seconds Hand", CLOCK_SECONDS, MENU_CHECKED, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

static const char *const CLOCK_WEEKDAYS[7] = { "Sunday", "Monday", "Tuesday", "Wednesday",
                                               "Thursday", "Friday", "Saturday" };
static const char *const CLOCK_MONTHS[12] = { "January", "February", "March", "April", "May", "June",
                                              "July", "August", "September", "October", "November",
                                              "December" };
/* The segments of each digit, a to g as bits 0 to 6. */
static const unsigned char CLOCK_SEGMENTS[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };

typedef struct {
    Window *win;
    int w, h;                    /* the client area */
    int view, seconds_hand;
    uint32_t *face;              /* the analog face without its hands */
    int face_w, face_h;
    int shown;                   /* the time shown, in seconds of the day, or -1 */
    /* the stopwatch: running since `since`, after `before` milliseconds of earlier runs */
    int running;
    long since, before;
    long laps[CLOCK_LAPS];       /* the total time at each lap */
    int lap_count;
    char span[24];               /* the stopwatch's time drawn, its segments' length and where */
    int span_length, span_x, span_y;
    ButtonTracker buttons;
    ScrollBar scroll;
    MenuItem view_menu[CLOCK_VIEW_ITEMS], options_menu[CLOCK_OPTION_ITEMS];
    Menu menus[3];
} ClockApp;

static int clock_round(double value) { return (int)(value < 0 ? value - 0.5 : value + 0.5); }

/* ------------------------------------------------------------------ digits of seven segments */

static int clock_thickness(int length) { return max_int(2, (length + 2) / 5); }
static int clock_char_width(char ch, int length, int thickness) {
    return ch == ':' || ch == '.' ? thickness : length + 2 * thickness;
}
static int clock_digits_width(const char *text, int length, int thickness) {
    int width = 0;
    for (; *text; text++) width += clock_char_width(*text, length, thickness) + thickness;
    return max_int(0, width - thickness);
}

/* Draws digits, ':', '.' and ' ' (a digit with no segment lit) with segments `length` long and
 * `thickness` thick. */
static void clock_draw_digits(int x, int y, const char *text, int length, int thickness) {
    int s = length, t = thickness, h = 2 * s + 3 * t;
    for (; *text; x += clock_char_width(*text, s, t) + t, text++) {
        if (*text == ':') {
            fill_rect(x, y + h / 3 - t / 2, t, t, CLOCK_LIT);
            fill_rect(x, y + 2 * h / 3 - t / 2, t, t, CLOCK_LIT);
            continue;
        }
        if (*text == '.') {
            fill_rect(x, y + h - t, t, t, CLOCK_LIT);
            continue;
        }
        int segments = *text >= '0' && *text <= '9' ? CLOCK_SEGMENTS[*text - '0'] : 0;
        const Rect parts[7] = { rect(x + t, y, s, t), rect(x + t + s, y + t, t, s),
                                rect(x + t + s, y + 2 * t + s, t, s), rect(x + t, y + 2 * t + 2 * s, s, t),
                                rect(x, y + 2 * t + s, t, s), rect(x, y + t, t, s), rect(x + t, y + t + s, s, t) };
        for (int i = 0; i < 7; i++)
            fill_rect(parts[i].x, parts[i].y, parts[i].w, parts[i].h, segments >> i & 1 ? CLOCK_LIT : CLOCK_UNLIT);
    }
}

/* The longest segments with which the text fits in a rectangle, and where it starts in its
 * middle. */
static int clock_fit_digits(Rect area, const char *text, int *x, int *y) {
    int best = 3;
    for (int s = 4; s < 400; s++) {
        int t = clock_thickness(s);
        if (clock_digits_width(text, s, t) > area.w || 2 * s + 3 * t > area.h) break;
        best = s;
    }
    int t = clock_thickness(best);
    *x = area.x + (area.w - clock_digits_width(text, best, t)) / 2;
    *y = area.y + (area.h - 2 * best - 3 * t) / 2;
    return best;
}

/* ------------------------------------------------------------------ words */

static void clock_format_time(const DateTime *now, int seconds, char *text, int size) {
    if (os_settings()->clock_24h) {
        if (seconds) snprintf(text, size, "%02d:%02d:%02d UTC", now->hour, now->minute, now->second);
        else snprintf(text, size, "%02d:%02d UTC", now->hour, now->minute);
    } else {
        int hour = now->hour % 12 ? now->hour % 12 : 12;
        const char *half = now->hour < 12 ? "AM" : "PM";
        if (seconds) snprintf(text, size, "%d:%02d:%02d %s UTC", hour, now->minute, now->second, half);
        else snprintf(text, size, "%d:%02d %s UTC", hour, now->minute, half);
    }
}

/* The date, in full when it fits in `width` pixels, shortened when it does not. */
static void clock_format_date(const DateTime *now, int width, char *text, int size) {
    const char *weekday = CLOCK_WEEKDAYS[now->weekday % 7], *month = CLOCK_MONTHS[(now->month + 11) % 12];
    snprintf(text, size, "%s %d %s %d", weekday, now->day, month, now->year);
    if (text_width(text) > width) snprintf(text, size, "%.3s %d %.3s %d", weekday, now->day, month, now->year);
}

/* The stopwatch's time: minutes, seconds and hundredths, or hours first after the first hour. */
static void clock_format_span(long ms, char *text, int size) {
    long hundredths = ms / 10;
    int hours = (int)(hundredths / 360000), minutes = (int)(hundredths / 6000 % 60);
    int seconds = (int)(hundredths / 100 % 60), part = (int)(hundredths % 100);
    if (hours) snprintf(text, size, "%d:%02d:%02d.%d", hours, minutes, seconds, part / 10);
    else snprintf(text, size, "%02d:%02d.%02d", minutes, seconds, part);
}

/* ------------------------------------------------------------------ the analog face */

static void clock_geometry(const ClockApp *c, int *cx, int *cy, int *radius) {
    *cx = c->face_w / 2;
    *cy = c->face_h / 2;
    *radius = max_int(16, min_int(c->face_w, c->face_h) / 2 - 8);
}

/* The face without its hands, into a picture of its own, for the window's size. */
static void clock_make_face(ClockApp *c) {
    os_free(c->face);
    c->face_w = c->w;
    c->face_h = max_int(1, c->h - CLOCK_TEXT_H);
    c->face = os_alloc((size_t)c->face_w * c->face_h * 4);
    if (!c->face) return;
    GfxContext saved = gfx_save();
    gfx_target(c->face, c->face_w, c->face_h, c->face_w);
    int cx, cy, r;
    clock_geometry(c, &cx, &cy, &r);
    double k = r / 92.0;
    fill_rect(0, 0, c->face_w, c->face_h, FACE);
    fill_circle(cx, cy, r + 4, SHADOW);
    fill_circle(cx, cy, r + 2, os_theme_colour(THEME_TITLE));
    fill_circle(cx, cy, r - 2, WHITE);
    for (int mark = 0; mark < 60; mark++) {
        double angle = mark * M_PI / 30, dx = sin(angle), dy = -cos(angle);
        double outer = r - 6 * k, inner = mark % 5 ? r - 10 * k : r - 18 * k;
        int x0 = cx + clock_round(dx * inner), y0 = cy + clock_round(dy * inner);
        int x1 = cx + clock_round(dx * outer), y1 = cy + clock_round(dy * outer);
        if (mark % 5) draw_line(x0, y0, x1, y1, SHADOW);
        else thick_line(x0, y0, x1, y1, max_int(2, clock_round(3 * k)), BLACK);
    }
    if (r >= 56) {
        for (int hour = 1; hour <= 12; hour++) {
            double angle = hour * M_PI / 6;
            char label[3];
            snprintf(label, sizeof label, "%d", hour);
            int x = cx + clock_round(sin(angle) * (r - 30 * k)) - text_width(label) / 2;
            int y = cy - clock_round(cos(angle) * (r - 30 * k)) - FONT_HEIGHT / 2;
            if (r >= 120) draw_text_bold(x, y, label, NAVY);
            else draw_text(x, y, label, NAVY);
        }
    }
    if (r >= 80) draw_text(cx - text_width("UTC") / 2, cy + clock_round(r * 0.36), "UTC", SHADOW);
    gfx_restore(saved);
}

/* A hand: `turn` of the way round from twelve, `length` out and `tail` back past the middle. */
static void clock_draw_hand(int cx, int cy, double turn, double length, double tail, int width, uint32_t colour) {
    double angle = turn * 2 * M_PI, dx = sin(angle), dy = -cos(angle);
    thick_line(cx - clock_round(dx * tail), cy - clock_round(dy * tail), cx + clock_round(dx * length),
               cy + clock_round(dy * length), width, colour);
}

static void clock_draw_analog(ClockApp *c, const DateTime *now) {
    if (!c->face || c->face_w != c->w || c->face_h != max_int(1, c->h - CLOCK_TEXT_H)) clock_make_face(c);
    os_window_draw(c->win);
    if (c->face) blit(c->face, c->face_w, 0, 0, c->face_w, c->face_h);
    int cx, cy, r;
    clock_geometry(c, &cx, &cy, &r);
    double minutes = now->minute + (c->seconds_hand ? now->second / 60.0 : 0);
    clock_draw_hand(cx, cy, (now->hour % 12 + minutes / 60) / 12, r * 0.52, r * 0.09, max_int(3, r / 16), BLACK);
    clock_draw_hand(cx, cy, minutes / 60, r * 0.78, r * 0.11, max_int(2, r / 28), BLACK);
    if (c->seconds_hand) clock_draw_hand(cx, cy, now->second / 60.0, r * 0.86, r * 0.18, max_int(1, r / 70), RED);
    fill_circle(cx, cy, max_int(2, r / 22), c->seconds_hand ? RED : BLACK);

    int y = c->face_h, field = min_int(c->w - 16, 170);
    char time[32], date[48];
    fill_rect(0, y, c->w, c->h - y, FACE);
    clock_format_time(now, c->seconds_hand, time, sizeof time);
    clock_format_date(now, c->w - 8, date, sizeof date);
    draw_field((c->w - field) / 2, y + 2, field, 22, CLOCK_LCD);
    draw_text_bold((c->w - text_width(time)) / 2, y + 7, time, CLOCK_LIT);
    draw_text((c->w - text_width(date)) / 2, y + 31, date, BLACK);
    os_window_update_all(c->win);
}

/* ------------------------------------------------------------------ the digital clock */

static void clock_draw_digital(ClockApp *c, const DateTime *now) {
    os_window_draw(c->win);
    fill_rect(0, 0, c->w, c->h, FACE);
    Rect panel = rect(6, 6, c->w - 12, c->h - 12);
    draw_field(panel.x, panel.y, panel.w, panel.h, CLOCK_LCD);
    char digits[16], date[48], zone[16];
    int twelve = !os_settings()->clock_24h, hour = twelve ? (now->hour % 12 ? now->hour % 12 : 12) : now->hour;
    snprintf(digits, sizeof digits, twelve ? "%2d:%02d:%02d" : "%02d:%02d:%02d", hour, now->minute, now->second);
    snprintf(zone, sizeof zone, "%s%s", twelve ? (now->hour < 12 ? "AM  " : "PM  ") : "", "UTC");
    clock_format_date(now, panel.w - 16, date, sizeof date);
    int words = panel.h >= 100 ? 2 * 16 + 4 : 0;
    Rect area = rect(panel.x + 12, panel.y + 10, panel.w - 24, panel.h - 20 - words);
    int x, y, length = clock_fit_digits(area, digits, &x, &y);
    clock_draw_digits(x, y, digits, length, clock_thickness(length));
    if (words) {
        int y = panel.y + panel.h - 8 - words;
        draw_text((c->w - text_width(date)) / 2, y, date, CLOCK_DIM);
        draw_text_bold((c->w - text_width(zone)) / 2, y + 18, zone, CLOCK_DIM);
    }
    os_window_update_all(c->win);
}

/* ------------------------------------------------------------------ the stopwatch */

static long clock_elapsed(const ClockApp *c) { return c->before + (c->running ? os_now_ms() - c->since : 0); }

static Rect clock_panel(const ClockApp *c) { return rect(8, 8, c->w - 16, clamp_int(c->h / 4, 48, 110)); }
static Rect clock_button(const ClockApp *c, int index) {
    int y = clock_panel(c).y + clock_panel(c).h + 8, w = (c->w - 24) / 2;
    return rect(8 + index * (w + 8), y, w, CLOCK_BUTTON_H);
}
static Rect clock_list(const ClockApp *c) {
    int y = clock_button(c, 0).y + CLOCK_BUTTON_H + 8;
    return rect(8, y, c->w - 16, c->h - 8 - y);
}
static int clock_list_rows(const ClockApp *c) { return max_int(1, (clock_list(c).h - 6) / CLOCK_ROW_H); }

/* The stopwatch's time. While it runs, only the digits that changed are drawn again. */
static void clock_draw_span(ClockApp *c, int whole) {
    Rect panel = clock_panel(c);
    char text[24];
    clock_format_span(clock_elapsed(c), text, sizeof text);
    os_window_draw(c->win);
    if (whole || strlen(text) != strlen(c->span)) {
        draw_field(panel.x, panel.y, panel.w, panel.h, CLOCK_LCD);
        Rect area = rect(panel.x + 10, panel.y + 8, panel.w - 20, panel.h - 16);
        c->span_length = clock_fit_digits(area, text, &c->span_x, &c->span_y);
        clock_draw_digits(c->span_x, c->span_y, text, c->span_length, clock_thickness(c->span_length));
        os_window_update(c->win, panel.x, panel.y, panel.w, panel.h);
        copy_text(c->span, sizeof c->span, text);
        return;
    }
    int s = c->span_length, t = clock_thickness(s), x = c->span_x;
    Rect dirty = rect(0, 0, 0, 0);
    for (int i = 0; text[i]; x += clock_char_width(text[i], s, t) + t, i++) {
        if (text[i] == c->span[i]) continue;
        char one[2] = { text[i], 0 };
        Rect r = rect(x, c->span_y, clock_char_width(text[i], s, t), 2 * s + 3 * t);
        fill_rect(r.x, r.y, r.w, r.h, CLOCK_LCD);
        clock_draw_digits(x, c->span_y, one, s, t);
        dirty = rect_union(dirty, r);
    }
    if (!rect_empty(dirty)) os_window_update(c->win, dirty.x, dirty.y, dirty.w, dirty.h);
    copy_text(c->span, sizeof c->span, text);
}

static void clock_draw_buttons(ClockApp *c) {
    const char *labels[2] = { c->running ? "Stop" : "Start", c->running ? "Lap" : "Reset" };
    for (int i = 0; i < 2; i++) {
        Rect r = clock_button(c, i);
        draw_button(r.x, r.y, r.w, r.h, labels[i], button_down(&c->buttons, i));
    }
}

/* The laps, newest first: each lap's own time, and the total at its end. With three laps or
 * more, the fastest is green and the slowest red. */
static void clock_draw_laps(ClockApp *c) {
    Rect list = clock_list(c);
    if (list.h < 24) return;
    draw_field(list.x, list.y, list.w, list.h, WHITE);
    int rows = clock_list_rows(c), scrolling = c->lap_count > rows;
    int width = list.w - 4 - (scrolling ? 16 : 0);
    c->scroll.r = rect(list.x + list.w - 18, list.y + 2, 16, list.h - 4);
    c->scroll.total = c->lap_count;
    c->scroll.visible = rows;
    c->scroll.top = clamp_int(c->scroll.top, 0, scroll_max(&c->scroll));
    long fastest = -1, slowest = -1;
    for (int i = 0; i < c->lap_count && c->lap_count >= 3; i++) {
        long lap = c->laps[i] - (i ? c->laps[i - 1] : 0);
        if (fastest < 0 || lap < fastest) fastest = lap;
        if (slowest < 0 || lap > slowest) slowest = lap;
    }
    gfx_clip(rect(list.x + 2, list.y + 2, width, list.h - 4));
    if (!c->lap_count) draw_text(list.x + 8, list.y + 5, "Lap times show here.", SHADOW);
    for (int row = 0; row < rows; row++) {
        int index = c->lap_count - 1 - c->scroll.top - row;
        if (index < 0) break;
        int y = list.y + 3 + row * CLOCK_ROW_H, right = list.x + 2 + width - 6;
        long total = c->laps[index], lap = total - (index ? c->laps[index - 1] : 0);
        char name[16], lap_text[24], total_text[24];
        snprintf(name, sizeof name, "Lap %d", index + 1);
        clock_format_span(lap, lap_text, sizeof lap_text);
        clock_format_span(total, total_text, sizeof total_text);
        uint32_t ink = lap == fastest ? GREEN : lap == slowest ? 0xc00000 : BLACK;
        if (row % 2) fill_rect(list.x + 2, y - 1, width, CLOCK_ROW_H, 0xf0f0f0);
        draw_text(list.x + 8, y + 1, name, BLACK);
        if (width >= 210) {
            draw_text(right - text_width(total_text), y + 1, total_text, SHADOW);
            right -= text_width(total_text) + 12;
        }
        draw_text(right - text_width(lap_text), y + 1, lap_text, ink);
    }
    gfx_clip(rect(0, 0, c->w, c->h));
    if (scrolling) draw_scroll_bar(&c->scroll);
}

static void clock_draw_stopwatch(ClockApp *c) {
    os_window_draw(c->win);
    fill_rect(0, 0, c->w, c->h, FACE);
    clock_draw_buttons(c);
    clock_draw_laps(c);
    os_window_update_all(c->win);
    clock_draw_span(c, 1);
}

static void clock_start_stop(ClockApp *c) {
    if (c->running) c->before += os_now_ms() - c->since;
    else c->since = os_now_ms();
    c->running = !c->running;
}

static void clock_lap_reset(ClockApp *c) {
    if (c->running) {
        if (c->lap_count == CLOCK_LAPS) {
            os_beep();
            return;
        }
        c->laps[c->lap_count++] = clock_elapsed(c);
    } else {
        c->before = 0;
        c->lap_count = 0;
    }
    c->scroll.top = 0;
}

/* ------------------------------------------------------------------ the window */

/* The timer: four times a second for the clocks, which draw when the time shown changes, twenty
 * times for a running stopwatch, and none for a stopped one. */
static void clock_set_timer(ClockApp *c) {
    int ms = c->view != CLOCK_STOPWATCH ? 250 : c->running ? 50 : 0;
    os_set_timer(c->win, CLOCK_TICK, ms);
}

static void clock_draw_all(ClockApp *c) {
    DateTime now;
    os_time(&now);
    c->shown = now.hour * 3600 + now.minute * 60 + now.second;
    if (c->view == CLOCK_ANALOG) clock_draw_analog(c, &now);
    else if (c->view == CLOCK_DIGITAL) clock_draw_digital(c, &now);
    else clock_draw_stopwatch(c);
}

static void clock_tick(ClockApp *c) {
    if (c->view == CLOCK_STOPWATCH) {
        if (c->running) clock_draw_span(c, 0);
        return;
    }
    DateTime now;
    os_time(&now);
    int shown = now.hour * 3600 + now.minute * 60 + (c->view == CLOCK_ANALOG && !c->seconds_hand ? 0 : now.second);
    if (shown != c->shown) clock_draw_all(c);
}

static void clock_check_menus(ClockApp *c) {
    for (int view = 0; view < 3; view++) c->view_menu[view].flags = view == c->view ? MENU_CHECKED : 0;
    c->options_menu[0].flags = c->seconds_hand ? MENU_CHECKED : 0;
}

static void clock_command(ClockApp *c, int id) {
    if (id >= CLOCK_SHOW_ANALOG && id <= CLOCK_SHOW_STOPWATCH) c->view = id - CLOCK_SHOW_ANALOG;
    else if (id == CLOCK_SECONDS) c->seconds_hand = !c->seconds_hand;
    else return;
    clock_check_menus(c);
    clock_set_timer(c);
    clock_draw_all(c);
}

static void clock_mouse(ClockApp *c, const Message *m) {
    if (c->view != CLOCK_STOPWATCH) return;
    if (c->lap_count > clock_list_rows(c) && scroll_bar_message(&c->scroll, m)) {
        os_window_draw(c->win);
        clock_draw_laps(c);
        Rect list = clock_list(c);
        os_window_update(c->win, list.x, list.y, list.w, list.h);
        return;
    }
    if (m->type == MSG_MOUSE_WHEEL && rect_contains(clock_list(c), m->x, m->y)) {
        c->scroll.top -= m->wheel * 3;
        clock_draw_stopwatch(c);
        return;
    }
    Rect buttons[2] = { clock_button(c, 0), clock_button(c, 1) };
    int changed, clicked = track_buttons(&c->buttons, buttons, 2, m, &changed);
    if (clicked == 0) clock_start_stop(c);
    if (clicked == 1) clock_lap_reset(c);
    if (clicked >= 0) {
        clock_set_timer(c);
        clock_draw_stopwatch(c);
    } else if (changed) {
        os_window_draw(c->win);
        clock_draw_buttons(c);
        os_window_update(c->win, buttons[0].x, buttons[0].y, c->w - 16, CLOCK_BUTTON_H);
    }
}

/* Space starts and stops the stopwatch, Enter takes a lap or resets it. */
static void clock_key(ClockApp *c, const Message *m) {
    if (c->view != CLOCK_STOPWATCH) return;
    if (m->key == KEY_SPACE) clock_start_stop(c);
    else if (m->key == KEY_ENTER || m->key == KEY_KPENTER) clock_lap_reset(c);
    else return;
    clock_set_timer(c);
    clock_draw_stopwatch(c);
}

void clock_main(const char *argument) {
    (void)argument;
    ClockApp *c = os_alloc(sizeof *c);
    if (!c) os_exit(1);
    memcpy(c->view_menu, CLOCK_VIEW_MENU, sizeof c->view_menu);
    memcpy(c->options_menu, CLOCK_OPTIONS_MENU, sizeof c->options_menu);
    c->menus[0] = (Menu){ "View", c->view_menu };
    c->menus[1] = (Menu){ "Options", c->options_menu };
    c->seconds_hand = 1;
    c->buttons.pressed = -1;
    c->shown = -1;
    clock_check_menus(c);
    c->win = os_window_create("Clock", &ICON_CLOCK, 240, 290, WIN_RESIZABLE);
    if (!c->win) os_exit(1);
    os_window_set_menu(c->win, c->menus);
    os_window_min_size(c->win, 180, 200);
    os_window_pixels(c->win, &c->w, &c->h);
    clock_set_timer(c);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: case MSG_MOUSE_WHEEL:
            clock_mouse(c, &m);
            break;
        case MSG_KEY: clock_key(c, &m); break;
        case MSG_COMMAND: clock_command(c, m.id); break;
        case MSG_TIMER: clock_tick(c); break;
        case MSG_RESIZE:
            os_window_pixels(c->win, &c->w, &c->h);
            clock_draw_all(c);
            break;
        case MSG_SETTINGS:
            /* the theme colours the face's ring */
            os_free(c->face);
            c->face = NULL;
            clock_draw_all(c);
            break;
        default: os_default_message(&m);
        }
    }
}
