/*
 * Small pieces the applications share: buttons that track the pointer, a vertical scroll bar, and
 * string helpers.
 */

/* ------------------------------------------------------------------ buttons */

/* A set of buttons in a window: the one the pointer pressed shows pushed in while the pointer
 * stays on it, and counts as clicked if the button is released there. */
typedef struct {
    int pressed;        /* the button held down, or -1 */
    int inside;         /* whether the pointer is still on it */
} ButtonTracker;

static int button_at(const Rect *buttons, int count, int x, int y) {
    for (int i = 0; i < count; i++)
        if (rect_contains(buttons[i], x, y)) return i;
    return -1;
}

/* Feeds a message to a set of buttons; answers the button clicked, or -1. `changed` is set when a
 * button's look changed and the window should draw it again. */
static int track_buttons(ButtonTracker *tracker, const Rect *buttons, int count, const Message *message,
                         int *changed) {
    int at = button_at(buttons, count, message->x, message->y);
    *changed = 0;
    if (message->type == MSG_MOUSE_DOWN && message->button == BUTTON_LEFT) {
        tracker->pressed = at;
        tracker->inside = 1;
        *changed = at >= 0;
        return -1;
    }
    if (tracker->pressed < 0) return -1;
    if (message->type == MSG_MOUSE_MOVE) {
        int inside = at == tracker->pressed;
        if (inside != tracker->inside) {
            tracker->inside = inside;
            *changed = 1;
        }
    } else if (message->type == MSG_MOUSE_UP && message->button == BUTTON_LEFT) {
        int clicked = at == tracker->pressed ? at : -1;
        tracker->pressed = -1;
        *changed = 1;
        return clicked;
    }
    return -1;
}

static int button_down(const ButtonTracker *tracker, int index) {
    return tracker->pressed == index && tracker->inside;
}

/* ------------------------------------------------------------------ the scroll bar */

/* A vertical scroll bar: arrows at both ends and a thumb whose size is the part shown. */
typedef struct {
    Rect r;              /* where it is, 16 pixels wide */
    int total, visible;  /* rows in all, and shown */
    int top;             /* the first row shown */
    int dragging;        /* the thumb is held: the offset of the pointer in it */
    int held;            /* -1 or 1 while an arrow is held */
} ScrollBar;

static int scroll_max(const ScrollBar *bar) { return max_int(0, bar->total - bar->visible); }

static Rect scroll_thumb(const ScrollBar *bar) {
    int track = bar->r.h - 32;
    if (bar->total <= bar->visible || track <= 8) return rect(0, 0, 0, 0);
    int size = max_int(10, track * bar->visible / bar->total);
    int y = bar->r.y + 16 + (track - size) * bar->top / scroll_max(bar);
    return rect(bar->r.x, y, bar->r.w, size);
}

static void draw_scroll_arrow(int x, int y, int up) {
    for (int i = 0; i < 4; i++) {
        int row = up ? y + 6 + i : y + 9 - i;
        hline(x + 7 - i, row, 1 + 2 * i, BLACK);
    }
}

static void draw_scroll_bar(const ScrollBar *bar) {
    Rect r = bar->r;
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x + (y & 1); x < r.x + r.w; x += 2) put_pixel(x, y, WHITE);
    for (int y = r.y; y < r.y + r.h; y++)
        for (int x = r.x + 1 - (y & 1); x < r.x + r.w; x += 2) put_pixel(x, y, FACE);
    draw_button(r.x, r.y, r.w, 16, "", bar->held == -1);
    draw_scroll_arrow(r.x, r.y, 1);
    draw_button(r.x, r.y + r.h - 16, r.w, 16, "", bar->held == 1);
    draw_scroll_arrow(r.x, r.y + r.h - 16, 0);
    Rect thumb = scroll_thumb(bar);
    if (thumb.h) draw_button(thumb.x, thumb.y, thumb.w, thumb.h, "", 0);
}

/* Feeds a message to the scroll bar; answers 1 when it took it (`top` may have changed). */
static int scroll_bar_message(ScrollBar *bar, const Message *message) {
    Rect r = bar->r;
    if (message->type == MSG_MOUSE_DOWN && message->button == BUTTON_LEFT && rect_contains(r, message->x, message->y)) {
        Rect thumb = scroll_thumb(bar);
        if (message->y < r.y + 16) {
            bar->top = max_int(0, bar->top - 1);
            bar->held = -1;
        } else if (message->y >= r.y + r.h - 16) {
            bar->top = min_int(scroll_max(bar), bar->top + 1);
            bar->held = 1;
        } else if (rect_contains(thumb, message->x, message->y)) {
            bar->dragging = message->y - thumb.y + 1;
        } else {
            int page = max_int(1, bar->visible - 1);
            bar->top = clamp_int(bar->top + (message->y < thumb.y ? -page : page), 0, scroll_max(bar));
        }
        return 1;
    }
    if (message->type == MSG_MOUSE_MOVE && bar->dragging) {
        int track = r.h - 32, size = scroll_thumb(bar).h;
        if (track > size) {
            int y = message->y - (bar->dragging - 1) - (r.y + 16);
            bar->top = clamp_int((y * scroll_max(bar) + (track - size) / 2) / (track - size), 0, scroll_max(bar));
        }
        return 1;
    }
    if (message->type == MSG_MOUSE_UP && (bar->dragging || bar->held)) {
        bar->dragging = 0;
        bar->held = 0;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ strings */

static void copy_text(char *to, size_t size, const char *from) {
    size_t i = 0;
    for (; i + 1 < size && from[i]; i++) to[i] = from[i];
    to[i] = 0;
}

static int ends_with(const char *text, const char *suffix) {
    size_t length = strlen(text), suffix_length = strlen(suffix);
    return length >= suffix_length && strcmp(text + length - suffix_length, suffix) == 0;
}
