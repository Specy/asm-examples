/*
 * Settings: the desktop's wallpaper, the windows' colour theme, the double-click speed, the
 * taskbar clock's 24 hours and the sounds. What the dialog shows is a copy of the Settings, which
 * OK and Apply hand to os_apply_settings: it saves them in asmos.ini and tells every window.
 *
 * Settings is single-instance: a second one finds the first in the process list, says so and ends.
 * It still keeps its state in memory of its own, as every application does.
 */

enum { SETTINGS_W = 420, SETTINGS_H = 370 };
enum { SETTINGS_OK, SETTINGS_CANCEL, SETTINGS_APPLY, SETTINGS_BROWSE, SETTINGS_BUTTONS };
enum { SETTINGS_SLIDER_X = 176, SETTINGS_SLIDER_W = 150, SETTINGS_SLIDER_Y = 280, SETTINGS_STEP = 50 };
enum { SETTINGS_PREVIEW_W = 84, SETTINGS_PREVIEW_H = 63 };

static const char *const settings_wallpapers[WALLPAPER_COUNT] = {
    "Teal", "Blue", "Green", "Plum", "Gradient", "Pattern", "Picture"
};
static const char *const settings_themes[THEME_COUNT] = { "Classic", "Olive", "Plum", "Slate" };
/* The same colours the kernel's themes have, for the preview of a theme not yet applied: the
 * active title bar's two ends, the inactive one's, and the selection. */
static const uint32_t settings_theme_colours[THEME_COUNT][5] = {
    [THEME_CLASSIC] = { 0x000080, 0x1084d0, 0x808080, 0xb4b4b4, 0x000080 },
    [THEME_OLIVE] = { 0x385020, 0x8fa860, 0x7d7d6e, 0xb0b09a, 0x506030 },
    [THEME_PLUM] = { 0x581848, 0xb05890, 0x806878, 0xb8a8b0, 0x702060 },
    [THEME_SLATE] = { 0x203040, 0x6888a8, 0x707880, 0xa8b0b8, 0x305070 },
};
/* The plain wallpapers' colours, as the desktop paints them. */
static const uint32_t settings_wallpaper_colours[4] = { 0x008080, 0x204880, 0x306030, 0x503050 };

typedef struct {
    Window *win;
    Settings chosen;           /* what the dialog shows */
    uint32_t *picture;         /* the chosen picture, for the preview, or NULL */
    int picture_w, picture_h;
    char picture_path[64];     /* the path `picture` was read from */
    ButtonTracker buttons;
    int sliding;               /* the slider's thumb is held */
} SettingsApp;

/* ------------------------------------------------------------------ layout */

static Rect settings_button_rect(int which) {
    switch (which) {
    case SETTINGS_OK: return rect(SETTINGS_W - 3 * 84 - 4, SETTINGS_H - 32, 78, 24);
    case SETTINGS_CANCEL: return rect(SETTINGS_W - 2 * 84 - 4, SETTINGS_H - 32, 78, 24);
    case SETTINGS_APPLY: return rect(SETTINGS_W - 84 - 4, SETTINGS_H - 32, 78, 24);
    default: return rect(224, 104, 72, 22);
    }
}
static Rect settings_wallpaper_rect(int which) {
    return rect(22 + which / 4 * 96, 30 + which % 4 * 18, 88, 16);
}
static Rect settings_theme_rect(int which) { return rect(22, 164 + which * 18, 88, 16); }
static Rect settings_clock_rect(void) { return rect(22, 306, 130, 16); }
static Rect settings_sounds_rect(void) { return rect(176, 306, 120, 16); }
static Rect settings_slider_rect(void) {
    return rect(SETTINGS_SLIDER_X - 6, SETTINGS_SLIDER_Y - 10, SETTINGS_SLIDER_W + 12, 22);
}

static int settings_slider_x(int ms) {
    return SETTINGS_SLIDER_X + (ms - 200) * SETTINGS_SLIDER_W / 700;
}
static int settings_slider_value(int x) {
    int ms = 200 + (x - SETTINGS_SLIDER_X) * 700 / SETTINGS_SLIDER_W;
    ms = (ms + SETTINGS_STEP / 2) / SETTINGS_STEP * SETTINGS_STEP;
    return clamp_int(ms, 200, 900);
}

static int settings_same(const Settings *a, const Settings *b) {
    return a->wallpaper == b->wallpaper && !strcmp(a->picture, b->picture) && a->theme == b->theme &&
           a->double_click_ms == b->double_click_ms && a->clock_24h == b->clock_24h && a->sounds == b->sounds;
}
static int settings_changed(SettingsApp *s) { return !settings_same(&s->chosen, os_settings()); }

/* Reads the chosen picture again if its path changed, for the preview. */
static void settings_load_picture(SettingsApp *s) {
    if (s->picture && !strcmp(s->picture_path, s->chosen.picture)) return;
    os_free(s->picture);
    s->picture = NULL;
    copy_text(s->picture_path, sizeof s->picture_path, s->chosen.picture);
    long size;
    char *data = os_read_file(s->chosen.picture, &size);
    if (!data) return;
    s->picture = os_bmp_decode(data, size, &s->picture_w, &s->picture_h);
    os_free(data);
}

/* ------------------------------------------------------------------ drawing */

/* An etched frame with a caption in its top line, as dialogs group their parts. */
static void settings_group(int x, int y, int w, int h, const char *caption) {
    frame_rect(x + 1, y + 1, w - 1, h - 1, WHITE);
    frame_rect(x, y, w - 1, h - 1, SHADOW);
    fill_rect(x + 8, y - 6, text_width(caption) + 6, FONT_HEIGHT, FACE);
    draw_text(x + 11, y - 6, caption, BLACK);
}

/* A small monitor with the chosen wallpaper and a strip of taskbar on its screen. */
static void settings_draw_monitor(SettingsApp *s, int x, int y) {
    int sw = SETTINGS_PREVIEW_W, sh = SETTINGS_PREVIEW_H;
    fill_rect(x, y, sw + 14, sh + 14, FACE);
    bevel_raised(x, y, sw + 14, sh + 14);
    bevel_thin_sunken(x + 5, y + 5, sw + 4, sh + 4);
    fill_rect(x + sw - 2, y + sh + 9, 6, 2, 0x00c000);
    fill_rect(x + (sw + 14) / 2 - 12, y + sh + 14, 24, 6, FACE);
    bevel_thin_raised(x + (sw + 14) / 2 - 12, y + sh + 14, 24, 6);
    fill_rect(x + (sw + 14) / 2 - 24, y + sh + 20, 48, 5, FACE);
    bevel_raised(x + (sw + 14) / 2 - 24, y + sh + 20, 48, 5);
    int px = x + 7, py = y + 7, desk = sh - 5;
    gfx_clip(rect(px, py, sw, sh));
    int which = s->chosen.wallpaper;
    if (which < 4) {
        fill_rect(px, py, sw, desk, settings_wallpaper_colours[which]);
        draw_icon(&ICON_CHIP, px + sw / 2 - 4, py + desk / 2 - 10, 1);
    } else if (which == WALLPAPER_GRADIENT) {
        for (int row = 0; row < desk; row++) {
            int t = row * 255 / desk;
            hline(px, py + row, sw, (uint32_t)((0x10 + t / 8) << 16 | (0x50 - t / 6) << 8 | (0x90 - t / 3)));
        }
    } else if (which == WALLPAPER_PATTERN) {
        fill_rect(px, py, sw, desk, 0x007070);
        for (int row = 0; row < desk; row += 4)
            for (int column = 0; column < sw; column += 4)
                if (((row ^ column) >> 2) & 1) fill_rect(px + column + 1, py + row + 1, 2, 2, 0x109090);
    } else if (s->picture) {
        blit_scaled(s->picture, s->picture_w, s->picture_w, s->picture_h, px, py, sw, desk);
    } else {
        fill_rect(px, py, sw, desk, BLACK);
        draw_text(px + (sw - text_width("No picture")) / 2, py + desk / 2 - 7, "No picture", WHITE);
    }
    /* three desktop icons and the taskbar */
    for (int i = 0; i < 3; i++) fill_rect(px + 3, py + 3 + i * 9, 5, 5, i == 1 ? YELLOW : WHITE);
    fill_rect(px, py + desk, sw, 5, FACE);
    hline(px, py + desk, sw, WHITE);
    fill_rect(px + 1, py + desk + 1, 9, 3, SHADOW);
    gfx_clip(rect(0, 0, SETTINGS_W, SETTINGS_H));
}

/* A window drawn small: its frame, a title bar in the theme's colours, and a line of its text. */
static void settings_draw_window(int x, int y, int w, int h, const char *title, int active,
                                 const uint32_t *colours) {
    fill_rect(x, y, w, h, FACE);
    bevel_frame(x, y, w, h);
    gradient_rect(x + 4, y + 4, w - 8, 18, colours[active ? 0 : 2], colours[active ? 1 : 3]);
    draw_text_bold(x + 8, y + 7, title, active ? WHITE : LIGHT);
    draw_field(x + 4, y + 24, w - 8, h - 28, WHITE);
    draw_text(x + 10, y + 29, "Normal", BLACK);
    if (active) {
        int width = text_width("Selected");
        fill_rect(x + 70, y + 27, width + 6, FONT_HEIGHT + 4, colours[4]);
        draw_text(x + 73, y + 29, "Selected", WHITE);
    }
}

static void settings_draw_theme_preview(SettingsApp *s) {
    const uint32_t *colours = settings_theme_colours[s->chosen.theme];
    int x = 124, y = 158, w = 280, h = 80;
    uint32_t desk = s->chosen.wallpaper < 4 ? settings_wallpaper_colours[s->chosen.wallpaper] : 0x008080;
    draw_field(x, y, w, h, desk);
    gfx_clip(rect(x + 2, y + 2, w - 4, h - 4));
    settings_draw_window(x + 10, y + 6, 190, 46, "Inactive Window", 0, colours);
    settings_draw_window(x + 64, y + 30, 206, 46, "Active Window", 1, colours);
    gfx_clip(rect(0, 0, SETTINGS_W, SETTINGS_H));
}

static void settings_draw_slider(SettingsApp *s) {
    Rect area = settings_slider_rect();
    fill_rect(area.x, area.y, area.w, SETTINGS_SLIDER_Y + 12 + FONT_HEIGHT - area.y, FACE);
    bevel_sunken(SETTINGS_SLIDER_X, SETTINGS_SLIDER_Y - 2, SETTINGS_SLIDER_W, 4);
    for (int ms = 200; ms <= 900; ms += 100) vline(settings_slider_x(ms), SETTINGS_SLIDER_Y + 8, 3, BLACK);
    int x = settings_slider_x(s->chosen.double_click_ms);
    draw_button(x - 5, SETTINGS_SLIDER_Y - 10, 11, 20, "", 0);
    draw_text(SETTINGS_SLIDER_X - 4, SETTINGS_SLIDER_Y + 12, "Fast", SHADOW);
    draw_text(SETTINGS_SLIDER_X + SETTINGS_SLIDER_W - 28, SETTINGS_SLIDER_Y + 12, "Slow", SHADOW);
    char value[16];
    snprintf(value, sizeof value, "%d ms", s->chosen.double_click_ms);
    fill_rect(SETTINGS_SLIDER_X + SETTINGS_SLIDER_W + 14, SETTINGS_SLIDER_Y - 6, 60, FONT_HEIGHT, FACE);
    draw_text(SETTINGS_SLIDER_X + SETTINGS_SLIDER_W + 14, SETTINGS_SLIDER_Y - 6, value, BLACK);
}

static void settings_draw_button(SettingsApp *s, int which) {
    static const char *const labels[SETTINGS_BUTTONS] = { "OK", "Cancel", "Apply", "Browse..." };
    Rect r = settings_button_rect(which);
    int down = button_down(&s->buttons, which);
    fill_rect(r.x - 1, r.y - 1, r.w + 2, r.h + 2, FACE);
    draw_button(r.x, r.y, r.w, r.h, "", down);
    int enabled = which != SETTINGS_APPLY || settings_changed(s);
    int x = r.x + (r.w - text_width(labels[which])) / 2 + down, y = r.y + (r.h - FONT_HEIGHT) / 2 + down;
    if (!enabled) draw_text(x + 1, y + 1, labels[which], WHITE);
    draw_text(x, y, labels[which], enabled ? BLACK : SHADOW);
    if (which == SETTINGS_OK) frame_rect(r.x - 1, r.y - 1, r.w + 2, r.h + 2, BLACK);
}

static void settings_draw_all(SettingsApp *s) {
    os_window_draw(s->win);
    fill_rect(0, 0, SETTINGS_W, SETTINGS_H, FACE);
    settings_group(8, 10, SETTINGS_W - 16, 126, "Desktop wallpaper");
    for (int i = 0; i < WALLPAPER_COUNT; i++) {
        Rect r = settings_wallpaper_rect(i);
        draw_radio(r.x, r.y + 1, settings_wallpapers[i], s->chosen.wallpaper == i);
    }
    draw_field(22, 104, 198, 22, WHITE);
    gfx_clip(rect(24, 106, 194, 18));
    draw_text(27, 109, s->chosen.picture, s->chosen.wallpaper == WALLPAPER_PICTURE ? BLACK : SHADOW);
    gfx_clip(rect(0, 0, SETTINGS_W, SETTINGS_H));
    settings_draw_monitor(s, SETTINGS_W - 16 - 104, 18);
    settings_group(8, 146, SETTINGS_W - 16, 100, "Window theme");
    for (int i = 0; i < THEME_COUNT; i++) {
        Rect r = settings_theme_rect(i);
        draw_radio(r.x, r.y + 1, settings_themes[i], s->chosen.theme == i);
    }
    settings_draw_theme_preview(s);
    settings_group(8, 256, SETTINGS_W - 16, 72, "Mouse and taskbar");
    draw_text(22, SETTINGS_SLIDER_Y - 6, "Double-click speed:", BLACK);
    settings_draw_slider(s);
    Rect clock = settings_clock_rect(), sounds = settings_sounds_rect();
    draw_check(clock.x, clock.y + 1, "24-hour clock", s->chosen.clock_24h);
    draw_check(sounds.x, sounds.y + 1, "Play sounds", s->chosen.sounds);
    for (int i = 0; i < SETTINGS_BUTTONS; i++) settings_draw_button(s, i);
    os_window_update_all(s->win);
}

static void settings_draw_buttons(SettingsApp *s) {
    os_window_draw(s->win);
    for (int i = 0; i < SETTINGS_BUTTONS; i++) {
        settings_draw_button(s, i);
        Rect r = settings_button_rect(i);
        os_window_update(s->win, r.x - 1, r.y - 1, r.w + 2, r.h + 2);
    }
}

/* ------------------------------------------------------------------ what the controls do */

static void settings_apply(SettingsApp *s) {
    if (settings_changed(s)) os_apply_settings(&s->chosen);
}

static void settings_browse(SettingsApp *s) {
    char path[64];
    copy_text(path, sizeof path, s->chosen.picture[0] ? s->chosen.picture : "pictures");
    if (!os_file_dialog(s->win, 0, "Choose a Picture", ".bmp", path, sizeof path)) return;
    long size;
    char *data = os_read_file(path, &size);
    int w, h;
    uint32_t *pixels = data ? os_bmp_decode(data, size, &w, &h) : NULL;
    os_free(data);
    if (!pixels) {
        os_message_box(s->win, "Settings",
                       "That picture cannot be the wallpaper: choose an uncompressed 24- or 32-bit BMP file.",
                       MB_OK, MB_WARNING);
        return;
    }
    os_free(pixels);
    copy_text(s->chosen.picture, sizeof s->chosen.picture, path);
    s->chosen.wallpaper = WALLPAPER_PICTURE;
    settings_load_picture(s);
}

static void settings_slide(SettingsApp *s, int x) {
    int ms = settings_slider_value(x);
    if (ms == s->chosen.double_click_ms) return;
    s->chosen.double_click_ms = ms;
    os_window_draw(s->win);
    settings_draw_slider(s);
    Rect area = settings_slider_rect();
    os_window_update(s->win, area.x, area.y - 6, area.w + 80, area.h + 22);
    settings_draw_buttons(s);
}

static void settings_mouse(SettingsApp *s, const Message *m) {
    Rect buttons[SETTINGS_BUTTONS];
    for (int i = 0; i < SETTINGS_BUTTONS; i++) buttons[i] = settings_button_rect(i);
    /* Apply does nothing while nothing changed */
    if (!settings_changed(s)) buttons[SETTINGS_APPLY] = rect(0, 0, 0, 0);
    int changed, clicked = track_buttons(&s->buttons, buttons, SETTINGS_BUTTONS, m, &changed);
    if (changed) settings_draw_buttons(s);
    switch (clicked) {
    case SETTINGS_OK:
        settings_apply(s);
        os_exit(0);
    case SETTINGS_CANCEL: os_exit(0);
    case SETTINGS_APPLY:
        settings_apply(s);
        settings_draw_all(s);
        return;
    case SETTINGS_BROWSE:
        settings_browse(s);
        settings_draw_all(s);
        return;
    }
    if (m->type == MSG_MOUSE_UP) s->sliding = 0;
    if (m->type == MSG_MOUSE_MOVE && s->sliding) settings_slide(s, m->x);
    if (m->type != MSG_MOUSE_DOWN || m->button != BUTTON_LEFT || s->buttons.pressed >= 0) return;
    if (rect_contains(settings_slider_rect(), m->x, m->y)) {
        s->sliding = 1;
        settings_slide(s, m->x);
        return;
    }
    Settings before = s->chosen;
    for (int i = 0; i < WALLPAPER_COUNT; i++)
        if (rect_contains(settings_wallpaper_rect(i), m->x, m->y)) s->chosen.wallpaper = i;
    for (int i = 0; i < THEME_COUNT; i++)
        if (rect_contains(settings_theme_rect(i), m->x, m->y)) s->chosen.theme = i;
    if (rect_contains(settings_clock_rect(), m->x, m->y)) s->chosen.clock_24h = !s->chosen.clock_24h;
    if (rect_contains(settings_sounds_rect(), m->x, m->y)) s->chosen.sounds = !s->chosen.sounds;
    if (settings_same(&before, &s->chosen)) return;
    if (s->chosen.wallpaper == WALLPAPER_PICTURE) settings_load_picture(s);
    settings_draw_all(s);
}

static void settings_key(SettingsApp *s, const Message *m) {
    switch (m->key) {
    case KEY_ENTER: case KEY_KPENTER:
        settings_apply(s);
        os_exit(0);
    case KEY_ESC: os_exit(0);
    case KEY_LEFT: case KEY_RIGHT: {
        int step = m->key == KEY_LEFT ? -SETTINGS_STEP : SETTINGS_STEP;
        settings_slide(s, settings_slider_x(clamp_int(s->chosen.double_click_ms + step, 200, 900)));
        break;
    }
    }
}

/* Answers whether an older Settings is running, which this one leaves the job to. */
static int settings_already_open(void) {
    ProcessInfo list[32];
    int count = min_int(os_processes(list, 32), 32), me = os_getpid();
    for (int i = 0; i < count; i++)
        if (list[i].pid != me && list[i].pid < me && !strcmp(list[i].name, APPS[APP_SETTINGS].name)) return 1;
    return 0;
}

void settings_main(const char *argument) {
    (void)argument;
    if (settings_already_open()) {
        os_message_box(NULL, "Settings", "Settings is already open.", MB_OK, MB_INFO);
        os_exit(0);
    }
    SettingsApp *s = os_alloc(sizeof *s);
    if (!s) os_exit(1);
    s->chosen = *os_settings();
    s->buttons.pressed = -1;
    s->win = os_window_create("Settings", &ICON_SETTINGS, SETTINGS_W, SETTINGS_H, WIN_CENTER | WIN_NO_MINIMIZE);
    if (!s->win) os_exit(1);
    if (s->chosen.wallpaper == WALLPAPER_PICTURE) settings_load_picture(s);
    settings_draw_all(s);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: settings_mouse(s, &m); break;
        case MSG_KEY: settings_key(s, &m); break;
        case MSG_SETTINGS: case MSG_FOCUS: case MSG_BLUR: settings_draw_all(s); break;
        case MSG_CLOSE: os_exit(0);
        default: os_default_message(&m);
        }
    }
}
