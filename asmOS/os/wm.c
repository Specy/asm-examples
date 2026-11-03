/*
 * The window manager and compositor: windows and their frames, menus, the desktop with its icons,
 * the taskbar and the Start menu, drag and drop, and the pointer.
 *
 * Every window has a picture of its own, its client area, which its process draws into whenever
 * it likes. The compositor never asks a process to draw: when something changes, a rectangle of
 * the screen is marked dirty, and wm_render repaints the dirty rectangles into the back buffer,
 * from the desktop up through the windows (their frames, and their pictures copied) to the
 * taskbar and the menus, then copies them onto the framebuffer. The pointer, a dragged file and a
 * resize outline are drawn on the framebuffer alone, over what the compositor put there. Whatever
 * reaches the framebuffer is then shown (show), which most machines need not do: their
 * framebuffer is the screen.
 *
 * The system process runs all of this, never preempted. A process reaches it through the os_
 * calls, which hold the kernel lock while they change anything here.
 */

enum { MAX_WINDOWS = 48, MAX_DIRTY = 32, MAX_POPUPS = 4 };
enum { BORDER = 4, TITLE_H = 18, MENU_BAR_H = 19, TASKBAR_H = 28 };
enum { MIN_CLIENT_W = 120, MIN_CLIENT_H = 48 };

/* A picture a resize replaced, freed once the process has taken MSG_RESIZE: until then it may
 * still be drawing into it. */
typedef struct Retired {
    struct Retired *next;
    uint32_t *pixels;
} Retired;

struct Window {
    int used;
    int pid;                  /* the owner: 0 for the system's own windows, the dialogs */
    int x, y, w, h;           /* the frame, in screen pixels */
    uint32_t *pixels;         /* the client area's picture */
    int pw, ph;               /* its size */
    Retired *retired;
    char title[64];
    const Icon *icon;
    int flags, minimized, maximized;
    Rect restore;
    int min_w, min_h;
    const Menu *menu;
    /* a system window: what its paint, pointer and keys do, in the system process */
    void (*paint)(Window *win);
    void (*on_mouse)(Window *win, const Message *message);
    void (*on_key)(Window *win, const Message *message);
    void *data;
    int modal_pid;            /* a dialog: the process waiting for it, whose windows wait too */
};

static Window window_slots[MAX_WINDOWS];
static Window *stack[MAX_WINDOWS];   /* bottom to top */
static int window_count;

static uint32_t *framebuffer, *back_buffer;
static int framebuffer_stride;

static Rect dirty[MAX_DIRTY];
static int dirty_count;

static int cursor_x, cursor_y, cursor_shown;
static Rect outline;                /* a resize's outline, drawn on the framebuffer */
static int outline_shown;
static char drag_path[128];         /* a file being dragged */
static int drag_active;

/* What the pointer is doing with a press that has not been released yet. */
enum { HELD_NOTHING, HELD_MOVE, HELD_RESIZE, HELD_CLIENT, HELD_CLOSE, HELD_MAXIMIZE,
       HELD_MINIMIZE, HELD_ICON };
enum { EDGE_LEFT = 1, EDGE_RIGHT = 2, EDGE_TOP = 4, EDGE_BOTTOM = 8 };
static int held;
static Window *held_window;
static int drag_dx, drag_dy;
static int held_inside;
static int resize_edges;
static Rect resize_start;
static int press_x, press_y;
static int held_icon = -1;

static int selected_icon = -1;
static int cascade;
static int shutting_down;
static char clock_text[12];
static int cpu_percent;

static Rect screen_rect(void) { return rect(0, 0, screen_w, screen_h - TASKBAR_H); }
static Rect taskbar_rect(void) { return rect(0, screen_h - TASKBAR_H, screen_w, TASKBAR_H); }
static long area(Rect r) { return rect_empty(r) ? 0 : (long)r.w * r.h; }

/* ------------------------------------------------------------------ dirty rectangles */

static void invalidate(Rect r) {
    r = rect_intersect(r, rect(0, 0, screen_w, screen_h));
    if (rect_empty(r)) return;
    /* A rectangle joins another when their union is barely bigger than the two, which can make
     * it join a third. Strips around a moved window stay apart, since their union would be the
     * whole window; two dirty rectangles may overlap, and the overlap is painted twice. */
    for (int i = 0; i < dirty_count; i++) {
        Rect joined = rect_union(r, dirty[i]);
        if (area(joined) * 4 <= (area(r) + area(dirty[i])) * 5) {
            r = joined;
            dirty[i] = dirty[--dirty_count];
            i = -1;
        }
    }
    if (dirty_count == MAX_DIRTY) {
        for (int i = 1; i < dirty_count; i++) dirty[0] = rect_union(dirty[0], dirty[i]);
        dirty_count = 1;
        dirty[0] = rect_union(dirty[0], r);
        return;
    }
    dirty[dirty_count++] = r;
}

/* ------------------------------------------------------------------ what is drawn over the screen */

static const char *const ARROW[19] = {
    "X           ", "XX          ", "X.X         ", "X..X        ", "X...X       ",
    "X....X      ", "X.....X     ", "X......X    ", "X.......X   ", "X........X  ",
    "X.........X ", "X......XXXXX", "X...X..X    ", "X..XX..X    ", "X.X  X..X   ",
    "XX   X..X   ", "X     X..X  ", "      X..X  ", "       XX   ",
};
enum { ARROW_W = 12, ARROW_H = 19 };

/* The pointer, with a file's icon beside it while one is dragged. */
static Rect cursor_rect(void) {
    return rect(cursor_x, cursor_y, drag_active ? 32 : ARROW_W, drag_active ? 36 : ARROW_H);
}

/* Shows what was drawn into the framebuffer in a rectangle of it, on a machine that has to. */
static void show(Rect r) {
    r = rect_intersect(r, rect(0, 0, screen_w, screen_h));
    if (!rect_empty(r)) platform_screen_present(r.x, r.y, r.w, r.h);
}

static void present(Rect r) {
    r = rect_intersect(r, rect(0, 0, screen_w, screen_h));
    if (rect_empty(r)) return;
    copy_rect32(framebuffer + r.y * framebuffer_stride + r.x, framebuffer_stride * 4,
                back_buffer + r.y * screen_w + r.x, screen_w * 4, r.w, r.h);
    show(r);
}

/* asmOS DOOM: the back buffer brought up to date where a picture went straight to the
 * framebuffer, before anything reads it there (wm_render's present_direct) */
static void back_refresh(Rect r);

static void cursor_draw(void) {
    if (drag_active) {
        GfxContext saved = gfx_save();
        gfx_target(framebuffer, screen_w, screen_h, framebuffer_stride);
        draw_icon(os_app_for_file(drag_path) == APP_VIEWER ? &ICON_PICTURE : &ICON_FILE,
                  cursor_x + 12, cursor_y + 18, 1);
        gfx_restore(saved);
    }
    for (int row = 0; row < ARROW_H; row++) {
        int y = cursor_y + row;
        if (y >= screen_h) break;
        uint32_t *line = framebuffer + y * framebuffer_stride;
        for (int column = 0; column < ARROW_W; column++) {
            int x = cursor_x + column;
            char c = ARROW[row][column];
            if (x >= screen_w || c == ' ') continue;
            line[x] = c == 'X' ? BLACK : WHITE;
        }
    }
    cursor_shown = 1;
    show(cursor_rect());
}

static void cursor_hide(void) {
    if (cursor_shown) {
        back_refresh(cursor_rect());
        present(cursor_rect());
    }
    cursor_shown = 0;
}

static void cursor_move(int x, int y) {
    if (x == cursor_x && y == cursor_y && cursor_shown) return;
    cursor_hide();
    cursor_x = x;
    cursor_y = y;
    cursor_draw();
}

/* The four 2-pixel strips of a resize outline. */
static void outline_strips(Rect r, Rect *strips) {
    strips[0] = rect(r.x, r.y, r.w, 2);
    strips[1] = rect(r.x, r.y + r.h - 2, r.w, 2);
    strips[2] = rect(r.x, r.y, 2, r.h);
    strips[3] = rect(r.x + r.w - 2, r.y, 2, r.h);
}

static void outline_draw(void) {
    Rect strips[4];
    outline_strips(outline, strips);
    for (int i = 0; i < 4; i++) {
        Rect s = rect_intersect(strips[i], rect(0, 0, screen_w, screen_h));
        for (int y = s.y; y < s.y + s.h; y++)
            for (int x = s.x; x < s.x + s.w; x++)
                if ((x + y) & 1) framebuffer[y * framebuffer_stride + x] ^= 0xffffff;
        show(s);
    }
    outline_shown = 1;
}

static void outline_hide(void) {
    if (!outline_shown) return;
    Rect strips[4];
    outline_strips(outline, strips);
    for (int i = 0; i < 4; i++) {
        back_refresh(strips[i]);
        present(strips[i]);
    }
    outline_shown = 0;
}

/* ------------------------------------------------------------------ windows */

static int has_menu(const Window *win) { return win->menu != NULL; }
static Rect window_frame(const Window *win) { return rect(win->x, win->y, win->w, win->h); }
static Rect client_rect(const Window *win) {
    int top = BORDER + TITLE_H + 1 + (has_menu(win) ? MENU_BAR_H : 0);
    return rect(win->x + BORDER, win->y + top, win->w - 2 * BORDER, win->h - top - BORDER);
}
static Rect title_rect(const Window *win) {
    return rect(win->x + BORDER, win->y + BORDER, win->w - 2 * BORDER, TITLE_H);
}
static Rect menu_bar_rect(const Window *win) {
    return rect(win->x + BORDER, win->y + BORDER + TITLE_H + 1, win->w - 2 * BORDER, MENU_BAR_H);
}
static Rect close_button(const Window *win) {
    return rect(win->x + win->w - BORDER - 18, win->y + BORDER + 2, 16, 14);
}
static Rect maximize_button(const Window *win) {
    return win->flags & WIN_RESIZABLE ? rect(win->x + win->w - BORDER - 36, win->y + BORDER + 2, 16, 14)
                                      : rect(0, 0, 0, 0);
}
static Rect minimize_button(const Window *win) {
    if (win->flags & WIN_NO_MINIMIZE || win->modal_pid) return rect(0, 0, 0, 0);
    int offset = win->flags & WIN_RESIZABLE ? 52 : 36;
    return rect(win->x + win->w - BORDER - offset, win->y + BORDER + 2, 16, 14);
}

static Window *top_window(void) {
    for (int i = window_count - 1; i >= 0; i--)
        if (!stack[i]->minimized) return stack[i];
    return NULL;
}

static int stack_index(const Window *win) {
    for (int i = 0; i < window_count; i++)
        if (stack[i] == win) return i;
    return -1;
}

int wm_window_count(int pid) {
    int count = 0;
    for (int i = 0; i < window_count; i++) count += stack[i]->pid == pid;
    return count;
}

/* The dialog a process waits for, whose windows take no input meanwhile. */
static Window *dialog_of(int pid) {
    if (pid <= 0) return NULL;
    for (int i = window_count - 1; i >= 0; i--)
        if (stack[i]->modal_pid == pid) return stack[i];
    return NULL;
}

static void invalidate_title(Window *win) {
    if (win && !win->minimized) invalidate(title_rect(win));
}
static void invalidate_taskbar(void) { invalidate(taskbar_rect()); }

static void send_focus(Window *win, int type) {
    if (!win || win->pid <= 0) return;
    Message message = { 0 };
    message.type = type;
    message.win = win;
    post_message(win->pid, &message);
}

static void window_raise(Window *win) {
    Window *before = top_window();
    int index = stack_index(win);
    if (index < 0) return;
    win->minimized = 0;
    for (int i = index; i < window_count - 1; i++) stack[i] = stack[i + 1];
    stack[window_count - 1] = win;
    if (before != win) {
        invalidate(window_frame(win));
        invalidate_title(before);
        invalidate_taskbar();
        send_focus(before, MSG_BLUR);
        send_focus(win, MSG_FOCUS);
    }
}

/* Raises a window, or the dialog its process waits for. */
static void window_focus(Window *win) {
    Window *dialog = dialog_of(win->pid);
    if (dialog && dialog != win) {
        if (win->minimized) window_raise(win);
        window_raise(dialog);
        os_beep();
        return;
    }
    window_raise(win);
}

/* A client area of w by h pixels: a new picture, with what fits of the old one, the rest the
 * face colour. The old picture goes on the retired list while a process may still draw into it. */
static void window_set_picture_size(Window *win, int w, int h) {
    w = max_int(1, w);
    h = max_int(1, h);
    if (w == win->pw && h == win->ph) return;
    uint32_t *pixels = malloc((size_t)w * h * 4);
    if (!pixels) return;
    fill_rect32(pixels, w * 4, w, h, FACE);
    if (win->pixels) {
        copy_rect32(pixels, w * 4, win->pixels, win->pw * 4, min_int(w, win->pw), min_int(h, win->ph));
        if (win->pid > 0) {
            Retired *retired = malloc(sizeof *retired);
            if (retired) {
                retired->pixels = win->pixels;
                retired->next = win->retired;
                win->retired = retired;
            } else {
                free(win->pixels);
            }
        } else {
            free(win->pixels);
        }
    }
    win->pixels = pixels;
    win->pw = w;
    win->ph = h;
    if (win->pid > 0) {
        Message message = { 0 };
        message.type = MSG_RESIZE;
        message.win = win;
        message.w = w;
        message.h = h;
        post_message(win->pid, &message);
    } else if (win->paint) {
        win->paint(win);
    }
}

/* Called as the owner takes MSG_RESIZE: it draws into the new picture from now on. */
void wm_take_retired(Window *win) {
    while (win->retired) {
        Retired *retired = win->retired;
        win->retired = retired->next;
        free(retired->pixels);
        free(retired);
    }
}

/* Gives a window a new frame, and its client area the size that follows. */
static void window_set_frame(Window *win, Rect frame) {
    invalidate(window_frame(win));
    win->x = frame.x;
    win->y = frame.y;
    win->w = frame.w;
    win->h = frame.h;
    Rect client = client_rect(win);
    window_set_picture_size(win, client.w, client.h);
    invalidate(window_frame(win));
}

static void window_toggle_maximize(Window *win) {
    if (!(win->flags & WIN_RESIZABLE)) return;
    if (win->maximized) {
        win->maximized = 0;
        window_set_frame(win, win->restore);
    } else {
        win->restore = window_frame(win);
        win->maximized = 1;
        window_set_frame(win, rect(0, 0, screen_w, screen_h - TASKBAR_H));
    }
}

static void window_minimize(Window *win) {
    win->minimized = 1;
    invalidate(window_frame(win));
    invalidate_title(top_window());
    invalidate_taskbar();
    send_focus(win, MSG_BLUR);
}

/* Takes a window off the screen and frees it. The caller holds the lock, or is the system. */
static void window_destroy(Window *win) {
    int index = stack_index(win);
    if (index < 0) return;
    int was_top = win == top_window();
    if (held_window == win) {
        held = HELD_NOTHING;
        held_window = NULL;
        outline_hide();
    }
    if (!win->minimized) invalidate(window_frame(win));
    for (int i = index; i < window_count - 1; i++) stack[i] = stack[i + 1];
    window_count--;
    forget_window_timers(win);
    wm_take_retired(win);
    free(win->pixels);
    if (win->pid == 0) free(win->data);
    memset(win, 0, sizeof *win);
    Window *top = top_window();
    invalidate_title(top);
    invalidate_taskbar();
    if (was_top) send_focus(top, MSG_FOCUS);
}

static Window *window_new(int pid, const char *title, const Icon *icon, int w, int h, int flags) {
    Window *win = NULL;
    for (int i = 0; i < MAX_WINDOWS && !win; i++)
        if (!window_slots[i].used) win = &window_slots[i];
    if (!win) return NULL;
    memset(win, 0, sizeof *win);
    win->used = 1;
    win->pid = pid;
    win->flags = flags;
    win->icon = icon;
    win->min_w = MIN_CLIENT_W;
    win->min_h = MIN_CLIENT_H;
    snprintf(win->title, sizeof win->title, "%s", title);
    win->w = w + 2 * BORDER;
    win->h = h + 2 * BORDER + TITLE_H + 1;
    int room_x = max_int(1, screen_w - win->w), room_y = max_int(1, screen_h - TASKBAR_H - win->h);
    if (flags & WIN_CENTER) {
        win->x = room_x / 2;
        win->y = room_y / 2;
    } else {
        win->x = (40 + cascade * 24) % room_x;
        win->y = (16 + cascade * 24) % room_y;
        cascade = (cascade + 1) % 9;
    }
    window_set_picture_size(win, w, h);
    Window *before = top_window();
    stack[window_count++] = win;
    invalidate(window_frame(win));
    invalidate_title(before);
    invalidate_taskbar();
    send_focus(before, MSG_BLUR);
    return win;
}

/* ---- what processes call */

Window *os_window_create(const char *title, const Icon *icon, int w, int h, int flags) {
    lock();
    Window *win = window_new(current->pid, title, icon ? icon : APPS[current->app].icon, w, h, flags);
    unlock();
    return win;
}

void os_window_close(Window *win) {
    lock();
    if (win && win->used && win->pid == current->pid) window_destroy(win);
    unlock();
}

void os_window_set_title(Window *win, const char *title) {
    lock();
    snprintf(win->title, sizeof win->title, "%s", title);
    invalidate_title(win);
    invalidate_taskbar();
    unlock();
}

void os_window_move(Window *win, int x, int y) {
    lock();
    if (win && win->used && win->pid == current->pid && !win->maximized) {
        Rect frame = window_frame(win);
        frame.x = clamp_int(x, 0, max_int(0, screen_w - frame.w));
        frame.y = clamp_int(y, 0, max_int(0, screen_h - TASKBAR_H - frame.h));
        window_set_frame(win, frame);
    }
    unlock();
}

uint32_t *os_window_pixels(Window *win, int *w, int *h) {
    if (w) *w = win->pw;
    if (h) *h = win->ph;
    return win->pixels;
}

void os_window_draw(Window *win) { gfx_target(win->pixels, win->pw, win->ph, win->pw); }

void os_window_update(Window *win, int x, int y, int w, int h) {
    lock();
    if (win->used && !win->minimized) {
        Rect client = client_rect(win);
        invalidate(rect_intersect(rect(client.x + x, client.y + y, w, h), client));
    }
    unlock();
}

void os_window_update_all(Window *win) { os_window_update(win, 0, 0, win->pw, win->ph); }

int os_window_focused(Window *win) { return win == top_window(); }

void os_window_focus(Window *win) {
    lock();
    window_focus(win);
    unlock();
}

void os_window_min_size(Window *win, int w, int h) {
    win->min_w = w;
    win->min_h = h;
}

Rect os_window_client_rect(Window *win) { return client_rect(win); }

void os_window_set_menu(Window *win, const Menu *menus) {
    lock();
    int had = has_menu(win);
    win->menu = menus;
    if (had != has_menu(win)) {
        /* the menu bar takes its height from the frame's, as a window grows by its menu */
        Rect frame = window_frame(win);
        frame.h += (has_menu(win) ? 1 : -1) * MENU_BAR_H;
        /* a centred window stays centred */
        if (win->flags & WIN_CENTER) frame.y = max_int(0, frame.y - (has_menu(win) ? 1 : -1) * MENU_BAR_H / 2);
        window_set_frame(win, frame);
    }
    invalidate(window_frame(win));
    unlock();
}

/* Every window, menu and dialog of a process that ended. */
static void popup_close_for(int pid);
void wm_process_ended(int pid) {
    popup_close_for(pid);
    for (int i = window_count - 1; i >= 0; i--) {
        if (i >= window_count) continue;
        Window *win = stack[i];
        if (win->pid == pid || win->modal_pid == pid) window_destroy(win);
    }
}

/* ------------------------------------------------------------------ painting */

static void paint_frame(Window *win, Rect region) {
    int active = win == top_window();
    gfx_clip(region);
    gfx_origin(0, 0);
    int x = win->x, y = win->y, w = win->w, h = win->h;
    bevel_frame(x, y, w, h);
    frame_rect(x + 2, y + 2, w - 4, h - 4, FACE);
    frame_rect(x + 3, y + 3, w - 6, h - 6, FACE);
    hline(x + BORDER, y + BORDER + TITLE_H, w - 2 * BORDER, FACE);
    Rect title = title_rect(win);
    if (rect_overlaps(title, region)) {
        gradient_rect(title.x, title.y, title.w, title.h,
                      os_theme_colour(active ? THEME_TITLE : THEME_INACTIVE),
                      os_theme_colour(active ? THEME_TITLE_END : THEME_INACTIVE_END));
        if (win->icon) draw_icon(win->icon, title.x + 2, title.y + 1, 1);
        Rect buttons_start = minimize_button(win).w ? minimize_button(win)
                             : maximize_button(win).w ? maximize_button(win)
                                                      : close_button(win);
        gfx_clip(rect_intersect(region, rect(title.x, title.y, buttons_start.x - title.x - 2, title.h)));
        draw_text_bold(title.x + (win->icon ? 21 : 4), title.y + 3, win->title, active ? WHITE : LIGHT);
        gfx_clip(region);
        Rect close = close_button(win), maximize = maximize_button(win), minimize = minimize_button(win);
        int close_down = held == HELD_CLOSE && held_window == win && held_inside;
        int maximize_down = held == HELD_MAXIMIZE && held_window == win && held_inside;
        int minimize_down = held == HELD_MINIMIZE && held_window == win && held_inside;
        draw_button(close.x, close.y, close.w, close.h, "", close_down);
        int cx = close.x + 4 + close_down, cy = close.y + 3 + close_down;
        for (int i = 0; i < 7; i++) {
            fill_rect(cx + i, cy + i, 2, 1, BLACK);
            fill_rect(cx + 6 - i, cy + i, 2, 1, BLACK);
        }
        if (maximize.w) {
            draw_button(maximize.x, maximize.y, maximize.w, maximize.h, "", maximize_down);
            int mx = maximize.x + 3 + maximize_down, my = maximize.y + 2 + maximize_down;
            if (win->maximized) {
                frame_rect(mx + 3, my, 7, 6, BLACK);
                hline(mx + 3, my + 1, 7, BLACK);
                fill_rect(mx, my + 3, 7, 6, FACE);
                frame_rect(mx, my + 3, 7, 6, BLACK);
                hline(mx, my + 4, 7, BLACK);
            } else {
                frame_rect(mx, my, 9, 9, BLACK);
                hline(mx, my + 1, 9, BLACK);
            }
        }
        if (minimize.w) {
            draw_button(minimize.x, minimize.y, minimize.w, minimize.h, "", minimize_down);
            fill_rect(minimize.x + 4 + minimize_down, minimize.y + 9 + minimize_down, 6, 2, BLACK);
        }
    }
    if (has_menu(win)) {
        Rect bar = menu_bar_rect(win);
        if (rect_overlaps(bar, region)) {
            fill_rect(bar.x, bar.y, bar.w, bar.h, FACE);
            void paint_menu_bar(Window *win, Rect bar);
            paint_menu_bar(win, bar);
        }
    }
}

static void paint_client(Window *win, Rect region) {
    Rect client = client_rect(win);
    Rect visible = rect_intersect(client, region);
    if (rect_empty(visible)) return;
    gfx_clip(visible);
    gfx_origin(client.x, client.y);
    blit(win->pixels, win->pw, 0, 0, win->pw, win->ph);
    /* a client area a resize made larger than its picture, until its process draws again */
    if (win->pw < client.w) fill_rect(win->pw, 0, client.w - win->pw, client.h, FACE);
    if (win->ph < client.h) fill_rect(0, win->ph, client.w, client.h - win->ph, FACE);
    gfx_origin(0, 0);
}

/* ---- the desktop */

enum { ICON_CELL_W = 88, ICON_CELL_H = 68, MAX_DESKTOP_FILES = 24 };
static DirEntry desktop_files[MAX_DESKTOP_FILES];
static int desktop_file_count;
static uint32_t *wallpaper_picture;   /* WALLPAPER_PICTURE, scaled to the screen */

static int desktop_app_count(void) {
    int count = 0;
    for (int i = 0; i < APP_COUNT; i++) count += APPS[i].on_desktop;
    return count;
}
static int desktop_icon_count(void) { return desktop_app_count() + desktop_file_count; }

/* The application of an icon, or -1 for a file of the Desktop folder. */
static int desktop_icon_app(int index) {
    for (int i = 0; i < APP_COUNT; i++)
        if (APPS[i].on_desktop && index-- == 0) return i;
    return -1;
}

static DirEntry *desktop_icon_file(int index) {
    index -= desktop_app_count();
    return index >= 0 && index < desktop_file_count ? &desktop_files[index] : NULL;
}

static Rect desktop_icon_rect(int index) {
    int per_column = max_int(1, (screen_h - TASKBAR_H - 8) / ICON_CELL_H);
    return rect(4 + index / per_column * ICON_CELL_W, 6 + index % per_column * ICON_CELL_H,
                ICON_CELL_W, ICON_CELL_H - 4);
}

static void desktop_refresh(void) {
    DirEntry entries[MAX_DESKTOP_FILES];
    int count = os_list_dir("Desktop", entries, MAX_DESKTOP_FILES);
    if (count < 0) count = 0;
    if (count == desktop_file_count && !memcmp(entries, desktop_files, count * sizeof entries[0])) return;
    memcpy(desktop_files, entries, count * sizeof entries[0]);
    desktop_file_count = count;
    invalidate(screen_rect());
}

static const Icon *file_icon(const char *name, int is_dir) {
    if (is_dir) return &ICON_FOLDER;
    int app = os_app_for_file(name);
    return app == APP_VIEWER ? &ICON_PICTURE : app == APP_MUSIC ? &ICON_MUSIC : app == APP_VIDEO ? &ICON_VIDEO
           : app == APP_IDE ? &ICON_IDE : app == APP_NOTEPAD ? &ICON_NOTEPAD : &ICON_FILE;
}

enum { LOGO_W = 160, LOGO_H = 128 + 4 + FONT_HEIGHT * 4 };

static uint32_t wallpaper_colour(void) {
    static const uint32_t colours[] = { 0x008080, 0x204880, 0x306030, 0x503050 };
    int which = os_settings()->wallpaper;
    return which < 4 ? colours[which] : 0x008080;
}

/* A large chip and the system's name, a shade lighter than the desktop, drawn once into a picture
 * of their own and then copied wherever the desktop is repainted. */
static uint32_t *wallpaper_logo(void) {
    static uint32_t *logo;
    static uint32_t made_for = 0xffffffff;
    uint32_t base = wallpaper_colour();
    if (logo && made_for == base) return logo;
    if (!logo) logo = malloc(LOGO_W * LOGO_H * 4);
    made_for = base;
    GfxContext saved = gfx_save();
    gfx_target(logo, LOGO_W, LOGO_H, LOGO_W);
    fill_rect(0, 0, LOGO_W, LOGO_H, base);
    uint32_t lighter = ((base >> 1 & 0x7f7f7f) + 0x101010 + (base >> 2 & 0x3f3f3f)) & 0xffffff;
    lighter = (base & 0xfefefe) / 2 + (lighter & 0xfefefe) / 2 + 0x0a0a0a;
    for (int row = 0; row < 16; row++)
        for (int column = 0; column < 16; column++) {
            char c = ICON_CHIP.rows[row][column];
            if (c == ' ') continue;
            uint32_t shade = c == 'y' ? lighter + 0x181818 : c == 'd' ? lighter + 0x0c0c0c : lighter;
            fill_rect((LOGO_W - 128) / 2 + column * 8, row * 8, 8, 8, shade);
        }
    const char *name = "asmOS";
    int scale = 4, left = (LOGO_W - text_width(name) * scale) / 2, top = 128 + 4;
    for (int i = 0; name[i]; i++) {
        const unsigned char *glyph = (const unsigned char *)font_bits + (name[i] - FONT_FIRST) * FONT_H;
        for (int line = 0; line < FONT_H; line++)
            for (int column = 0; column < FONT_W; column++)
                if (glyph[line] & (0x80 >> column))
                    fill_rect(left + (i * FONT_W + column) * scale, top + line * scale, scale, scale,
                              lighter + 0x101010);
    }
    gfx_restore(saved);
    return logo;
}

uint32_t *bmp_decode(const char *data, long size, int *w, int *h);   /* bmp.c */

/* WALLPAPER_PICTURE's picture, read and scaled to the screen once. */
static void wallpaper_load(void) {
    free(wallpaper_picture);
    wallpaper_picture = NULL;
    if (os_settings()->wallpaper != WALLPAPER_PICTURE) return;
    long size;
    char *data = read_file_with(os_settings()->picture, &size, malloc, free);
    if (!data) return;
    int w, h;
    uint32_t *picture = bmp_decode(data, size, &w, &h);
    free(data);
    if (!picture) return;
    int sh = screen_h - TASKBAR_H;
    wallpaper_picture = malloc((size_t)screen_w * sh * 4);
    if (wallpaper_picture) {
        GfxContext saved = gfx_save();
        gfx_target(wallpaper_picture, screen_w, sh, screen_w);
        blit_scaled(picture, w, w, h, 0, 0, screen_w, sh);
        gfx_restore(saved);
    }
    free(picture);
}

void wm_settings_changed(void) {
    wallpaper_load();
    invalidate(rect(0, 0, screen_w, screen_h));
}

static void paint_wallpaper(Rect region) {
    int sh = screen_h - TASKBAR_H, which = os_settings()->wallpaper;
    if (which == WALLPAPER_PICTURE && wallpaper_picture) {
        blit(wallpaper_picture, screen_w, 0, 0, screen_w, sh);
        return;
    }
    if (which == WALLPAPER_GRADIENT) {
        /* a vertical fade, in bands of 4 lines, only those the region needs */
        for (int y = region.y & ~3; y < min_int(sh, region.y + region.h); y += 4) {
            int t = y * 255 / sh;
            fill_rect(0, y, screen_w, 4, (uint32_t)((0x10 + t / 8) << 16 | (0x50 - t / 6) << 8 | (0x90 - t / 3)));
        }
        return;
    }
    if (which == WALLPAPER_PATTERN) {
        fill_rect(0, 0, screen_w, sh, 0x007070);
        for (int y = region.y & ~7; y < min_int(sh, region.y + region.h); y += 8)
            for (int x = region.x & ~7; x < region.x + region.w; x += 8)
                if (((x ^ y) >> 3) & 1) fill_rect(x + 2, y + 2, 4, 4, 0x109090);
        return;
    }
    fill_rect(0, 0, screen_w, sh, wallpaper_colour());
    blit(wallpaper_logo(), LOGO_W, screen_w / 2 + 60 - LOGO_W / 2, sh / 2 - 84, LOGO_W, LOGO_H);
}

static void paint_desktop(Rect region) {
    gfx_clip(rect_intersect(region, screen_rect()));
    gfx_origin(0, 0);
    paint_wallpaper(region);
    int count = desktop_icon_count();
    for (int i = 0; i < count; i++) {
        Rect cell = desktop_icon_rect(i);
        if (!rect_overlaps(cell, region)) continue;
        int app = desktop_icon_app(i);
        DirEntry *file = desktop_icon_file(i);
        const char *label = app >= 0 ? APPS[app].name : file->name;
        const Icon *icon = app >= 0 ? APPS[app].icon : file_icon(file->name, file->is_dir);
        draw_icon(icon, cell.x + (cell.w - 32) / 2, cell.y + 2, 2);
        /* the label on up to two lines of 11 characters, broken at a space */
        char lines[2][16] = { "", "" };
        const char *space = strchr(label, ' ');
        if (strlen(label) > 11 && space && space - label <= 11) {
            snprintf(lines[0], sizeof lines[0], "%.*s", (int)(space - label), label);
            snprintf(lines[1], sizeof lines[1], "%.9s%s", space + 1, strlen(space + 1) > 11 ? ".." : "");
        } else {
            snprintf(lines[0], sizeof lines[0], "%.9s%s", label, strlen(label) > 11 ? ".." : "");
            if (strlen(label) <= 11) snprintf(lines[0], sizeof lines[0], "%.15s", label);
        }
        for (int l = 0; l < 2 && lines[l][0]; l++) {
            int width = text_width(lines[l]);
            int lx = cell.x + (cell.w - width) / 2, ly = cell.y + 37 + l * 13;
            if (i == selected_icon) {
                fill_rect(lx - 2, ly - 1, width + 4, FONT_HEIGHT + 1, os_theme_colour(THEME_SELECTION));
                draw_text(lx, ly, lines[l], WHITE);
            } else {
                draw_text(lx + 1, ly + 1, lines[l], BLACK);
                draw_text(lx, ly, lines[l], WHITE);
            }
        }
    }
}

static int desktop_icon_at(int x, int y) {
    int count = desktop_icon_count();
    for (int i = 0; i < count; i++) {
        Rect cell = desktop_icon_rect(i);
        if (rect_contains(rect(cell.x + 10, cell.y, cell.w - 20, cell.h), x, y)) return i;
    }
    return -1;
}

static void desktop_open(int index) {
    int app = desktop_icon_app(index);
    if (app >= 0) {
        os_spawn(app, NULL);
        return;
    }
    DirEntry *file = desktop_icon_file(index);
    if (!file) return;
    char path[96];
    snprintf(path, sizeof path, "Desktop/%s", file->name);
    os_spawn(file->is_dir ? APP_FILES : os_app_for_file(path), path);
}

/* ---- the taskbar */

enum { START_W = 70, TRAY_W = 98 };

static Rect start_button(void) { return rect(2, screen_h - TASKBAR_H + 4, START_W, 22); }
static Rect tray_rect(void) { return rect(screen_w - TRAY_W - 2, screen_h - TASKBAR_H + 4, TRAY_W, 22); }

/* The windows the taskbar has a button for: every one but the dialogs, in the order opened. */
static int task_windows(Window **list) {
    int count = 0;
    for (int slot = 0; slot < MAX_WINDOWS; slot++)
        if (window_slots[slot].used && !window_slots[slot].modal_pid) list[count++] = &window_slots[slot];
    return count;
}

static Rect task_button(int index, int count) {
    int left = START_W + 8, right = screen_w - TRAY_W - 8;
    int width = count ? min_int(150, (right - left) / count) : 150;
    return rect(left + index * width, screen_h - TASKBAR_H + 4, width - 3, 22);
}

static void update_clock_text(void) {
    DateTime now;
    os_time(&now);
    if (os_settings()->clock_24h)
        snprintf(clock_text, sizeof clock_text, "%02d:%02d", now.hour, now.minute);
    else
        snprintf(clock_text, sizeof clock_text, "%d:%02d %s", (now.hour + 11) % 12 + 1, now.minute,
                 now.hour < 12 ? "AM" : "PM");
}

static void paint_taskbar(Rect region) {
    Rect bar = taskbar_rect();
    gfx_clip(rect_intersect(region, bar));
    gfx_origin(0, 0);
    fill_rect(bar.x, bar.y, bar.w, bar.h, FACE);
    hline(bar.x, bar.y, bar.w, LIGHT);
    hline(bar.x, bar.y + 1, bar.w, WHITE);
    Rect start = start_button();
    int popup_is_start(void);
    int pressed = popup_is_start();
    draw_button(start.x, start.y, start.w, start.h, "", pressed);
    draw_icon(&ICON_CHIP, start.x + 5 + pressed, start.y + 3 + pressed, 1);
    draw_text_bold(start.x + 24 + pressed, start.y + 5 + pressed, "Start", BLACK);
    Window *list[MAX_WINDOWS];
    int count = task_windows(list);
    for (int i = 0; i < count; i++) {
        Window *win = list[i];
        Rect button = task_button(i, count);
        if (!rect_overlaps(button, region)) continue;
        int active = win == top_window();
        fill_rect(button.x, button.y, button.w, button.h, active ? 0xe8e8e8 : FACE);
        if (active) {
            frame_rect(button.x, button.y, button.w, button.h, BLACK);
            hline(button.x + 1, button.y + 1, button.w - 2, SHADOW);
            vline(button.x + 1, button.y + 1, button.h - 2, SHADOW);
        } else {
            bevel_raised(button.x, button.y, button.w, button.h);
        }
        if (win->icon) draw_icon(win->icon, button.x + 4 + active, button.y + 3 + active, 1);
        Rect previous = gfx_get_clip();
        gfx_clip(rect_intersect(previous, rect(button.x, button.y, button.w - 4, button.h)));
        if (active) draw_text_bold(button.x + 23 + active, button.y + 5 + active, win->title, BLACK);
        else draw_text(button.x + 23, button.y + 5, win->title, BLACK);
        gfx_clip(previous);
    }
    Rect tray = tray_rect();
    bevel_thin_sunken(tray.x, tray.y, tray.w, tray.h);
    /* the processor meter: how busy it was in the last second */
    int meter_h = 14, filled = cpu_percent * meter_h / 100;
    fill_rect(tray.x + 5, tray.y + 4, 8, meter_h, BLACK);
    fill_rect(tray.x + 5, tray.y + 4 + meter_h - filled, 8, filled, cpu_percent > 80 ? RED : 0x00e000);
    draw_text(tray.x + 18 + (tray.w - 18 - text_width(clock_text)) / 2, tray.y + 5, clock_text, BLACK);
}

static int task_at(int x, int y, Window **found) {
    Window *list[MAX_WINDOWS];
    int count = task_windows(list);
    for (int i = 0; i < count; i++)
        if (rect_contains(task_button(i, count), x, y)) {
            *found = list[i];
            return 1;
        }
    return 0;
}

/* ------------------------------------------------------------------ menus */

/* The open menus: the first is a menu bar's, a context menu, the Start menu or the desktop's, and
 * each later one a submenu of the one before. */
enum { POPUP_BAR, POPUP_CONTEXT, POPUP_START, POPUP_DESKTOP };
enum { ITEM_H = 20, SEPARATOR_H = 8, BANNER_W = 24 };
typedef struct {
    const MenuItem *items;
    int count;
    Rect rect;
    int hover;
    int banner;
} Popup;
static Popup popups[MAX_POPUPS];
static int popup_count;
static int popup_kind;
static Window *popup_window;      /* the window of a menu bar's or a context menu */
static int popup_pid = -1;        /* the process waiting for a context menu's answer */
static int bar_index = -1;        /* the menu of the bar that is open */

int popup_is_start(void) { return popup_count && popup_kind == POPUP_START; }

static int item_height(const MenuItem *item) { return strcmp(item->label, "-") ? ITEM_H : SEPARATOR_H; }

static Rect popup_item_rect(const Popup *popup, int index) {
    int y = popup->rect.y + 3;
    for (int i = 0; i < index; i++) y += item_height(&popup->items[i]);
    int left = popup->rect.x + 3 + (popup->banner ? BANNER_W : 0);
    return rect(left, y, popup->rect.x + popup->rect.w - 3 - left, item_height(&popup->items[index]));
}

/* Opens a menu at level `level`, closing any deeper one: placed at x, y, or ending at x if it
 * would run off the right edge, and above y_up if it would run off the bottom. */
static void popup_open(int level, const MenuItem *items, int x, int y, int x_left, int y_up, int banner) {
    for (int i = level; i < popup_count; i++) invalidate(popups[i].rect);
    popup_count = level + 1;
    Popup *popup = &popups[level];
    popup->items = items;
    popup->hover = -1;
    popup->banner = banner;
    int count = 0, height = 6, width = 0;
    for (; items[count].label; count++) {
        height += item_height(&items[count]);
        int w = text_width(items[count].label) + (items[count].shortcut ? text_width(items[count].shortcut) + 24 : 0);
        width = max_int(width, w);
    }
    popup->count = count;
    width += 48 + (banner ? BANNER_W : 0);
    if (x + width > screen_w) x = max_int(0, x_left - width);
    if (y + height > screen_h - TASKBAR_H && !banner) y = max_int(0, y_up - height);
    if (banner) y = screen_h - TASKBAR_H - height + 2;
    popup->rect = rect(x, y, width, height);
    invalidate(popup->rect);
}

static void popup_close_all(void) {
    for (int i = 0; i < popup_count; i++) invalidate(popups[i].rect);
    if (popup_count && popup_kind == POPUP_BAR && popup_window) invalidate(menu_bar_rect(popup_window));
    if (popup_count && popup_kind == POPUP_START) invalidate(start_button());
    if (popup_count && popup_kind == POPUP_CONTEXT && popup_pid > 0) answer_process(popup_pid, 0);
    popup_count = 0;
    popup_pid = -1;
    popup_window = NULL;
    bar_index = -1;
}

static void popup_close_for(int pid) {
    if (popup_count && ((popup_window && popup_window->pid == pid) || popup_pid == pid)) {
        popup_pid = -1;
        popup_close_all();
    }
}

static void paint_popup(const Popup *popup, Rect region) {
    if (!rect_overlaps(popup->rect, region)) return;
    gfx_clip(rect_intersect(region, popup->rect));
    gfx_origin(0, 0);
    Rect r = popup->rect;
    fill_rect(r.x, r.y, r.w, r.h, FACE);
    bevel_raised(r.x, r.y, r.w, r.h);
    if (popup->banner) {
        int banner_h = r.h - 6;
        for (int i = 0; i < banner_h; i += 4) {
            int shade = 0x80 * i / banner_h;
            fill_rect(r.x + 3, r.y + 3 + banner_h - i - 4, BANNER_W, 4, (uint32_t)(shade << 8 | (0x40 + shade)));
        }
        draw_text_up(r.x + 8, r.y + r.h - 8, "asm", 0xc0c0ff);
        draw_text_up(r.x + 9, r.y + r.h - 8 - 3 * FONT_W, "OS", WHITE);
        draw_text_up(r.x + 8, r.y + r.h - 8 - 3 * FONT_W, "OS", WHITE);
    }
    for (int i = 0; i < popup->count; i++) {
        const MenuItem *item = &popup->items[i];
        Rect row = popup_item_rect(popup, i);
        if (!rect_overlaps(row, region)) continue;
        if (!strcmp(item->label, "-")) {
            hline(row.x + 2, row.y + 3, row.w - 4, SHADOW);
            hline(row.x + 2, row.y + 4, row.w - 4, WHITE);
            continue;
        }
        int disabled = item->flags & MENU_DISABLED, hover = i == popup->hover && !disabled;
        uint32_t ink = disabled ? SHADOW : hover ? WHITE : BLACK;
        if (hover) fill_rect(row.x, row.y, row.w, row.h, os_theme_colour(THEME_SELECTION));
        if (disabled) draw_text(row.x + 25, row.y + 5, item->label, WHITE);
        draw_text(row.x + 24, row.y + 4, item->label, ink);
        if (item->flags & MENU_CHECKED) {
            for (int k = 0; k < 2; k++) {
                draw_line(row.x + 7, row.y + 9 + k, row.x + 9, row.y + 11 + k, ink);
                draw_line(row.x + 9, row.y + 11 + k, row.x + 14, row.y + 6 + k, ink);
            }
        }
        if (item->shortcut)
            draw_text(row.x + row.w - 12 - text_width(item->shortcut), row.y + 4, item->shortcut, ink);
        if (item->submenu)
            for (int k = 0; k < 4; k++) vline(row.x + row.w - 12 + k, row.y + 6 + k, 8 - 2 * k, ink);
    }
}

/* The menu bar of a window: its menus' titles, the open one pushed in. */
void paint_menu_bar(Window *win, Rect bar) {
    int x = bar.x + 2;
    for (int i = 0; win->menu[i].title; i++) {
        int w = text_width(win->menu[i].title) + 14;
        int open = popup_count && popup_kind == POPUP_BAR && popup_window == win && bar_index == i;
        if (open) bevel_thin_sunken(x, bar.y + 1, w, bar.h - 2);
        draw_text(x + 7 + open, bar.y + 3 + open, win->menu[i].title, BLACK);
        x += w;
    }
}

static int menu_bar_at(Window *win, int px, int py, int *left) {
    Rect bar = menu_bar_rect(win);
    if (!rect_contains(bar, px, py)) return -1;
    int x = bar.x + 2;
    for (int i = 0; win->menu[i].title; i++) {
        int w = text_width(win->menu[i].title) + 14;
        if (px >= x && px < x + w) {
            *left = x;
            return i;
        }
        x += w;
    }
    return -1;
}

static void bar_open(Window *win, int index, int left) {
    popup_count = 0;
    popup_kind = POPUP_BAR;
    popup_window = win;
    bar_index = index;
    Rect bar = menu_bar_rect(win);
    popup_open(0, win->menu[index].items, left, bar.y + bar.h, left, bar.y, 0);
    invalidate(bar);
}

static void system_command(int id);

/* An item chosen: its command goes where the menu came from. */
static void popup_choose(const MenuItem *item) {
    int kind = popup_kind, pid = popup_pid;
    Window *win = popup_window;
    popup_pid = -1;
    popup_close_all();
    if (kind == POPUP_CONTEXT) {
        answer_process(pid, item->id);
    } else if (kind == POPUP_BAR && win && win->used) {
        if (win->pid > 0) {
            Message message = { 0 };
            message.type = MSG_COMMAND;
            message.win = win;
            message.id = item->id;
            post_message(win->pid, &message);
        }
    } else {
        system_command(item->id);
    }
}

static int popup_at(int x, int y, int *index) {
    for (int level = popup_count - 1; level >= 0; level--) {
        Popup *popup = &popups[level];
        if (!rect_contains(popup->rect, x, y)) continue;
        *index = -1;
        for (int i = 0; i < popup->count; i++)
            if (rect_contains(popup_item_rect(popup, i), x, y)) *index = i;
        return level;
    }
    return -1;
}

static void popup_hover(int level, int index) {
    Popup *popup = &popups[level];
    if (popup->hover != index) {
        if (popup->hover >= 0) invalidate(popup_item_rect(popup, popup->hover));
        popup->hover = index;
        if (index >= 0) invalidate(popup_item_rect(popup, index));
    }
    if (index >= 0 && popup->items[index].submenu && !(popup->items[index].flags & MENU_DISABLED)) {
        if (level + 1 < popup_count && popups[level + 1].items == popup->items[index].submenu) return;
        if (level + 1 < MAX_POPUPS) {
            Rect row = popup_item_rect(popup, index);
            popup_open(level + 1, popup->items[index].submenu, popup->rect.x + popup->rect.w - 4, row.y - 3,
                       popup->rect.x + 4, row.y + row.h + 3, 0);
        }
    } else if (index >= 0 && level + 1 < popup_count) {
        for (int i = level + 1; i < popup_count; i++) invalidate(popups[i].rect);
        popup_count = level + 1;
    }
}

int os_popup_menu(Window *win, int x, int y, const MenuItem *items) {
    lock();
    popup_close_all();
    Rect client = client_rect(win);
    popup_kind = POPUP_CONTEXT;
    popup_window = win;
    popup_pid = current->pid;
    popup_open(0, items, client.x + x, client.y + y, client.x + x, client.y + y, 0);
    return wait_for_answer();
}

/* The Start menu, made from the applications' table at start-up. */
enum { CMD_APP = 1000, CMD_RUN = 2000, CMD_SHUT_DOWN, CMD_NEW_TEXT, CMD_NEW_FOLDER, CMD_REFRESH,
       CMD_SETTINGS, CMD_ARRANGE };
static MenuItem group_items[3][APP_COUNT + 1];
static MenuItem programs_items[4] = {
    { "Accessories", 0, 0, NULL, group_items[GROUP_ACCESSORIES] },
    { "Games", 0, 0, NULL, group_items[GROUP_GAMES] },
    { "System", 0, 0, NULL, group_items[GROUP_SYSTEM] },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem start_items[] = {
    { "Programs", 0, 0, NULL, programs_items },
    { "Files", CMD_APP + APP_FILES, 0, NULL, NULL },
    { "Settings", CMD_APP + APP_SETTINGS, 0, NULL, NULL },
    { "About asmOS", CMD_APP + APP_ABOUT, 0, NULL, NULL },
    { "Run...", CMD_RUN, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Shut Down...", CMD_SHUT_DOWN, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem new_items[] = {
    { "Folder", CMD_NEW_FOLDER, 0, NULL, NULL },
    { "Text File", CMD_NEW_TEXT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem desktop_items[] = {
    { "New", 0, 0, NULL, new_items },
    { "Refresh", CMD_REFRESH, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Settings...", CMD_SETTINGS, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

static void start_menu_build(void) {
    int counts[3] = { 0, 0, 0 };
    for (int i = 0; i < APP_COUNT; i++) {
        int group = APPS[i].group;
        MenuItem *item = &group_items[group][counts[group]++];
        item->label = APPS[i].name;
        item->id = CMD_APP + i;
    }
}

static void start_menu_toggle(void) {
    if (popup_is_start()) {
        popup_close_all();
        return;
    }
    popup_close_all();
    popup_kind = POPUP_START;
    popup_open(0, start_items, 0, 0, 0, 0, 1);
    invalidate(start_button());
}

/* A file name in the Desktop folder that is not taken yet: "New Text File 2.txt" and so on. */
static void desktop_new_name(char *path, int size, const char *base, const char *extension) {
    for (int number = 1; number < 100; number++) {
        if (number == 1) snprintf(path, size, "Desktop/%s%s", base, extension);
        else snprintf(path, size, "Desktop/%s %d%s", base, number, extension);
        if (!os_stat(path, NULL)) return;
    }
}

void dialog_run(void);   /* dialogs.c: the Run box */

static void system_command(int id) {
    if (id >= CMD_APP && id < CMD_APP + APP_COUNT) {
        os_spawn(id - CMD_APP, NULL);
    } else if (id == CMD_SHUT_DOWN) {
        os_shutdown();
    } else if (id == CMD_RUN) {
        dialog_run();
    } else if (id == CMD_NEW_TEXT || id == CMD_NEW_FOLDER) {
        char path[96];
        os_make_dir("Desktop");
        desktop_new_name(path, sizeof path, id == CMD_NEW_TEXT ? "New Text File" : "New Folder",
                         id == CMD_NEW_TEXT ? ".txt" : "");
        if (id == CMD_NEW_TEXT) os_write_file(path, "", 0);
        else os_make_dir(path);
        desktop_refresh();
    } else if (id == CMD_REFRESH) {
        desktop_refresh();
        invalidate(screen_rect());
    } else if (id == CMD_SETTINGS) {
        os_spawn(APP_SETTINGS, NULL);
    }
}

/* ------------------------------------------------------------------ one dirty rectangle */

static void paint_region(Rect region) {
    /* begin with the highest window that covers the region whole: nothing below it shows */
    int first = -1;
    for (int i = window_count - 1; i >= 0 && first < 0; i--) {
        if (stack[i]->minimized) continue;
        Rect covered = rect_intersect(region, window_frame(stack[i]));
        if (covered.w == region.w && covered.h == region.h) first = i;
    }
    if (first < 0) {
        if (rect_overlaps(region, screen_rect())) paint_desktop(region);
        first = 0;
    }
    for (int i = first; i < window_count; i++) {
        Window *win = stack[i];
        if (win->minimized || !rect_overlaps(region, window_frame(win))) continue;
        Rect inside = rect_intersect(region, client_rect(win));
        if (inside.w != region.w || inside.h != region.h) paint_frame(win, region);
        paint_client(win, region);
    }
    if (rect_overlaps(region, taskbar_rect())) paint_taskbar(region);
    for (int i = 0; i < popup_count; i++) paint_popup(&popups[i], region);
}

/* Where the back buffer is behind the framebuffer: rectangles present_direct showed. */
static Rect stale;

/* Only the part asked for: the rest stays behind, and stale may then cover parts made fresh,
 * which costs only their painting again. */
static void back_refresh(Rect r) {
    if (rect_empty(stale) || !rect_overlaps(stale, r)) return;
    Rect part = rect_intersect(stale, r);
    if (part.x == stale.x && part.y == stale.y && part.w == stale.w && part.h == stale.h)
        stale = rect(0, 0, 0, 0);
    paint_region(part);
}

/* A dirty rectangle all inside the top window's picture, under no menu nor the taskbar, is that
 * picture alone, as paint_region would paint it: it goes to the framebuffer straight from there,
 * and the back buffer, left behind, is repainted when something reads it. */
static int present_direct(Rect r) {
    Window *top = top_window();
    if (!top || rect_overlaps(r, taskbar_rect())) return 0;
    for (int i = 0; i < popup_count; i++)
        if (rect_overlaps(r, popups[i].rect)) return 0;
    Rect client = client_rect(top);
    Rect picture = rect_intersect(client, rect(client.x, client.y, top->pw, top->ph));
    Rect inside = rect_intersect(r, picture);
    if (inside.w != r.w || inside.h != r.h || rect_empty(r)) return 0;
    copy_rect32(framebuffer + r.y * framebuffer_stride + r.x, framebuffer_stride * 4,
                top->pixels + (r.y - client.y) * top->pw + (r.x - client.x), top->pw * 4, r.w, r.h);
    show(r);
    stale = rect_empty(stale) ? r : rect_union(stale, r);
    return 1;
}

static void wm_render(void) {
    if (!dirty_count) return;
    int direct[MAX_DIRTY];
    for (int i = 0; i < dirty_count; i++)
        if (!(direct[i] = present_direct(dirty[i]))) paint_region(dirty[i]);
    int cursor_hit = 0, outline_hit = 0;
    Rect strips[4];
    outline_strips(outline, strips);
    for (int i = 0; i < dirty_count; i++) {
        if (!direct[i]) present(dirty[i]);
        if (rect_overlaps(dirty[i], cursor_rect())) cursor_hit = 1;
        for (int k = 0; k < 4; k++) outline_hit |= outline_shown && rect_overlaps(dirty[i], strips[k]);
    }
    dirty_count = 0;
    if (outline_hit) outline_draw();
    if (cursor_hit || !cursor_shown) cursor_draw();
}

/* ------------------------------------------------------------------ moving and resizing */

/* Makes dirty the parts of `whole` outside `hole`: up to four rectangles around it. */
static void invalidate_outside(Rect whole, Rect hole) {
    hole = rect_intersect(hole, whole);
    if (rect_empty(hole)) {
        invalidate(whole);
        return;
    }
    invalidate(rect(whole.x, whole.y, whole.w, hole.y - whole.y));
    invalidate(rect(whole.x, hole.y + hole.h, whole.w, whole.y + whole.h - hole.y - hole.h));
    invalidate(rect(whole.x, hole.y, hole.x - whole.x, hole.h));
    invalidate(rect(hole.x + hole.w, hole.y, whole.x + whole.w - hole.x - hole.w, hole.h));
}

static void window_move(Window *win, int x, int y) {
    x = clamp_int(x, 40 - win->w, screen_w - 40);
    y = clamp_int(y, 0, screen_h - TASKBAR_H - BORDER - 10);
    if (x == win->x && y == win->y) return;
    Rect before = window_frame(win);
    int dx = x - win->x, dy = y - win->y;
    if (win != top_window() || popup_count) {
        win->x = x;
        win->y = y;
        invalidate(before);
        invalidate(window_frame(win));
        return;
    }
    /* On top, the window's pixels in the back buffer are all its own, so they move as they are
     * and only what the move uncovers is repainted. Whatever is waiting to be repainted goes
     * first, while the window is still where those rectangles were measured. */
    wm_render();
    back_refresh(rect(0, 0, screen_w, screen_h));   /* the move copies the back buffer */
    Rect to = rect_intersect(rect(before.x + dx, before.y + dy, before.w, before.h), screen_rect());
    Rect from = rect_intersect(rect(to.x - dx, to.y - dy, to.w, to.h), screen_rect());
    to = rect(from.x + dx, from.y + dy, from.w, from.h);
    win->x = x;
    win->y = y;
    Rect after = window_frame(win);
    if (!rect_empty(to)) {
        move_rect32(back_buffer + to.y * screen_w + to.x, back_buffer + from.y * screen_w + from.x,
                    screen_w * 4, to.w, to.h);
        present(to);
        if (rect_overlaps(to, cursor_rect())) cursor_draw();
    }
    invalidate_outside(after, to);
    invalidate_outside(before, after);
}

/* Which edges of a resizable window's frame the point is on: within its border, and within 16
 * pixels of a corner counts for both edges. */
static int edges_at(Window *win, int x, int y) {
    if (!(win->flags & WIN_RESIZABLE) || win->maximized) return 0;
    Rect frame = window_frame(win);
    if (!rect_contains(frame, x, y)) return 0;
    Rect inner = rect(frame.x + BORDER, frame.y + BORDER, frame.w - 2 * BORDER, frame.h - 2 * BORDER);
    if (rect_contains(inner, x, y)) return 0;
    int edges = 0, corner = 16;
    if (x < frame.x + corner) edges |= EDGE_LEFT;
    if (x >= frame.x + frame.w - corner) edges |= EDGE_RIGHT;
    if (y < frame.y + corner) edges |= EDGE_TOP;
    if (y >= frame.y + frame.h - corner) edges |= EDGE_BOTTOM;
    return edges;
}

static Rect resized_frame(Window *win, int x, int y) {
    Rect r = resize_start;
    int dx = x - press_x, dy = y - press_y;
    int extra_w = r.w - client_rect(win).w, extra_h = r.h - client_rect(win).h;
    int min_w = win->min_w + extra_w, min_h = win->min_h + extra_h;
    if (resize_edges & EDGE_LEFT) {
        int w = max_int(min_w, r.w - dx);
        r.x += r.w - w;
        r.w = w;
    }
    if (resize_edges & EDGE_RIGHT) r.w = max_int(min_w, r.w + dx);
    if (resize_edges & EDGE_TOP) {
        int h = max_int(min_h, r.h - dy);
        r.y += r.h - h;
        r.h = h;
    }
    if (resize_edges & EDGE_BOTTOM) r.h = max_int(min_h, r.h + dy);
    return r;
}

/* ------------------------------------------------------------------ the pointer's events */

static Window *window_at(int x, int y) {
    for (int i = window_count - 1; i >= 0; i--)
        if (!stack[i]->minimized && rect_contains(window_frame(stack[i]), x, y)) return stack[i];
    return NULL;
}

static void send_to_client(Window *win, const MouseEvent *event) {
    Rect client = client_rect(win);
    Message message = { 0 };
    message.type = event->kind == MOUSE_DOWN ? MSG_MOUSE_DOWN
                   : event->kind == MOUSE_UP ? MSG_MOUSE_UP
                   : event->kind == MOUSE_MOVE ? MSG_MOUSE_MOVE : MSG_MOUSE_WHEEL;
    message.win = win;
    message.x = event->x - client.x;
    message.y = event->y - client.y;
    message.button = event->button;
    message.buttons = event->buttons;
    message.clicks = event->clicks;
    message.wheel = event->wheel;
    message.mods = key_modifiers();
    if (win->pid > 0) post_message(win->pid, &message);
    else if (win->on_mouse) win->on_mouse(win, &message);
}

static void select_icon(int index) {
    if (index == selected_icon) return;
    if (selected_icon >= 0) invalidate(desktop_icon_rect(selected_icon));
    selected_icon = index;
    if (index >= 0) invalidate(desktop_icon_rect(index));
}

void os_drag_file(const char *path) {
    lock();
    if (held == HELD_CLIENT || held == HELD_ICON) {
        snprintf(drag_path, sizeof drag_path, "%s", path);
        cursor_hide();
        drag_active = 1;
        cursor_draw();
        held = HELD_NOTHING;
        held_window = NULL;
    }
    unlock();
}

/* A file dropped: on a window, its process gets MSG_DROP; on the desktop, it is copied into the
 * Desktop folder. */
static void drop_file(int x, int y) {
    cursor_hide();
    drag_active = 0;
    cursor_draw();
    Window *win = window_at(x, y);
    if (win) {
        if (win->pid <= 0) return;
        Rect client = client_rect(win);
        Message message = { 0 };
        message.type = MSG_DROP;
        message.win = win;
        message.x = x - client.x;
        message.y = y - client.y;
        snprintf(message.text, sizeof message.text, "%s", drag_path);
        window_focus(win);
        post_message(win->pid, &message);
    } else if (rect_contains(screen_rect(), x, y) && strncmp(drag_path, "Desktop/", 8) != 0) {
        char to[160];
        os_make_dir("Desktop");
        snprintf(to, sizeof to, "Desktop/%s", os_base_name(drag_path));
        if (os_stat(drag_path, NULL) == 1) os_copy_file(drag_path, to);
        desktop_refresh();
    }
}

static void press(const MouseEvent *event) {
    int x = event->x, y = event->y;
    if (popup_count) {
        int index, level = popup_at(x, y, &index);
        if (level >= 0) {
            if (index >= 0) {
                const MenuItem *item = &popups[level].items[index];
                if (strcmp(item->label, "-") && !(item->flags & MENU_DISABLED) && !item->submenu)
                    popup_choose(item);
            }
            return;
        }
        int on_start = popup_is_start() && rect_contains(start_button(), x, y);
        Window *bar_window = popup_kind == POPUP_BAR ? popup_window : NULL;
        int left, bar = bar_window ? menu_bar_at(bar_window, x, y, &left) : -1;
        popup_close_all();
        if (on_start || bar >= 0) return;
    }
    if (rect_contains(taskbar_rect(), x, y)) {
        if (event->button != BUTTON_LEFT) return;
        Window *win;
        if (rect_contains(start_button(), x, y)) start_menu_toggle();
        else if (task_at(x, y, &win)) {
            if (win == top_window()) window_minimize(win);
            else window_focus(win);
        }
        return;
    }
    Window *win = window_at(x, y);
    if (!win) {
        int icon = desktop_icon_at(x, y);
        select_icon(icon);
        if (event->button == BUTTON_RIGHT && icon < 0) {
            popup_kind = POPUP_DESKTOP;
            popup_open(0, desktop_items, x, y, x, y, 0);
        } else if (icon >= 0 && event->clicks == 2 && event->button == BUTTON_LEFT) {
            select_icon(-1);
            desktop_open(icon);
        } else if (icon >= 0 && desktop_icon_file(icon) && event->button == BUTTON_LEFT) {
            held = HELD_ICON;
            held_icon = icon;
            press_x = x;
            press_y = y;
        }
        return;
    }
    select_icon(-1);
    Window *dialog = dialog_of(win->pid);
    if (dialog && dialog != win) {
        window_focus(win);
        return;
    }
    window_focus(win);
    held_window = win;
    held_inside = 1;
    press_x = x;
    press_y = y;
    int edges = edges_at(win, x, y), left, bar;
    if (rect_contains(client_rect(win), x, y)) {
        held = HELD_CLIENT;
        send_to_client(win, event);
    } else if (event->button != BUTTON_LEFT) {
        held = HELD_NOTHING;
    } else if (edges) {
        held = HELD_RESIZE;
        resize_edges = edges;
        resize_start = window_frame(win);
        outline = resize_start;
        outline_draw();
    } else if (rect_contains(close_button(win), x, y)) {
        held = HELD_CLOSE;
        invalidate(close_button(win));
    } else if (rect_contains(maximize_button(win), x, y)) {
        held = HELD_MAXIMIZE;
        invalidate(maximize_button(win));
    } else if (rect_contains(minimize_button(win), x, y)) {
        held = HELD_MINIMIZE;
        invalidate(minimize_button(win));
    } else if (has_menu(win) && (bar = menu_bar_at(win, x, y, &left)) >= 0) {
        held = HELD_NOTHING;
        bar_open(win, bar, left);
    } else if (rect_contains(title_rect(win), x, y)) {
        if (event->clicks == 2 && win->flags & WIN_RESIZABLE) {
            held = HELD_NOTHING;
            window_toggle_maximize(win);
        } else if (!win->maximized) {
            held = HELD_MOVE;
            drag_dx = x - win->x;
            drag_dy = y - win->y;
        }
    }
}

static void request_close(Window *win) {
    if (win->pid > 0) {
        Message message = { 0 };
        message.type = MSG_CLOSE;
        message.win = win;
        post_message(win->pid, &message);
    } else if (win->on_key) {
        Message message = { 0 };
        message.type = MSG_KEY;
        message.key = KEY_ESC;
        win->on_key(win, &message);
    }
}

static void release(const MouseEvent *event) {
    Window *win = held_window;
    int what = held;
    held = HELD_NOTHING;
    held_window = NULL;
    if (drag_active) {
        drop_file(event->x, event->y);
    } else if (what == HELD_CLIENT && win) {
        send_to_client(win, event);
    } else if (what == HELD_RESIZE && win) {
        Rect frame = resized_frame(win, event->x, event->y);
        outline_hide();
        window_set_frame(win, frame);
    } else if (what == HELD_ICON) {
        held_icon = -1;
    } else if (win) {
        Rect button = what == HELD_CLOSE ? close_button(win)
                      : what == HELD_MAXIMIZE ? maximize_button(win)
                      : what == HELD_MINIMIZE ? minimize_button(win) : rect(0, 0, 0, 0);
        invalidate(button);
        if (!rect_contains(button, event->x, event->y)) return;
        if (what == HELD_CLOSE) request_close(win);
        if (what == HELD_MAXIMIZE) window_toggle_maximize(win);
        if (what == HELD_MINIMIZE) window_minimize(win);
    }
}

static void motion(const MouseEvent *event) {
    int x = event->x, y = event->y;
    if (popup_count) {
        int index, level = popup_at(x, y, &index);
        if (level >= 0) popup_hover(level, index);
        int left, bar;
        if (popup_kind == POPUP_BAR && popup_window && (bar = menu_bar_at(popup_window, x, y, &left)) >= 0 &&
            bar != bar_index)
            bar_open(popup_window, bar, left);
    }
    Window *win = held_window;
    if (drag_active) return;
    if (held == HELD_MOVE && win) {
        window_move(win, x - drag_dx, y - drag_dy);
    } else if (held == HELD_RESIZE && win) {
        Rect frame = resized_frame(win, x, y);
        if (frame.x != outline.x || frame.y != outline.y || frame.w != outline.w || frame.h != outline.h) {
            outline_hide();
            outline = frame;
            outline_draw();
            cursor_draw();
        }
    } else if (held == HELD_CLIENT && win) {
        send_to_client(win, event);
    } else if (held == HELD_ICON) {
        if (abs(x - press_x) + abs(y - press_y) > 4) {
            DirEntry *file = desktop_icon_file(held_icon);
            if (file && !file->is_dir) {
                char path[96];
                snprintf(path, sizeof path, "Desktop/%s", file->name);
                lock();
                os_drag_file(path);
                kernel_lock--;
            }
            held = HELD_NOTHING;
        }
    } else if (held == HELD_CLOSE || held == HELD_MAXIMIZE || held == HELD_MINIMIZE) {
        Rect button = held == HELD_CLOSE ? close_button(win)
                      : held == HELD_MAXIMIZE ? maximize_button(win) : minimize_button(win);
        int inside = rect_contains(button, x, y);
        if (inside != held_inside) {
            held_inside = inside;
            invalidate(button);
        }
    } else if (!held) {
        /* a window that follows the pointer without a press, such as a game's or Paint's */
        Window *under = window_at(x, y);
        if (under && under == top_window() && rect_contains(client_rect(under), x, y)) send_to_client(under, event);
    }
}

void wm_mouse_event(const MouseEvent *event) {
    cursor_move(event->x, event->y);
    switch (event->kind) {
    case MOUSE_DOWN:
        if (!held && !drag_active) press(event);
        else if (held == HELD_CLIENT && held_window) send_to_client(held_window, event);
        break;
    case MOUSE_UP:
        if (event->buttons == 0) release(event);
        else if (held == HELD_CLIENT && held_window) send_to_client(held_window, event);
        break;
    case MOUSE_MOVE:
        motion(event);
        break;
    case MOUSE_WHEEL: {
        Window *win = window_at(event->x, event->y);
        if (win && !popup_count) send_to_client(win, event);
        break;
    }
    }
}

/* ------------------------------------------------------------------ keys */

static void popup_key(const KeyEvent *event) {
    Popup *popup = &popups[popup_count - 1];
    int level = popup_count - 1;
    if (event->code == KEY_ESC) {
        if (popup_count > 1) {
            invalidate(popup->rect);
            popup_count--;
        } else {
            popup_close_all();
        }
    } else if (event->code == KEY_UP || event->code == KEY_DOWN) {
        int step = event->code == KEY_UP ? -1 : 1, index = popup->hover;
        for (int tries = 0; tries < popup->count; tries++) {
            index = (index + step + popup->count) % popup->count;
            if (index < 0) index = popup->count - 1;
            if (strcmp(popup->items[index].label, "-") && !(popup->items[index].flags & MENU_DISABLED)) break;
        }
        popup_hover(level, index);
        if (level + 1 < popup_count) {
            for (int i = level + 1; i < popup_count; i++) invalidate(popups[i].rect);
            popup_count = level + 1;
        }
    } else if (event->code == KEY_RIGHT && popup->hover >= 0 && popup->items[popup->hover].submenu) {
        popup_hover(level, popup->hover);
        if (popup_count > level + 1) popup_hover(level + 1, 0);
    } else if (event->code == KEY_LEFT && popup_count > 1) {
        invalidate(popup->rect);
        popup_count--;
    } else if ((event->code == KEY_LEFT || event->code == KEY_RIGHT) && popup_kind == POPUP_BAR) {
        int count = 0;
        while (popup_window->menu[count].title) count++;
        int next = (bar_index + (event->code == KEY_LEFT ? count - 1 : 1)) % count, left = menu_bar_rect(popup_window).x + 2;
        for (int i = 0; i < next; i++) left += text_width(popup_window->menu[i].title) + 14;
        bar_open(popup_window, next, left);
    } else if (event->code == KEY_ENTER && popup->hover >= 0) {
        const MenuItem *item = &popup->items[popup->hover];
        if (item->submenu) popup_hover(level, popup->hover);
        else if (!(item->flags & MENU_DISABLED)) popup_choose(item);
    }
}

static void cycle_windows(void) {
    if (window_count) window_focus(stack[0]);
}

void wm_key_event(const KeyEvent *event, int released) {
    Window *win = top_window();
    if (released) {
        if (win && win->pid > 0 && !popup_count) {
            Message message = { 0 };
            message.type = MSG_KEY_UP;
            message.win = win;
            message.key = event->code;
            message.mods = event->mods;
            post_message(win->pid, &message);
        }
        return;
    }
    if (event->code == KEY_LEFTMETA || event->code == KEY_RIGHTMETA ||
        (event->code == KEY_ESC && event->mods & MOD_CTRL)) {
        start_menu_toggle();
        return;
    }
    if (popup_count) {
        popup_key(event);
        return;
    }
    if (event->code == KEY_F10 && win && has_menu(win)) {
        bar_open(win, 0, menu_bar_rect(win).x + 2);
        popup_hover(0, 0);
        return;
    }
    if (event->mods & MOD_ALT && event->code == KEY_F4) {
        if (win) request_close(win);
        return;
    }
    if (event->mods & MOD_ALT && event->code == KEY_TAB) {
        cycle_windows();
        return;
    }
    if (!win) return;
    Message message = { 0 };
    message.type = MSG_KEY;
    message.win = win;
    message.key = event->code;
    message.ch = event->ch;
    message.mods = event->mods;
    if (win->pid > 0) post_message(win->pid, &message);
    else if (win->on_key) win->on_key(win, &message);
}

/* ------------------------------------------------------------------ time and start-up */

static void wm_tick_second(void) {
    char before[sizeof clock_text];
    memcpy(before, clock_text, sizeof before);
    update_clock_text();
    int busy;
    long instructions;
    os_cpu_load(&busy, &instructions);
    if (strcmp(before, clock_text) || busy != cpu_percent) invalidate(tray_rect());
    cpu_percent = busy;
    desktop_refresh();
}

static void wm_start(uint32_t *fb, int stride, uint32_t *back) {
    framebuffer = fb;
    framebuffer_stride = stride;
    back_buffer = back;
    gfx_target(back, screen_w, screen_h, screen_w);
    start_menu_build();
    update_clock_text();
    desktop_refresh();
    wallpaper_load();
    cursor_x = screen_w / 2;
    cursor_y = screen_h / 2;
    invalidate(rect(0, 0, screen_w, screen_h));
}
