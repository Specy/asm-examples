/*
 * Image Viewer: a .bmp picture on a dark background, fitted to the window or at a zoom of its own,
 * and the other pictures of its folder one after another. It shows the File it is started with,
 * one chosen with File > Open, or one dropped on its window; Edit in Paint opens the picture in
 * Paint. Page Up and Page Down, or the arrows, step through the folder; + and - (or the wheel)
 * zoom, F fits the picture to the window and 1 shows it at its actual size, and a picture larger
 * than the window is dragged around. A double click switches between fitting and actual size.
 */

enum { VIEWER_TOOLBAR_H = 30, VIEWER_STATUS_H = 20, VIEWER_MAX_FILES = 256, VIEWER_BUTTONS = 8,
       VIEWER_ROW_MAX = 1024 };
enum { VIEWER_BACKGROUND = 0x262626 };
enum { VIEWER_OPEN = 1, VIEWER_EDIT, VIEWER_EXIT, VIEWER_FIT, VIEWER_ACTUAL, VIEWER_ZOOM_IN, VIEWER_ZOOM_OUT,
       VIEWER_PREVIOUS, VIEWER_NEXT, VIEWER_ABOUT };
/* Steps past the first or the last picture, for Home and End. */
enum { VIEWER_TO_FIRST = -100000, VIEWER_TO_LAST = 100000 };

static MenuItem viewer_file_menu[] = {
    { "Open...", VIEWER_OPEN, 0, "Ctrl+O", NULL },
    { "Edit in Paint", VIEWER_EDIT, 0, "Ctrl+E", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", VIEWER_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
/* Each window has a copy of the View menu, whose checks it sets. */
static const MenuItem VIEWER_VIEW_MENU[] = {
    { "Fit to Window", VIEWER_FIT, 0, "F", NULL },
    { "Actual Size", VIEWER_ACTUAL, 0, "1", NULL },
    { "Zoom In", VIEWER_ZOOM_IN, 0, "+", NULL },
    { "Zoom Out", VIEWER_ZOOM_OUT, 0, "-", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Previous Picture", VIEWER_PREVIOUS, 0, "PgUp", NULL },
    { "Next Picture", VIEWER_NEXT, 0, "PgDn", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem viewer_help_menu[] = {
    { "About Image Viewer", VIEWER_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem VIEWER_CONTEXT_MENU[] = {
    { "Previous Picture", VIEWER_PREVIOUS, 0, NULL, NULL },
    { "Next Picture", VIEWER_NEXT, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Fit to Window", VIEWER_FIT, 0, NULL, NULL },
    { "Actual Size", VIEWER_ACTUAL, 0, NULL, NULL },
    { "Zoom In", VIEWER_ZOOM_IN, 0, NULL, NULL },
    { "Zoom Out", VIEWER_ZOOM_OUT, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Edit in Paint", VIEWER_EDIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

static const char VIEWER_LEFT_LABEL[] = { (char)CH_LEFT, 0 };
static const char VIEWER_RIGHT_LABEL[] = { (char)CH_RIGHT, 0 };
/* The toolbar's buttons, in groups: their labels, widths and commands. */
static const char *const VIEWER_BUTTON_LABELS[VIEWER_BUTTONS] = {
    "Open", VIEWER_LEFT_LABEL, VIEWER_RIGHT_LABEL, "-", "+", "Fit", "1:1", "Edit in Paint",
};
static const int VIEWER_BUTTON_WIDTHS[VIEWER_BUTTONS] = { 48, 28, 28, 28, 28, 40, 40, 112 };
static const int VIEWER_BUTTON_COMMANDS[VIEWER_BUTTONS] = {
    VIEWER_OPEN, VIEWER_PREVIOUS, VIEWER_NEXT, VIEWER_ZOOM_OUT, VIEWER_ZOOM_IN, VIEWER_FIT, VIEWER_ACTUAL,
    VIEWER_EDIT,
};
/* The zooms Zoom In and Zoom Out go through, in percent. */
static const int VIEWER_ZOOMS[] = { 5, 10, 25, 33, 50, 67, 100, 150, 200, 300, 400, 600, 800, 1200, 1600 };

typedef struct {
    Window *win;
    int w, h;                  /* the client area */
    uint32_t *pixels;          /* the picture, or NULL */
    int pw, ph;
    char path[128];
    char problem[96];          /* why there is no picture */
    int fit;                   /* fitted to the window, rather than shown at `zoom` percent */
    int zoom;
    int pan_x, pan_y;          /* how far a picture larger than the window is scrolled, in shown pixels */
    int panning, grab_x, grab_y;
    int index, count;          /* the picture's place among the pictures of its folder */
    ButtonTracker buttons;
    MenuItem viewer_menu[sizeof VIEWER_VIEW_MENU / sizeof VIEWER_VIEW_MENU[0]];
    Menu menus[4];
} Viewer;

static Rect viewer_area(const Viewer *v) {
    return rect(0, VIEWER_TOOLBAR_H, v->w, max_int(0, v->h - VIEWER_TOOLBAR_H - VIEWER_STATUS_H));
}

static Rect viewer_button_rect(int index) {
    int x = 4;
    for (int i = 0; i < index; i++) x += VIEWER_BUTTON_WIDTHS[i] + 2 + (i == 0 || i == 2 || i == 6 ? 8 : 0);
    return rect(x, 4, VIEWER_BUTTON_WIDTHS[index], 22);
}

/* How large the picture shows. Fitted, a picture larger than the window shrinks to it, and a
 * smaller one grows by a whole number of times, so that its pixels stay square. */
static void viewer_shown_size(const Viewer *v, int *dw, int *dh) {
    Rect area = viewer_area(v);
    if (!v->fit) {
        *dw = max_int(1, (int)((long)v->pw * v->zoom / 100));
        *dh = max_int(1, (int)((long)v->ph * v->zoom / 100));
    } else if (v->pw <= area.w && v->ph <= area.h) {
        int times = clamp_int(min_int(area.w / v->pw, area.h / v->ph), 1, 16);
        *dw = v->pw * times;
        *dh = v->ph * times;
    } else if ((long)v->pw * area.h > (long)v->ph * area.w) {
        *dw = max_int(1, area.w);
        *dh = max_int(1, (int)((long)v->ph * area.w / v->pw));
    } else {
        *dh = max_int(1, area.h);
        *dw = max_int(1, (int)((long)v->pw * area.h / v->ph));
    }
}

static int viewer_percent(const Viewer *v) {
    int dw, dh;
    viewer_shown_size(v, &dw, &dh);
    return (int)(((long)dw * 100 + v->pw / 2) / v->pw);
}

/* Where the picture shows in the client area: in the middle, or scrolled by the pan when larger. */
static Rect viewer_place(Viewer *v) {
    Rect area = viewer_area(v);
    int dw, dh;
    viewer_shown_size(v, &dw, &dh);
    v->pan_x = clamp_int(v->pan_x, 0, max_int(0, dw - area.w));
    v->pan_y = clamp_int(v->pan_y, 0, max_int(0, dh - area.h));
    int x = dw <= area.w ? area.x + (area.w - dw) / 2 : area.x - v->pan_x;
    int y = dh <= area.h ? area.y + (area.h - dh) / 2 : area.y - v->pan_y;
    return rect(x, y, dw, dh);
}

/* ------------------------------------------------------------------ drawing */

/* Draws the picture scaled into `place`, where it meets `area`. Each picture row shown is worked
 * out once, pixel by pixel, then copied into all the window's rows that show it. */
static void viewer_blit_scaled(Viewer *v, Rect place, Rect area) {
    Rect r = rect_intersect(place, area);
    if (rect_empty(r)) return;
    if (place.w == v->pw && place.h == v->ph) {
        blit(v->pixels, v->pw, place.x, place.y, place.w, place.h);
        return;
    }
    uint32_t row[VIEWER_ROW_MAX];
    int columns[VIEWER_ROW_MAX], width = min_int(r.w, VIEWER_ROW_MAX);
    for (int i = 0; i < width; i++) columns[i] = (int)((long)(r.x + i - place.x) * v->pw / place.w);
    for (int y = r.y; y < r.y + r.h;) {
        long source = (long)(y - place.y) * v->ph / place.h;
        int rows = 1;
        while (y + rows < r.y + r.h && (long)(y + rows - place.y) * v->ph / place.h == source) rows++;
        const uint32_t *from = v->pixels + source * v->pw;
        for (int i = 0; i < width; i++) row[i] = from[columns[i]];
        blit(row, 0, r.x, y, width, rows);
        y += rows;
    }
}

static void viewer_draw_picture(Viewer *v) {
    Rect area = viewer_area(v);
    if (rect_empty(area)) return;
    os_window_draw(v->win);
    gfx_clip(area);
    if (!v->pixels) {
        fill_rect(area.x, area.y, area.w, area.h, VIEWER_BACKGROUND);
        const char *first = v->problem[0] ? v->problem : "No picture";
        const char *second = v->problem[0] ? "" : "Open one with File > Open, or drop a .bmp here.";
        int y = area.y + area.h / 2 - FONT_HEIGHT;
        draw_text(area.x + (area.w - text_width(first)) / 2, y, first, LIGHT);
        draw_text(area.x + (area.w - text_width(second)) / 2, y + 18, second, SHADOW);
    } else {
        /* the dark background in strips around the picture, so that no pixel is drawn twice */
        Rect place = viewer_place(v), inside = rect_intersect(area, place);
        uint32_t dark = VIEWER_BACKGROUND;
        if (rect_empty(inside)) {
            fill_rect(area.x, area.y, area.w, area.h, dark);
        } else {
            fill_rect(area.x, area.y, area.w, inside.y - area.y, dark);
            fill_rect(area.x, inside.y + inside.h, area.w, area.y + area.h - inside.y - inside.h, dark);
            fill_rect(area.x, inside.y, inside.x - area.x, inside.h, dark);
            fill_rect(inside.x + inside.w, inside.y, area.x + area.w - inside.x - inside.w, inside.h, dark);
        }
        viewer_blit_scaled(v, place, area);
    }
    gfx_clip(rect(0, 0, v->w, v->h));
    os_window_update(v->win, area.x, area.y, area.w, area.h);
}

static void viewer_draw_toolbar(Viewer *v) {
    os_window_draw(v->win);
    fill_rect(0, 0, v->w, VIEWER_TOOLBAR_H, FACE);
    for (int i = 0; i < VIEWER_BUTTONS; i++) {
        Rect r = viewer_button_rect(i);
        draw_button(r.x, r.y, r.w, r.h, VIEWER_BUTTON_LABELS[i], button_down(&v->buttons, i));
    }
    hline(0, VIEWER_TOOLBAR_H - 2, v->w, SHADOW);
    hline(0, VIEWER_TOOLBAR_H - 1, v->w, WHITE);
    os_window_update(v->win, 0, 0, v->w, VIEWER_TOOLBAR_H);
}

static void viewer_draw_status(Viewer *v) {
    os_window_draw(v->win);
    int y = v->h - VIEWER_STATUS_H, place_x = v->w - 74, zoom_x = place_x - 94, size_x = zoom_x - 94;
    fill_rect(0, y, v->w, VIEWER_STATUS_H, FACE);
    bevel_thin_sunken(2, y + 2, size_x - 4, VIEWER_STATUS_H - 4);
    bevel_thin_sunken(size_x, y + 2, 90, VIEWER_STATUS_H - 4);
    bevel_thin_sunken(zoom_x, y + 2, 90, VIEWER_STATUS_H - 4);
    bevel_thin_sunken(place_x, y + 2, 72, VIEWER_STATUS_H - 4);
    GfxContext saved = gfx_save();
    gfx_clip(rect(4, y, size_x - 8, VIEWER_STATUS_H));
    draw_text(8, y + 4, v->path[0] ? os_base_name(v->path) : "No picture", BLACK);
    gfx_restore(saved);
    if (v->pixels) {
        char text[32];
        snprintf(text, sizeof text, "%d %c %d", v->pw, CH_TIMES, v->ph);
        draw_text(size_x + 6, y + 4, text, BLACK);
        snprintf(text, sizeof text, v->fit ? "Fit, %d%%" : "%d%%", viewer_percent(v));
        draw_text(zoom_x + 6, y + 4, text, BLACK);
    }
    if (v->count > 0 && v->index >= 0) {
        char text[32];
        snprintf(text, sizeof text, "%d of %d", v->index + 1, v->count);
        draw_text(place_x + 6, y + 4, text, BLACK);
    }
    os_window_update(v->win, 0, y, v->w, VIEWER_STATUS_H);
}

static void viewer_menu_state(Viewer *v) {
    for (MenuItem *item = v->viewer_menu; item->label; item++) {
        int checked = (item->id == VIEWER_FIT && v->fit) || (item->id == VIEWER_ACTUAL && !v->fit && v->zoom == 100);
        item->flags = checked ? MENU_CHECKED : 0;
    }
}

static void viewer_draw_all(Viewer *v) {
    viewer_menu_state(v);
    viewer_draw_toolbar(v);
    viewer_draw_picture(v);
    viewer_draw_status(v);
}

/* ------------------------------------------------------------------ pictures */

/* The picture's folder in `dir`, and the .bmp files in it in the order Files lists them: answers
 * how many, with the picture's place among them in `index`, or -1. */
static int viewer_list_pictures(const Viewer *v, char *dir, int dir_size, DirEntry *entries, int *index) {
    const char *slash = strrchr(v->path, '/');
    if (slash) snprintf(dir, dir_size, "%.*s", (int)(slash - v->path), v->path);
    else dir[0] = 0;
    int all = os_list_dir(dir, entries, VIEWER_MAX_FILES), count = 0;
    *index = -1;
    for (int i = 0; i < all; i++) {
        if (entries[i].is_dir || os_app_for_file(entries[i].name) != APP_VIEWER) continue;
        entries[count] = entries[i];
        if (strcmp(entries[count].name, os_base_name(v->path)) == 0) *index = count;
        count++;
    }
    return count;
}

static void viewer_load(Viewer *v, const char *path) {
    os_free(v->pixels);
    v->pixels = NULL;
    copy_text(v->path, sizeof v->path, path);
    /* a large picture takes a moment to read: the window says so meanwhile */
    snprintf(v->problem, sizeof v->problem, "Opening %.60s...", os_base_name(path));
    viewer_draw_picture(v);
    viewer_draw_status(v);
    v->problem[0] = 0;
    long size;
    char *data = os_read_file(path, &size);
    if (!data) {
        snprintf(v->problem, sizeof v->problem, "Cannot read %.60s", os_base_name(path));
    } else {
        v->pixels = os_bmp_decode(data, size, &v->pw, &v->ph);
        os_free(data);
        if (!v->pixels)
            snprintf(v->problem, sizeof v->problem, "%.40s is not a BMP picture this viewer reads", os_base_name(path));
    }
    v->fit = 1;
    v->zoom = 100;
    v->pan_x = v->pan_y = 0;
    v->panning = 0;
    DirEntry *entries = os_alloc(VIEWER_MAX_FILES * sizeof *entries);
    char dir[128];
    v->count = entries ? viewer_list_pictures(v, dir, sizeof dir, entries, &v->index) : 0;
    os_free(entries);
    char title[96];
    snprintf(title, sizeof title, "%.60s - Image Viewer", os_base_name(path));
    os_window_set_title(v->win, title);
    viewer_draw_all(v);
}

/* Shows the picture `step` places after this one in its folder, round from the last to the first. */
static void viewer_step(Viewer *v, int step) {
    DirEntry *entries = v->path[0] ? os_alloc(VIEWER_MAX_FILES * sizeof *entries) : NULL;
    if (!entries) {
        os_beep();
        return;
    }
    char dir[128];
    int index, count = viewer_list_pictures(v, dir, sizeof dir, entries, &index);
    if (count > 0) {
        int next = step == VIEWER_TO_FIRST ? 0 : step == VIEWER_TO_LAST ? count - 1
                   : index < 0 ? 0 : ((index + step) % count + count) % count;
        if (next != index || !v->pixels) {
            char path[256];
            snprintf(path, sizeof path, "%s%s%s", dir, dir[0] ? "/" : "", entries[next].name);
            viewer_load(v, path);
        }
    } else {
        os_beep();
    }
    os_free(entries);
}

/* Changes the zoom, keeping the point of the picture in the middle of the window where it is. */
static void viewer_set_zoom(Viewer *v, int fit, int zoom) {
    if (!v->pixels) {
        os_beep();
        return;
    }
    Rect area = viewer_area(v);
    int dw, dh, nw, nh;
    viewer_shown_size(v, &dw, &dh);
    long cx = dw <= area.w ? dw / 2 : v->pan_x + area.w / 2, cy = dh <= area.h ? dh / 2 : v->pan_y + area.h / 2;
    v->fit = fit;
    v->zoom = zoom;
    viewer_shown_size(v, &nw, &nh);
    v->pan_x = (int)(cx * nw / dw - area.w / 2);
    v->pan_y = (int)(cy * nh / dh - area.h / 2);
    viewer_menu_state(v);
    viewer_draw_picture(v);
    viewer_draw_status(v);
}

static void viewer_zoom(Viewer *v, int direction) {
    if (!v->pixels) {
        os_beep();
        return;
    }
    int now = viewer_percent(v), count = (int)(sizeof VIEWER_ZOOMS / sizeof VIEWER_ZOOMS[0]);
    int next = direction > 0 ? VIEWER_ZOOMS[count - 1] : VIEWER_ZOOMS[0];
    if (direction > 0) {
        for (int i = count - 1; i >= 0; i--)
            if (VIEWER_ZOOMS[i] > now) next = VIEWER_ZOOMS[i];
    } else {
        for (int i = 0; i < count; i++)
            if (VIEWER_ZOOMS[i] < now) next = VIEWER_ZOOMS[i];
    }
    viewer_set_zoom(v, 0, next);
}

/* ------------------------------------------------------------------ commands, keys and the pointer */

static void viewer_command(Viewer *v, int id) {
    switch (id) {
    case VIEWER_OPEN: {
        char path[128];
        copy_text(path, sizeof path, v->path[0] ? v->path : "pictures");
        if (os_file_dialog(v->win, 0, "Open", ".bmp", path, sizeof path)) viewer_load(v, path);
        break;
    }
    case VIEWER_EDIT:
        if (v->pixels) os_spawn(APP_PAINT, v->path);
        else os_beep();
        break;
    case VIEWER_EXIT: os_exit(0);
    case VIEWER_FIT: viewer_set_zoom(v, 1, v->zoom); break;
    case VIEWER_ACTUAL: viewer_set_zoom(v, 0, 100); break;
    case VIEWER_ZOOM_IN: viewer_zoom(v, 1); break;
    case VIEWER_ZOOM_OUT: viewer_zoom(v, -1); break;
    case VIEWER_PREVIOUS: viewer_step(v, -1); break;
    case VIEWER_NEXT: viewer_step(v, 1); break;
    case VIEWER_ABOUT:
        os_message_box(v->win, "About Image Viewer",
                       "Image Viewer for asmOS.\nPage Up and Page Down step through the pictures of a folder; "
                       "+ and - zoom, F fits the picture to the window and 1 shows its actual size.",
                       MB_OK, MB_INFO);
        break;
    }
}

static void viewer_key(Viewer *v, const Message *m) {
    if (m->mods & MOD_CTRL) {
        if (m->key == KEY_O) viewer_command(v, VIEWER_OPEN);
        if (m->key == KEY_E) viewer_command(v, VIEWER_EDIT);
        return;
    }
    switch (m->key) {
    case KEY_PAGEUP: case KEY_LEFT: case KEY_UP: case KEY_BACKSPACE: viewer_command(v, VIEWER_PREVIOUS); return;
    case KEY_PAGEDOWN: case KEY_RIGHT: case KEY_DOWN: case KEY_SPACE: viewer_command(v, VIEWER_NEXT); return;
    case KEY_HOME: viewer_step(v, VIEWER_TO_FIRST); return;
    case KEY_END: viewer_step(v, VIEWER_TO_LAST); return;
    }
    switch (m->ch) {
    case '+': case '=': viewer_command(v, VIEWER_ZOOM_IN); break;
    case '-': viewer_command(v, VIEWER_ZOOM_OUT); break;
    case 'f': case 'F': viewer_command(v, VIEWER_FIT); break;
    case '1': viewer_command(v, VIEWER_ACTUAL); break;
    }
}

static void viewer_mouse(Viewer *v, const Message *m) {
    Rect buttons[VIEWER_BUTTONS];
    for (int i = 0; i < VIEWER_BUTTONS; i++) buttons[i] = viewer_button_rect(i);
    int before = v->buttons.pressed, changed;
    int clicked = track_buttons(&v->buttons, buttons, VIEWER_BUTTONS, m, &changed);
    if (changed) viewer_draw_toolbar(v);
    if (clicked >= 0) {
        viewer_command(v, VIEWER_BUTTON_COMMANDS[clicked]);
        return;
    }
    if (v->buttons.pressed >= 0 || before >= 0) return;
    if (m->type == MSG_MOUSE_WHEEL) {
        viewer_zoom(v, m->wheel);
        return;
    }
    Rect area = viewer_area(v);
    if (m->type == MSG_MOUSE_DOWN && m->button == BUTTON_RIGHT && rect_contains(area, m->x, m->y)) {
        int id = os_popup_menu(v->win, m->x, m->y, VIEWER_CONTEXT_MENU);
        if (id) viewer_command(v, id);
    } else if (m->type == MSG_MOUSE_DOWN && m->button == BUTTON_LEFT && rect_contains(area, m->x, m->y) && v->pixels) {
        if (m->clicks == 2) {
            viewer_command(v, v->fit ? VIEWER_ACTUAL : VIEWER_FIT);
            return;
        }
        v->panning = 1;
        v->grab_x = m->x + v->pan_x;
        v->grab_y = m->y + v->pan_y;
    } else if (m->type == MSG_MOUSE_MOVE && v->panning) {
        int x = v->grab_x - m->x, y = v->grab_y - m->y;
        if (x != v->pan_x || y != v->pan_y) {
            v->pan_x = x;
            v->pan_y = y;
            viewer_draw_picture(v);
        }
    } else if (m->type == MSG_MOUSE_UP) {
        v->panning = 0;
    }
}

void viewer_main(const char *argument) {
    Viewer *v = os_alloc(sizeof *v);
    if (!v) os_exit(1);
    memcpy(v->viewer_menu, VIEWER_VIEW_MENU, sizeof v->viewer_menu);
    v->menus[0] = (Menu){ "File", viewer_file_menu };
    v->menus[1] = (Menu){ "View", v->viewer_menu };
    v->menus[2] = (Menu){ "Help", viewer_help_menu };
    v->buttons.pressed = -1;
    v->fit = 1;
    v->zoom = 100;
    v->win = os_window_create("Image Viewer", &ICON_VIEWER, 440, 330, WIN_RESIZABLE);
    if (!v->win) os_exit(1);
    os_window_set_menu(v->win, v->menus);
    os_window_min_size(v->win, 400, 120);
    os_window_pixels(v->win, &v->w, &v->h);
    viewer_draw_all(v);
    if (argument && argument[0]) viewer_load(v, argument);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_KEY: viewer_key(v, &m); break;
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: case MSG_MOUSE_WHEEL: viewer_mouse(v, &m); break;
        case MSG_COMMAND: viewer_command(v, m.id); break;
        case MSG_RESIZE:
            os_window_pixels(v->win, &v->w, &v->h);
            viewer_draw_all(v);
            break;
        case MSG_SETTINGS: viewer_draw_all(v); break;
        case MSG_DROP:
            if (os_app_for_file(m.text) == APP_VIEWER) viewer_load(v, m.text);
            else os_message_box(v->win, "Image Viewer", "Image Viewer shows .bmp pictures.", MB_OK, MB_INFO);
            break;
        default: os_default_message(&m);
        }
    }
}
