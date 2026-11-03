/*
 * Paint: a picture to draw on, ten tools and a palette of 28 colours. The left button paints with
 * the first colour and the right button with the second; a click on the palette chooses them the
 * same way, and a double click changes a colour of the palette. A new picture is 400 by 260
 * pixels, and Image > Canvas Size makes it any size up to 1024 by 768; an opened .bmp keeps its
 * size, and scroll bars show the rest of a picture larger than the window.
 *
 * The Select tool drags out a rectangle, which can then be dragged elsewhere (with Ctrl, a copy
 * of it), flipped, inverted, cut or copied. The clipboard holds pictures every Paint window
 * shares: Paste puts one at the top left as a selection that floats over the picture until a
 * click elsewhere puts it down. Undo goes back through the last few changes.
 */

enum { PAINT_NEW_W = 400, PAINT_NEW_H = 260, PAINT_MAX_W = 1024, PAINT_MAX_H = 768 };
enum { PAINT_TOOLBOX_W = 58, PAINT_PALETTE_H = 40, PAINT_STATUS_H = 20, PAINT_MARGIN = 4, PAINT_BAR = 16,
       PAINT_STEP = 8, PAINT_SWATCH = 14, PAINT_COLOURS = 28 };
/* each level of Undo is a copy of the picture: a machine of a few megabytes keeps two */
enum { PAINT_UNDO_LEVELS = PLATFORM_SMALL_MEMORY ? 2 : 5 };
enum { PAINT_BACKGROUND = 0x808080 };
enum { PAINT_SELECT, PAINT_PICK, PAINT_PENCIL, PAINT_BRUSH, PAINT_ERASER, PAINT_FILL, PAINT_LINE, PAINT_SPRAY,
       PAINT_RECT, PAINT_ELLIPSE, PAINT_TOOL_COUNT };
/* What the button held down is doing. */
enum { PAINT_HOLD_NONE, PAINT_HOLD_STROKE, PAINT_HOLD_SHAPE, PAINT_HOLD_SELECT, PAINT_HOLD_MOVE };
enum { PAINT_NEW = 1, PAINT_OPEN, PAINT_SAVE, PAINT_SAVE_AS, PAINT_EXIT, PAINT_UNDO, PAINT_REDO, PAINT_CUT,
       PAINT_COPY, PAINT_PASTE, PAINT_CLEAR_SELECTION, PAINT_SELECT_ALL, PAINT_FLIP_H, PAINT_FLIP_V,
       PAINT_INVERT, PAINT_CANVAS_SIZE, PAINT_CLEAR_IMAGE, PAINT_ABOUT };
enum { PAINT_SPRAY_TIMER = 1 };

static MenuItem paint_file_menu[] = {
    { "New", PAINT_NEW, 0, "Ctrl+N", NULL },
    { "Open...", PAINT_OPEN, 0, "Ctrl+O", NULL },
    { "Save", PAINT_SAVE, 0, "Ctrl+S", NULL },
    { "Save As...", PAINT_SAVE_AS, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", PAINT_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
/* Each window has a copy of the Edit menu, whose items it enables and disables. */
static const MenuItem PAINT_EDIT_MENU[] = {
    { "Undo", PAINT_UNDO, 0, "Ctrl+Z", NULL },
    { "Redo", PAINT_REDO, 0, "Ctrl+Y", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Cut", PAINT_CUT, 0, "Ctrl+X", NULL },
    { "Copy", PAINT_COPY, 0, "Ctrl+C", NULL },
    { "Paste", PAINT_PASTE, 0, "Ctrl+V", NULL },
    { "Clear Selection", PAINT_CLEAR_SELECTION, 0, "Del", NULL },
    { "Select All", PAINT_SELECT_ALL, 0, "Ctrl+A", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem paint_image_menu[] = {
    { "Flip Horizontal", PAINT_FLIP_H, 0, NULL, NULL },
    { "Flip Vertical", PAINT_FLIP_V, 0, NULL, NULL },
    { "Invert Colours", PAINT_INVERT, 0, "Ctrl+I", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Canvas Size...", PAINT_CANVAS_SIZE, 0, NULL, NULL },
    { "Clear Image", PAINT_CLEAR_IMAGE, 0, "Ctrl+Shift+N", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem paint_help_menu[] = {
    { "About Paint", PAINT_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem PAINT_CONTEXT_MENU[] = {
    { "Cut", PAINT_CUT, 0, NULL, NULL },
    { "Copy", PAINT_COPY, 0, NULL, NULL },
    { "Paste", PAINT_PASTE, 0, NULL, NULL },
    { "Clear Selection", PAINT_CLEAR_SELECTION, 0, NULL, NULL },
    { "Select All", PAINT_SELECT_ALL, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Flip Horizontal", PAINT_FLIP_H, 0, NULL, NULL },
    { "Flip Vertical", PAINT_FLIP_V, 0, NULL, NULL },
    { "Invert Colours", PAINT_INVERT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

static const char *const PAINT_TOOL_NAMES[PAINT_TOOL_COUNT] = {
    "Select", "Pick Colour", "Pencil", "Brush", "Eraser", "Fill", "Line", "Airbrush", "Rectangle", "Ellipse",
};
static const char *const PAINT_TOOL_HINTS[PAINT_TOOL_COUNT] = {
    "drag out, then drag it", "click a colour", "one pixel wide", "a round brush", "paints the second colour",
    "fills one colour's area", "Shift keeps it straight", "hold the button down", "Shift makes a square",
    "Shift makes a circle",
};
/* The three sizes a tool offers, in pixels across, or none. */
static const int PAINT_SIZES[PAINT_TOOL_COUNT][3] = {
    [PAINT_BRUSH] = { 3, 5, 9 }, [PAINT_ERASER] = { 6, 10, 16 }, [PAINT_SPRAY] = { 9, 15, 23 },
    [PAINT_LINE] = { 1, 3, 5 }, [PAINT_RECT] = { 1, 3, 5 }, [PAINT_ELLIPSE] = { 1, 3, 5 },
};
static const uint32_t PAINT_PALETTE[PAINT_COLOURS] = {
    0x000000, 0x808080, 0x800000, 0x808000, 0x008000, 0x008080, 0x000080, 0x800080, 0x808040, 0x004040,
    0x0080ff, 0x004080, 0x8000ff, 0x804000,
    0xffffff, 0xc0c0c0, 0xff0000, 0xffff00, 0x00ff00, 0x00ffff, 0x0000ff, 0xff00ff, 0xffff80, 0x00ff80,
    0x80ffff, 0x8080ff, 0xff0080, 0xff8040,
};

/* A picture Undo keeps. */
typedef struct {
    uint32_t *pixels;
    int w, h;
} PaintPicture;

typedef struct {
    Window *win;
    int w, h;                       /* the client area */
    uint32_t *canvas;               /* the picture, cw by ch */
    int cw, ch;
    int tool, last_tool;            /* last_tool: the one Pick Colour goes back to */
    int size[PAINT_TOOL_COUNT];     /* each tool's choice of PAINT_SIZES */
    int style;                      /* rectangles and ellipses: outlined, outlined and filled, filled */
    uint32_t colours[2];            /* the left button's and the right button's */
    uint32_t palette[PAINT_COLOURS];
    int hold, button;               /* what the button held down does, and which button it is */
    int x0, y0, x1, y1;             /* a stroke's last point, or a shape's or a new selection's corners */
    int selected;                   /* there is a selection: `sel`, in the picture's pixels */
    Rect sel;
    uint32_t *floating;             /* the selection's pixels lifted off the picture, or NULL */
    int grab_x, grab_y;             /* where the pointer holds the selection it moves */
    int copying;                    /* Ctrl was held as the move began: the original stays */
    PaintPicture undo[PAINT_UNDO_LEVELS], redo[PAINT_UNDO_LEVELS];
    int undo_count, redo_count;
    int sx, sy;                     /* how far the view is scrolled, in pixels */
    Rect port;                      /* where the picture shows in the client area */
    ScrollBar vbar, hbar;           /* hbar is turned a quarter: see paint_hbar_message */
    int has_vbar, has_hbar;
    int pointer_x, pointer_y;       /* the pixel under the pointer, or -1 */
    int modified;
    char path[128];
    char message[80];
    MenuItem edit_menu[sizeof PAINT_EDIT_MENU / sizeof PAINT_EDIT_MENU[0]];
    Menu menus[5];
} Paint;

/* ------------------------------------------------------------------ geometry */

static int paint_view_bottom(const Paint *p) { return p->h - PAINT_PALETTE_H - PAINT_STATUS_H; }
/* Where the picture's pixel 0, 0 is in the client area. */
static int paint_ox(const Paint *p) { return p->port.x + PAINT_MARGIN - p->sx; }
static int paint_oy(const Paint *p) { return p->port.y + PAINT_MARGIN - p->sy; }
static int paint_max_sx(const Paint *p) { return max_int(0, p->cw + 2 * PAINT_MARGIN - p->port.w); }
static int paint_max_sy(const Paint *p) { return max_int(0, p->ch + 2 * PAINT_MARGIN - p->port.h); }

static Rect paint_tool_rect(int tool) { return rect(6 + tool % 2 * 24, 4 + tool / 2 * 24, 23, 23); }
static Rect paint_options_rect(void) { return rect(6, 128, 47, 84); }
static Rect paint_size_rect(int index) { return rect(9, 131 + index * 14, 41, 13); }
static Rect paint_style_rect(int index) { return rect(9, 175 + index * 12, 41, 11); }
static Rect paint_colours_rect(const Paint *p) { return rect(6, paint_view_bottom(p) + 5, 30, 29); }
static Rect paint_swatch_rect(const Paint *p, int index) {
    return rect(40 + index % 14 * (PAINT_SWATCH + 1), paint_view_bottom(p) + 5 + index / 14 * (PAINT_SWATCH + 1),
                PAINT_SWATCH, PAINT_SWATCH);
}

static Rect paint_pad(Rect r, int pad) { return rect(r.x - pad, r.y - pad, r.w + 2 * pad, r.h + 2 * pad); }
static Rect paint_corners(int x0, int y0, int x1, int y1) {
    return rect(min_int(x0, x1), min_int(y0, y1), abs(x1 - x0) + 1, abs(y1 - y0) + 1);
}
static Rect paint_transpose(Rect r) { return rect(r.y, r.x, r.h, r.w); }

static int paint_size(const Paint *p) {
    const int *sizes = PAINT_SIZES[p->tool];
    return sizes[0] ? sizes[p->size[p->tool]] : 1;
}

/* The selection being dragged out. */
static Rect paint_band(const Paint *p) { return paint_corners(p->x0, p->y0, p->x1, p->y1); }

/* ------------------------------------------------------------------ pens and shapes */

static void paint_dot(int x, int y, int size, uint32_t colour) {
    if (size <= 1) put_pixel(x, y, colour);
    else fill_rect(x - size / 2, y - size / 2, size, size, colour);
}

/* A line drawn with a square pen `size` pixels wide, or a round one when `round`. */
static void paint_line(int x0, int y0, int x1, int y1, int size, int round, uint32_t colour) {
    if (size <= 1) {
        draw_line(x0, y0, x1, y1, colour);
        return;
    }
    if (!round) {
        thick_line(x0, y0, x1, y1, size, colour);
        return;
    }
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, error = dx + dy;
    for (;;) {
        fill_circle(x0, y0, size / 2, colour);
        if (x0 == x1 && y0 == y1) break;
        int twice = 2 * error;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

/* A rectangle from corner to corner: style 0 outlined, 1 outlined and filled with `fill`, 2 filled
 * with the line's colour. The outline lies inside the corners. */
static void paint_box(int x0, int y0, int x1, int y1, int size, int style, uint32_t line, uint32_t fill) {
    Rect r = paint_corners(x0, y0, x1, y1);
    if (style == 2) {
        fill_rect(r.x, r.y, r.w, r.h, line);
        return;
    }
    if (style == 1) fill_rect(r.x, r.y, r.w, r.h, fill);
    fill_rect(r.x, r.y, r.w, size, line);
    fill_rect(r.x, r.y + r.h - size, r.w, size, line);
    fill_rect(r.x, r.y, size, r.h, line);
    fill_rect(r.x + r.w - size, r.y, size, r.h, line);
}

/* An ellipse in the rectangle from corner to corner, by Alois Zingl's integer algorithm: each step
 * gives a point in each quarter, the dots of an outline or the ends of a filled row. */
static void paint_oval_pass(int x0, int y0, int x1, int y1, int size, int filled, uint32_t colour) {
    long a = abs(x1 - x0), b = abs(y1 - y0), b1 = b & 1;
    long dx = 4 * (1 - a) * b * b, dy = 4 * (b1 + 1) * a * a, error = dx + dy + b1 * a * a;
    if (x0 > x1) {
        x0 = x1;
        x1 += (int)a;
    }
    if (y0 > y1) y0 = y1;
    y0 += (int)((b + 1) / 2);
    y1 = y0 - (int)b1;
    a *= 8 * a;
    b1 = 8 * b * b;
    do {
        if (filled) {
            hline(x0, y0, x1 - x0 + 1, colour);
            hline(x0, y1, x1 - x0 + 1, colour);
        } else {
            paint_dot(x1, y0, size, colour);
            paint_dot(x0, y0, size, colour);
            paint_dot(x0, y1, size, colour);
            paint_dot(x1, y1, size, colour);
        }
        long twice = 2 * error;
        if (twice <= dy) {
            y0++;
            y1--;
            dy += a;
            error += dy;
        }
        if (twice >= dx || 2 * error > dy) {
            x0++;
            x1--;
            dx += b1;
            error += dx;
        }
    } while (x0 <= x1);
    /* a very flat ellipse stops early: its tips are finished here */
    while (y0 - y1 < b) {
        if (filled) {
            hline(x0 - 1, y0, x1 - x0 + 3, colour);
            hline(x0 - 1, y1, x1 - x0 + 3, colour);
        } else {
            paint_dot(x0 - 1, y0, size, colour);
            paint_dot(x1 + 1, y0, size, colour);
            paint_dot(x0 - 1, y1, size, colour);
            paint_dot(x1 + 1, y1, size, colour);
        }
        y0++;
        y1--;
    }
}

static void paint_oval(int x0, int y0, int x1, int y1, int size, int style, uint32_t line, uint32_t fill) {
    if (style == 2) {
        paint_oval_pass(x0, y0, x1, y1, size, 1, line);
        return;
    }
    if (style == 1) paint_oval_pass(x0, y0, x1, y1, size, 1, fill);
    paint_oval_pass(x0, y0, x1, y1, size, 0, line);
}

/* The shape being dragged, drawn wherever the drawing calls draw now: over the view while the
 * button is held, then on the picture. */
static void paint_draw_shape(const Paint *p) {
    int right = p->button == BUTTON_RIGHT, size = paint_size(p);
    uint32_t line = p->colours[right], fill = p->colours[!right];
    if (p->tool == PAINT_LINE) paint_line(p->x0, p->y0, p->x1, p->y1, size, 0, line);
    else if (p->tool == PAINT_RECT) paint_box(p->x0, p->y0, p->x1, p->y1, size, p->style, line, fill);
    else if (p->tool == PAINT_ELLIPSE) paint_oval(p->x0, p->y0, p->x1, p->y1, size, p->style, line, fill);
}

static Rect paint_shape_bounds(const Paint *p) {
    return paint_pad(paint_corners(p->x0, p->y0, p->x1, p->y1), paint_size(p) / 2 + 1);
}

/* With Shift, a line keeps to the nearest of the eight directions, and a rectangle or an ellipse
 * is as wide as it is high. */
static void paint_constrain(Paint *p) {
    int dx = p->x1 - p->x0, dy = p->y1 - p->y0, sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    int across = max_int(abs(dx), abs(dy));
    if (p->tool == PAINT_LINE && abs(dx) > 2 * abs(dy)) dy = 0;
    else if (p->tool == PAINT_LINE && abs(dy) > 2 * abs(dx)) dx = 0;
    else {
        dx = sx * across;
        dy = sy * across;
    }
    p->x1 = p->x0 + dx;
    p->y1 = p->y0 + dy;
}

/* Dots at random in a circle, a few more each time the airbrush's timer comes. */
static void paint_spray(int x, int y, int radius, uint32_t colour) {
    int count = radius * radius / 6 + 4;
    for (int i = 0; i < count; i++) {
        int dx = rand() % (2 * radius + 1) - radius, dy = rand() % (2 * radius + 1) - radius;
        if (dx * dx + dy * dy <= radius * radius) put_pixel(x + dx, y + dy, colour);
    }
}

/* A dashed frame just outside `r`, as a selection shows. */
static void paint_marquee(Rect r) {
    int x0 = r.x - 1, y0 = r.y - 1, x1 = r.x + r.w, y1 = r.y + r.h;
    for (int x = x0; x <= x1; x++) {
        uint32_t colour = (x - x0) >> 2 & 1 ? WHITE : BLACK;
        put_pixel(x, y0, colour);
        put_pixel(x, y1, colour);
    }
    for (int y = y0; y <= y1; y++) {
        uint32_t colour = (y - y0) >> 2 & 1 ? WHITE : BLACK;
        put_pixel(x0, y, colour);
        put_pixel(x1, y, colour);
    }
}

/* ------------------------------------------------------------------ drawing the window */

/* Makes the drawing calls draw on the picture itself, until gfx_restore with what this answers. */
static GfxContext paint_on_canvas(Paint *p) {
    GfxContext saved = gfx_save();
    gfx_target(p->canvas, p->cw, p->ch, p->cw);
    return saved;
}

static void paint_layout(Paint *p) {
    Rect inner = rect(PAINT_TOOLBOX_W + 2, 2, p->w - PAINT_TOOLBOX_W - 4, paint_view_bottom(p) - 4);
    int need_w = p->cw + 2 * PAINT_MARGIN, need_h = p->ch + 2 * PAINT_MARGIN;
    /* each scroll bar takes room from the other direction, so a second pass settles them */
    p->has_hbar = p->has_vbar = 0;
    for (int pass = 0; pass < 2; pass++) {
        p->has_hbar = need_w > inner.w - (p->has_vbar ? PAINT_BAR : 0);
        p->has_vbar = need_h > inner.h - (p->has_hbar ? PAINT_BAR : 0);
    }
    p->port = rect(inner.x, inner.y, max_int(1, inner.w - (p->has_vbar ? PAINT_BAR : 0)),
                   max_int(1, inner.h - (p->has_hbar ? PAINT_BAR : 0)));
    p->sx = clamp_int(p->sx, 0, paint_max_sx(p));
    p->sy = clamp_int(p->sy, 0, paint_max_sy(p));
}

/* Sets the scroll bars from the scroll position; they count in steps of PAINT_STEP pixels. */
static void paint_set_bars(Paint *p) {
    p->vbar.r = rect(p->port.x + p->port.w, p->port.y, PAINT_BAR, p->port.h);
    p->vbar.visible = max_int(1, p->port.h / PAINT_STEP);
    p->vbar.total = p->vbar.visible + (paint_max_sy(p) + PAINT_STEP - 1) / PAINT_STEP;
    p->vbar.top = (p->sy + PAINT_STEP - 1) / PAINT_STEP;
    p->hbar.r = paint_transpose(rect(p->port.x, p->port.y + p->port.h, p->port.w, PAINT_BAR));
    p->hbar.visible = max_int(1, p->port.w / PAINT_STEP);
    p->hbar.total = p->hbar.visible + (paint_max_sx(p) + PAINT_STEP - 1) / PAINT_STEP;
    p->hbar.top = (p->sx + PAINT_STEP - 1) / PAINT_STEP;
}

/* The horizontal scroll bar is a ScrollBar turned a quarter: its rectangle and the pointer have x
 * and y swapped, so that widgets.c's logic serves it unchanged. */
static int paint_hbar_message(ScrollBar *bar, const Message *m) {
    Message turned = *m;
    turned.x = m->y;
    turned.y = m->x;
    return scroll_bar_message(bar, &turned);
}

static void paint_draw_hbar(const ScrollBar *bar) {
    Rect r = paint_transpose(bar->r);
    uint32_t rows[2][1024];
    int w = min_int(r.w, 1024);
    for (int x = 0; x < w; x++) {
        rows[0][x] = x & 1 ? FACE : WHITE;
        rows[1][x] = x & 1 ? WHITE : FACE;
    }
    for (int y = 0; y < r.h; y++) blit(rows[y & 1], w, r.x, r.y + y, w, 1);
    draw_button(r.x, r.y, PAINT_BAR, r.h, "", bar->held == -1);
    draw_button(r.x + r.w - PAINT_BAR, r.y, PAINT_BAR, r.h, "", bar->held == 1);
    for (int i = 0; i < 4; i++) {
        vline(r.x + 6 + i, r.y + 7 - i, 1 + 2 * i, BLACK);
        vline(r.x + r.w - PAINT_BAR + 9 - i, r.y + 7 - i, 1 + 2 * i, BLACK);
    }
    Rect thumb = paint_transpose(scroll_thumb(bar));
    if (thumb.w) draw_button(thumb.x, thumb.y, thumb.w, thumb.h, "", 0);
}

static void paint_draw_bars(Paint *p) {
    paint_set_bars(p);
    os_window_draw(p->win);
    if (p->has_vbar) {
        draw_scroll_bar(&p->vbar);
        os_window_update(p->win, p->vbar.r.x, p->vbar.r.y, p->vbar.r.w, p->vbar.r.h);
    }
    if (p->has_hbar) {
        Rect r = paint_transpose(p->hbar.r);
        paint_draw_hbar(&p->hbar);
        os_window_update(p->win, r.x, r.y, r.w, r.h);
    }
    if (p->has_vbar && p->has_hbar) {
        fill_rect(p->port.x + p->port.w, p->port.y + p->port.h, PAINT_BAR, PAINT_BAR, FACE);
        os_window_update(p->win, p->port.x + p->port.w, p->port.y + p->port.h, PAINT_BAR, PAINT_BAR);
    }
}

/* What lies over the picture, drawn with the picture's pixel 0, 0 at the origin. */
static void paint_draw_overlays(Paint *p, Rect picture) {
    Rect clip = gfx_get_clip();
    gfx_clip(rect_intersect(clip, picture));
    if (p->selected && p->floating) blit(p->floating, p->sel.w, p->sel.x, p->sel.y, p->sel.w, p->sel.h);
    if (p->hold == PAINT_HOLD_SHAPE) paint_draw_shape(p);
    gfx_clip(clip);
    if (p->selected) paint_marquee(p->sel);
    if (p->hold == PAINT_HOLD_SELECT) paint_marquee(paint_band(p));
}

/* Draws the rectangle `area` of the client area's view and shows it: the grey around the picture,
 * the picture, and what lies over it. */
static void paint_draw_view(Paint *p, Rect area) {
    area = rect_intersect(area, p->port);
    if (rect_empty(area)) return;
    os_window_draw(p->win);
    int ox = paint_ox(p), oy = paint_oy(p);
    Rect picture = rect(ox, oy, p->cw, p->ch), inside = rect_intersect(area, picture);
    gfx_clip(area);
    /* the grey in strips around the picture, so that no pixel is drawn twice */
    if (rect_empty(inside)) {
        fill_rect(area.x, area.y, area.w, area.h, PAINT_BACKGROUND);
    } else {
        fill_rect(area.x, area.y, area.w, inside.y - area.y, PAINT_BACKGROUND);
        fill_rect(area.x, inside.y + inside.h, area.w, area.y + area.h - inside.y - inside.h, PAINT_BACKGROUND);
        fill_rect(area.x, inside.y, inside.x - area.x, inside.h, PAINT_BACKGROUND);
        fill_rect(inside.x + inside.w, inside.y, area.x + area.w - inside.x - inside.w, inside.h, PAINT_BACKGROUND);
    }
    gfx_origin(ox, oy);
    blit(p->canvas, p->cw, 0, 0, p->cw, p->ch);
    paint_draw_overlays(p, picture);
    gfx_origin(0, 0);
    gfx_clip(rect(0, 0, p->w, p->h));
    os_window_update(p->win, area.x, area.y, area.w, area.h);
}

/* The same for a rectangle of the picture, in its own pixels. */
static void paint_show(Paint *p, Rect r) { paint_draw_view(p, rect(r.x + paint_ox(p), r.y + paint_oy(p), r.w, r.h)); }

static void paint_draw_tool_picture(int tool, int x, int y) {
    switch (tool) {
    case PAINT_SELECT:
        for (int i = 0; i <= 12; i++) {
            if (i >> 1 & 1) continue;
            put_pixel(x + 2 + i, y + 3, BLACK);
            put_pixel(x + 2 + i, y + 13, BLACK);
            if (i <= 10) {
                put_pixel(x + 2, y + 3 + i, BLACK);
                put_pixel(x + 14, y + 3 + i, BLACK);
            }
        }
        break;
    case PAINT_PICK:
        thick_line(x + 3, y + 13, x + 9, y + 7, 2, 0x4060c0);
        thick_line(x + 9, y + 7, x + 11, y + 5, 3, BLACK);
        fill_circle(x + 12, y + 4, 2, BLACK);
        break;
    case PAINT_PENCIL:
        thick_line(x + 5, y + 12, x + 12, y + 5, 3, 0xe0b000);
        draw_line(x + 3, y + 14, x + 4, y + 13, BLACK);
        fill_rect(x + 12, y + 3, 3, 3, 0xff8080);
        break;
    case PAINT_BRUSH:
        thick_line(x + 8, y + 9, x + 13, y + 3, 2, 0xa05a2c);
        fill_circle(x + 6, y + 11, 3, BLACK);
        break;
    case PAINT_ERASER:
        fill_rect(x + 3, y + 6, 11, 7, 0xff9fbf);
        frame_rect(x + 3, y + 6, 11, 7, BLACK);
        vline(x + 7, y + 6, 7, BLACK);
        break;
    case PAINT_FILL:
        frame_rect(x + 3, y + 5, 9, 9, BLACK);
        fill_rect(x + 4, y + 9, 7, 4, 0x0000ff);
        fill_rect(x + 12, y + 7, 2, 6, 0x0000ff);
        break;
    case PAINT_LINE: draw_line(x + 3, y + 13, x + 13, y + 3, BLACK); break;
    case PAINT_SPRAY: {
        static const signed char dots[][2] = { { 8, 3 }, { 5, 6 }, { 11, 6 }, { 8, 8 }, { 4, 10 },
                                               { 12, 10 }, { 7, 12 }, { 10, 13 }, { 6, 4 } };
        for (int i = 0; i < 9; i++) put_pixel(x + dots[i][0], y + dots[i][1], BLACK);
        break;
    }
    case PAINT_RECT: frame_rect(x + 2, y + 4, 13, 9, BLACK); break;
    case PAINT_ELLIPSE: paint_oval_pass(x + 2, y + 4, x + 14, y + 12, 1, 0, BLACK); break;
    }
}

static void paint_draw_toolbox(Paint *p) {
    os_window_draw(p->win);
    int bottom = paint_view_bottom(p);
    uint32_t selection = os_theme_colour(THEME_SELECTION);
    fill_rect(0, 0, PAINT_TOOLBOX_W, bottom, FACE);
    for (int i = 0; i < PAINT_TOOL_COUNT; i++) {
        Rect r = paint_tool_rect(i);
        int chosen = i == p->tool;
        if (chosen) {
            fill_rect(r.x, r.y, r.w, r.h, WHITE);
            bevel_sunken(r.x, r.y, r.w, r.h);
        } else {
            draw_button(r.x, r.y, r.w, r.h, "", 0);
        }
        paint_draw_tool_picture(i, r.x + 3 + chosen, r.y + 3 + chosen);
    }
    Rect box = paint_options_rect();
    bevel_thin_sunken(box.x, box.y, box.w, box.h);
    const int *sizes = PAINT_SIZES[p->tool];
    for (int i = 0; i < 3 && sizes[0]; i++) {
        Rect r = paint_size_rect(i);
        int chosen = p->size[p->tool] == i, cx = r.x + r.w / 2, cy = r.y + r.h / 2, size = sizes[i];
        uint32_t ink = chosen ? WHITE : BLACK;
        if (chosen) fill_rect(r.x, r.y, r.w, r.h, selection);
        if (p->tool == PAINT_BRUSH) {
            fill_circle(cx, cy, size / 2, ink);
        } else if (p->tool == PAINT_ERASER) {
            int across = min_int(size, 11);
            fill_rect(cx - across / 2, cy - across / 2, across, across, ink);
        } else if (p->tool == PAINT_SPRAY) {
            int radius = 2 + i * 2;
            for (int dy = -radius; dy <= radius; dy++)
                for (int dx = -radius; dx <= radius; dx++)
                    if (dx * dx + dy * dy <= radius * radius && ((dx + 2 * dy) & 3) == 0)
                        put_pixel(cx + dx, cy + dy, ink);
        } else {
            fill_rect(r.x + 5, cy - size / 2, r.w - 10, size, ink);
        }
    }
    for (int i = 0; i < 3 && (p->tool == PAINT_RECT || p->tool == PAINT_ELLIPSE); i++) {
        Rect r = paint_style_rect(i), shape = rect(r.x + 7, r.y + 2, r.w - 14, r.h - 4);
        int chosen = p->style == i;
        if (chosen) fill_rect(r.x, r.y, r.w, r.h, selection);
        if (i > 0) fill_rect(shape.x, shape.y, shape.w, shape.h, i == 1 ? SHADOW : chosen ? WHITE : BLACK);
        if (i < 2) frame_rect(shape.x, shape.y, shape.w, shape.h, chosen ? WHITE : BLACK);
    }
    os_window_update(p->win, 0, 0, PAINT_TOOLBOX_W, bottom);
}

static void paint_draw_palette(Paint *p) {
    os_window_draw(p->win);
    int top = paint_view_bottom(p);
    fill_rect(0, top, p->w, PAINT_PALETTE_H, FACE);
    /* the two colours, the first over the second */
    Rect box = paint_colours_rect(p);
    bevel_sunken(box.x, box.y, box.w, box.h);
    fill_rect(box.x + 12, box.y + 12, 14, 14, p->colours[1]);
    frame_rect(box.x + 12, box.y + 12, 14, 14, BLACK);
    fill_rect(box.x + 4, box.y + 4, 14, 14, p->colours[0]);
    frame_rect(box.x + 4, box.y + 4, 14, 14, BLACK);
    for (int i = 0; i < PAINT_COLOURS; i++) {
        Rect swatch = paint_swatch_rect(p, i);
        fill_rect(swatch.x, swatch.y, swatch.w, swatch.h, p->palette[i]);
        bevel_thin_sunken(swatch.x, swatch.y, swatch.w, swatch.h);
    }
    os_window_update(p->win, 0, top, p->w, PAINT_PALETTE_H);
}

static void paint_draw_status(Paint *p) {
    os_window_draw(p->win);
    int y = p->h - PAINT_STATUS_H, size_x = p->w - 94, where_x = size_x - 88;
    fill_rect(0, y, p->w, PAINT_STATUS_H, FACE);
    bevel_thin_sunken(2, y + 2, where_x - 4, PAINT_STATUS_H - 4);
    bevel_thin_sunken(where_x, y + 2, 84, PAINT_STATUS_H - 4);
    bevel_thin_sunken(size_x, y + 2, 92, PAINT_STATUS_H - 4);
    GfxContext saved = gfx_save();
    gfx_clip(rect(4, y, where_x - 8, PAINT_STATUS_H));
    draw_text(8, y + 4, p->message, BLACK);
    gfx_restore(saved);
    /* where the pointer is, or how large the rectangle being dragged out is */
    char where[32] = "";
    if (p->hold == PAINT_HOLD_SELECT || p->hold == PAINT_HOLD_SHAPE) {
        Rect r = paint_band(p);
        snprintf(where, sizeof where, "%d%c%d", r.w, CH_TIMES, r.h);
    } else if (p->hold == PAINT_HOLD_MOVE) {
        snprintf(where, sizeof where, "%d, %d", p->sel.x, p->sel.y);
    } else if (p->pointer_x >= 0) {
        snprintf(where, sizeof where, "%d, %d", p->pointer_x, p->pointer_y);
    }
    draw_text(where_x + 6, y + 4, where, BLACK);
    char size[32];
    snprintf(size, sizeof size, "%d %c %d", p->cw, CH_TIMES, p->ch);
    draw_text(size_x + 6, y + 4, size, BLACK);
    os_window_update(p->win, 0, y, p->w, PAINT_STATUS_H);
}

static void paint_draw_all(Paint *p) {
    paint_layout(p);
    os_window_draw(p->win);
    bevel_sunken(PAINT_TOOLBOX_W, 0, p->w - PAINT_TOOLBOX_W, paint_view_bottom(p));
    paint_draw_bars(p);
    paint_draw_view(p, p->port);
    paint_draw_toolbox(p);
    paint_draw_palette(p);
    paint_draw_status(p);
    os_window_update_all(p->win);
}

static void paint_title(Paint *p) {
    char title[96];
    snprintf(title, sizeof title, "%s%s - Paint", p->modified ? "*" : "",
             p->path[0] ? os_base_name(p->path) : "Untitled");
    os_window_set_title(p->win, title);
}

static void paint_say(Paint *p, const char *message) {
    copy_text(p->message, sizeof p->message, message);
    paint_draw_status(p);
}

/* ------------------------------------------------------------------ undo */

static void paint_copy_pixels(uint32_t *to, const uint32_t *from, int count) { copy_rect32(to, 0, from, 0, count, 1); }

static void paint_forget(PaintPicture *list, int *count) {
    while (*count) os_free(list[--*count].pixels);
}

static void paint_push(PaintPicture *list, int *count, PaintPicture picture) {
    if (*count == PAINT_UNDO_LEVELS) {
        os_free(list[0].pixels);
        memmove(list, list + 1, (PAINT_UNDO_LEVELS - 1) * sizeof *list);
        (*count)--;
    }
    list[(*count)++] = picture;
}

/* Keeps a copy of the picture as it is, for Undo, before a change. */
static void paint_checkpoint(Paint *p) {
    PaintPicture copy = { os_alloc((size_t)p->cw * p->ch * 4), p->cw, p->ch };
    if (copy.pixels) {
        paint_copy_pixels(copy.pixels, p->canvas, p->cw * p->ch);
        paint_push(p->undo, &p->undo_count, copy);
    }
    paint_forget(p->redo, &p->redo_count);
    if (!p->modified) {
        p->modified = 1;
        paint_title(p);
    }
}

/* ------------------------------------------------------------------ the selection */

/* Takes the selection's pixels off the picture, to move them; the hole left is the second colour,
 * unless `keep` leaves the original where it was. */
static int paint_lift(Paint *p, int keep) {
    if (p->floating) return 1;
    Rect s = p->sel;
    uint32_t *pixels = os_alloc((size_t)s.w * s.h * 4);
    if (!pixels) return 0;
    paint_checkpoint(p);
    uint32_t *corner = p->canvas + s.y * p->cw + s.x;
    copy_rect32(pixels, s.w * 4, corner, p->cw * 4, s.w, s.h);
    if (!keep) fill_rect32(corner, p->cw * 4, s.w, s.h, p->colours[1]);
    p->floating = pixels;
    return 1;
}

/* Puts a floating selection down where it is, and selects nothing. */
static void paint_drop(Paint *p) {
    if (!p->selected) return;
    Rect area = paint_pad(p->sel, 1);
    if (p->floating) {
        GfxContext saved = paint_on_canvas(p);
        blit(p->floating, p->sel.w, p->sel.x, p->sel.y, p->sel.w, p->sel.h);
        gfx_restore(saved);
        os_free(p->floating);
        p->floating = NULL;
    }
    p->selected = 0;
    paint_show(p, area);
}

static void paint_move_selection(Paint *p, int x, int y) {
    if (!p->selected || (x == p->sel.x && y == p->sel.y)) return;
    if (!p->floating && !paint_lift(p, p->copying)) return;
    Rect before = paint_pad(p->sel, 1);
    p->sel.x = x;
    p->sel.y = y;
    paint_show(p, rect_union(before, paint_pad(p->sel, 1)));
}

static int paint_copy_selection(Paint *p) {
    if (!p->selected) {
        os_beep();
        return 0;
    }
    Rect s = p->sel;
    if (p->floating) {
        os_clipboard_set_picture(p->floating, s.w, s.h);
    } else {
        uint32_t *pixels = os_alloc((size_t)s.w * s.h * 4);
        if (!pixels) return 0;
        copy_rect32(pixels, s.w * 4, p->canvas + s.y * p->cw + s.x, p->cw * 4, s.w, s.h);
        os_clipboard_set_picture(pixels, s.w, s.h);
        os_free(pixels);
    }
    char message[64];
    snprintf(message, sizeof message, "Copied %d %c %d pixels", s.w, CH_TIMES, s.h);
    paint_say(p, message);
    return 1;
}

/* Clears the selection to the second colour, as the Del key does. */
static void paint_clear_selection(Paint *p) {
    if (!p->selected) {
        os_beep();
        return;
    }
    Rect s = p->sel;
    if (p->floating) {
        os_free(p->floating);
        p->floating = NULL;
    } else {
        paint_checkpoint(p);
        fill_rect32(p->canvas + s.y * p->cw + s.x, p->cw * 4, s.w, s.h, p->colours[1]);
    }
    p->selected = 0;
    paint_show(p, paint_pad(s, 1));
}

/* Gives the picture a new size, keeping its top left; what is new is the second colour. */
static int paint_resize_canvas(Paint *p, int w, int h) {
    uint32_t *pixels = os_alloc((size_t)w * h * 4);
    if (!pixels) return 0;
    fill_rect32(pixels, w * 4, w, h, p->colours[1]);
    copy_rect32(pixels, w * 4, p->canvas, p->cw * 4, min_int(w, p->cw), min_int(h, p->ch));
    os_free(p->canvas);
    p->canvas = pixels;
    p->cw = w;
    p->ch = h;
    return 1;
}

static void paint_choose_tool(Paint *p, int tool) {
    if (tool != PAINT_SELECT) paint_drop(p);
    if (tool == PAINT_PICK && p->tool != PAINT_PICK) p->last_tool = p->tool;
    p->tool = tool;
    snprintf(p->message, sizeof p->message, "%s: %s", PAINT_TOOL_NAMES[tool], PAINT_TOOL_HINTS[tool]);
    paint_draw_toolbox(p);
    paint_draw_status(p);
}

static void paint_paste(Paint *p) {
    int w, h, grown = 0;
    uint32_t *pixels = os_clipboard_picture(&w, &h);
    if (!pixels) {
        os_beep();
        paint_say(p, os_clipboard_kind() == CLIP_TEXT ? "The clipboard holds text, not a picture"
                                                      : "The clipboard is empty");
        return;
    }
    paint_drop(p);
    if ((w > p->cw && p->cw < PAINT_MAX_W) || (h > p->ch && p->ch < PAINT_MAX_H)) {
        int answer = os_message_box(p->win, "Paint", "The picture on the clipboard is larger than this one. Should this "
                                    "picture grow to fit it?", MB_YES_NO_CANCEL, MB_QUESTION);
        if (answer == ID_CANCEL) {
            os_free(pixels);
            return;
        }
        if (answer == ID_YES) {
            paint_checkpoint(p);
            grown = paint_resize_canvas(p, min_int(max_int(w, p->cw), PAINT_MAX_W),
                                        min_int(max_int(h, p->ch), PAINT_MAX_H));
        }
    }
    if (!grown) paint_checkpoint(p);
    p->floating = pixels;
    p->selected = 1;
    p->sel = rect(max_int(0, p->sx - PAINT_MARGIN), max_int(0, p->sy - PAINT_MARGIN), w, h);
    p->tool = PAINT_SELECT;
    snprintf(p->message, sizeof p->message, "Pasted %d %c %d: drag it into place", w, CH_TIMES, h);
    paint_draw_all(p);
}

/* ------------------------------------------------------------------ whole pictures */

static void paint_flip_h(uint32_t *pixels, int w, int h) {
    for (int y = 0; y < h; y++) {
        os_yield_point();
        uint32_t *row = pixels + (long)y * w;
        for (int a = 0, b = w - 1; a < b; a++, b--) {
            uint32_t pixel = row[a];
            row[a] = row[b];
            row[b] = pixel;
        }
    }
}

static void paint_flip_v(uint32_t *pixels, int w, int h) {
    uint32_t *spare = os_alloc((size_t)w * 4);
    if (!spare) return;
    for (int a = 0, b = h - 1; a < b; a++, b--) {
        uint32_t *top = pixels + (long)a * w, *bottom = pixels + (long)b * w;
        paint_copy_pixels(spare, top, w);
        paint_copy_pixels(top, bottom, w);
        paint_copy_pixels(bottom, spare, w);
    }
    os_free(spare);
}

static void paint_invert(uint32_t *pixels, long count) {
    for (long block = 0; block < count; block += 4096) {
        os_yield_point();
        for (long i = block; i < count && i < block + 4096; i++) pixels[i] ^= 0xffffff;
    }
}

/* Flips or inverts the selection, or the whole picture when nothing is selected. */
static void paint_transform(Paint *p, int id) {
    uint32_t *pixels;
    int w, h;
    if (p->selected) {
        if (!paint_lift(p, 0)) return;
        pixels = p->floating;
        w = p->sel.w;
        h = p->sel.h;
    } else {
        paint_checkpoint(p);
        pixels = p->canvas;
        w = p->cw;
        h = p->ch;
    }
    if (id == PAINT_FLIP_H) paint_flip_h(pixels, w, h);
    else if (id == PAINT_FLIP_V) paint_flip_v(pixels, w, h);
    else paint_invert(pixels, (long)w * h);
    paint_show(p, p->selected ? paint_pad(p->sel, 1) : rect(0, 0, p->cw, p->ch));
}

/* Replaces the picture, with nothing selected and nothing to undo. */
static void paint_set_picture(Paint *p, uint32_t *pixels, int w, int h) {
    os_free(p->floating);
    p->floating = NULL;
    p->selected = 0;
    p->hold = PAINT_HOLD_NONE;
    paint_forget(p->undo, &p->undo_count);
    paint_forget(p->redo, &p->redo_count);
    os_free(p->canvas);
    p->canvas = pixels;
    p->cw = w;
    p->ch = h;
    p->sx = p->sy = 0;
}

static int paint_new_picture(Paint *p) {
    uint32_t *pixels = os_alloc((size_t)PAINT_NEW_W * PAINT_NEW_H * 4);
    if (!pixels) return 0;
    fill_rect32(pixels, PAINT_NEW_W * 4, PAINT_NEW_W, PAINT_NEW_H, WHITE);
    paint_set_picture(p, pixels, PAINT_NEW_W, PAINT_NEW_H);
    p->path[0] = 0;
    p->modified = 0;
    return 1;
}

static void paint_undo(Paint *p, int redo) {
    PaintPicture *from = redo ? p->redo : p->undo, *to = redo ? p->undo : p->redo;
    int *from_count = redo ? &p->redo_count : &p->undo_count, *to_count = redo ? &p->undo_count : &p->redo_count;
    if (!*from_count) {
        os_beep();
        return;
    }
    /* a floating selection goes down first, so that Redo brings back the move it made */
    paint_drop(p);
    paint_push(to, to_count, (PaintPicture){ p->canvas, p->cw, p->ch });
    PaintPicture back = from[--*from_count];
    p->canvas = back.pixels;
    p->cw = back.w;
    p->ch = back.h;
    p->modified = 1;
    paint_say(p, redo ? "Redone" : "Undone");
    paint_title(p);
    paint_draw_all(p);
}

/* ------------------------------------------------------------------ files */

/* Reads a .bmp File; answers its pixels, or NULL with the reason in `problem`. */
static uint32_t *paint_read_bmp(const char *path, int *w, int *h, char *problem, int size) {
    long length;
    char *data = os_read_file(path, &length);
    if (!data) {
        snprintf(problem, size, "%.60s could not be read.", os_base_name(path));
        return NULL;
    }
    uint32_t *pixels = os_bmp_decode(data, length, w, h);
    os_free(data);
    if (!pixels) {
        snprintf(problem, size, "%.60s is not a picture Paint can open: it opens uncompressed 24- and 32-bit BMP files.",
                 os_base_name(path));
        return NULL;
    }
    if (*w > PAINT_MAX_W || *h > PAINT_MAX_H) {
        os_free(pixels);
        snprintf(problem, size, "%.60s is %d %c %d pixels: Paint opens pictures up to %d %c %d.", os_base_name(path),
                 *w, CH_TIMES, *h, PAINT_MAX_W, CH_TIMES, PAINT_MAX_H);
        return NULL;
    }
    return pixels;
}

static void paint_opened(Paint *p, const char *path) {
    copy_text(p->path, sizeof p->path, path);
    p->modified = 0;
    snprintf(p->message, sizeof p->message, "Opened %.40s", os_base_name(path));
}

/* Saves under the picture's name, or asks for one; answers 1 when saved. */
static int paint_save(Paint *p, int ask) {
    char path[128];
    const char *untitled = os_stat("pictures", NULL) == 2 ? "pictures/Untitled.bmp" : "Untitled.bmp";
    copy_text(path, sizeof path, p->path[0] ? p->path : untitled);
    if (ask || !p->path[0]) {
        if (!os_file_dialog(p->win, 1, "Save As", ".bmp", path, sizeof path)) return 0;
        if (os_app_for_file(path) != APP_VIEWER && strlen(path) + 4 < sizeof path) strcat(path, ".bmp");
    }
    /* a floating selection is saved where it floats, without putting it down */
    const uint32_t *pixels = p->canvas;
    uint32_t *composed = NULL;
    if (p->floating && (composed = os_alloc((size_t)p->cw * p->ch * 4))) {
        paint_copy_pixels(composed, p->canvas, p->cw * p->ch);
        GfxContext saved = gfx_save();
        gfx_target(composed, p->cw, p->ch, p->cw);
        blit(p->floating, p->sel.w, p->sel.x, p->sel.y, p->sel.w, p->sel.h);
        gfx_restore(saved);
        pixels = composed;
    }
    long size;
    char *file = os_bmp_encode(pixels, p->cw, p->ch, &size);
    int ok = file && os_write_file(path, file, size) == 0;
    os_free(file);
    os_free(composed);
    if (!ok) {
        os_message_box(p->win, "Paint", "The picture could not be saved.", MB_OK, MB_WARNING);
        return 0;
    }
    copy_text(p->path, sizeof p->path, path);
    p->modified = 0;
    snprintf(p->message, sizeof p->message, "Saved %.40s, %ld bytes", os_base_name(path), size);
    paint_title(p);
    paint_draw_status(p);
    return 1;
}

/* Before the picture goes: answers 1 to go on, after saving if asked to. */
static int paint_confirm(Paint *p) {
    if (!p->modified) return 1;
    char question[160];
    snprintf(question, sizeof question, "Do you want to save the changes to %.60s?",
             p->path[0] ? os_base_name(p->path) : "Untitled");
    int answer = os_message_box(p->win, "Paint", question, MB_YES_NO_CANCEL, MB_QUESTION);
    if (answer == ID_YES) return paint_save(p, 0);
    return answer == ID_NO;
}

static void paint_open_path(Paint *p, const char *path) {
    if (!paint_confirm(p)) return;
    char problem[160];
    int w, h;
    uint32_t *pixels = paint_read_bmp(path, &w, &h, problem, sizeof problem);
    if (!pixels) {
        os_message_box(p->win, "Paint", problem, MB_OK, MB_WARNING);
        return;
    }
    paint_set_picture(p, pixels, w, h);
    paint_opened(p, path);
    paint_title(p);
    paint_draw_all(p);
}

static void paint_canvas_size(Paint *p) {
    char text[32];
    snprintf(text, sizeof text, "%d x %d", p->cw, p->ch);
    if (!os_input_box(p->win, "Canvas Size", "The picture's width and height in pixels, up to 1024 x 768:", text,
                      sizeof text))
        return;
    int w, h;
    if (sscanf(text, "%d %*[xX*,] %d", &w, &h) != 2 || w < 1 || h < 1 || w > PAINT_MAX_W || h > PAINT_MAX_H) {
        os_message_box(p->win, "Paint", "Type a width and a height, such as 640 x 480, up to 1024 x 768.", MB_OK,
                       MB_WARNING);
        return;
    }
    if (w == p->cw && h == p->ch) return;
    paint_drop(p);
    paint_checkpoint(p);
    paint_resize_canvas(p, w, h);
    paint_draw_all(p);
}

static void paint_edit_colour(Paint *p, int index) {
    char text[16];
    snprintf(text, sizeof text, "%06X", (unsigned)p->palette[index]);
    if (!os_input_box(p->win, "Edit Colour", "The colour's red, green and blue in hexadecimal, such as FF8000:", text,
                      sizeof text))
        return;
    const char *digits = text[0] == '#' ? text + 1 : text;
    char *end;
    long value = strtol(digits, &end, 16);
    if (end == digits || *end || value < 0 || value > 0xffffff) {
        os_message_box(p->win, "Paint", "A colour is six hexadecimal digits, such as FF8000 for orange.", MB_OK,
                       MB_WARNING);
        return;
    }
    p->palette[index] = (uint32_t)value;
    p->colours[0] = (uint32_t)value;
    paint_draw_palette(p);
}

/* ------------------------------------------------------------------ commands and keys */

static void paint_command(Paint *p, int id) {
    switch (id) {
    case PAINT_NEW:
        if (!paint_confirm(p) || !paint_new_picture(p)) return;
        paint_say(p, "New picture");
        break;
    case PAINT_OPEN: {
        char path[128];
        copy_text(path, sizeof path, p->path[0] ? p->path : "pictures");
        if (os_file_dialog(p->win, 0, "Open", ".bmp", path, sizeof path)) paint_open_path(p, path);
        return;
    }
    case PAINT_SAVE: paint_save(p, 0); return;
    case PAINT_SAVE_AS: paint_save(p, 1); return;
    case PAINT_EXIT:
        if (paint_confirm(p)) os_exit(0);
        return;
    case PAINT_UNDO: paint_undo(p, 0); return;
    case PAINT_REDO: paint_undo(p, 1); return;
    case PAINT_CUT:
        if (paint_copy_selection(p)) paint_clear_selection(p);
        return;
    case PAINT_COPY: paint_copy_selection(p); return;
    case PAINT_PASTE: paint_paste(p); return;
    case PAINT_CLEAR_SELECTION: paint_clear_selection(p); return;
    case PAINT_SELECT_ALL:
        paint_drop(p);
        p->selected = 1;
        p->sel = rect(0, 0, p->cw, p->ch);
        paint_choose_tool(p, PAINT_SELECT);
        paint_show(p, paint_pad(p->sel, 1));
        return;
    case PAINT_FLIP_H: case PAINT_FLIP_V: case PAINT_INVERT: paint_transform(p, id); return;
    case PAINT_CANVAS_SIZE: paint_canvas_size(p); return;
    case PAINT_CLEAR_IMAGE:
        paint_drop(p);
        paint_checkpoint(p);
        fill_rect32(p->canvas, p->cw * 4, p->cw, p->ch, p->colours[1]);
        break;
    case PAINT_ABOUT:
        os_message_box(p->win, "About Paint",
                       "Paint for asmOS.\nThe left button paints with the first colour, the right button with the "
                       "second. Double-click a colour of the palette to change it.",
                       MB_OK, MB_INFO);
        return;
    }
    paint_title(p);
    paint_draw_all(p);
}

static void paint_key(Paint *p, const Message *m) {
    /* while a button is held, only Escape counts: it forgets the shape or the selection dragged out */
    if (p->hold && m->key != KEY_ESC) return;
    if (m->mods & MOD_CTRL) {
        int id = 0;
        switch (m->key) {
        case KEY_N: id = m->mods & MOD_SHIFT ? PAINT_CLEAR_IMAGE : PAINT_NEW; break;
        case KEY_O: id = PAINT_OPEN; break;
        case KEY_S: id = PAINT_SAVE; break;
        case KEY_Z: id = PAINT_UNDO; break;
        case KEY_Y: id = PAINT_REDO; break;
        case KEY_X: id = PAINT_CUT; break;
        case KEY_C: id = PAINT_COPY; break;
        case KEY_V: id = PAINT_PASTE; break;
        case KEY_A: id = PAINT_SELECT_ALL; break;
        case KEY_I: id = PAINT_INVERT; break;
        }
        if (id) paint_command(p, id);
        return;
    }
    switch (m->key) {
    case KEY_DELETE: paint_command(p, PAINT_CLEAR_SELECTION); break;
    case KEY_ESC:
        if (p->hold == PAINT_HOLD_SHAPE || p->hold == PAINT_HOLD_SELECT) {
            Rect area = p->hold == PAINT_HOLD_SHAPE ? paint_shape_bounds(p) : paint_pad(paint_band(p), 1);
            p->hold = PAINT_HOLD_NONE;
            paint_show(p, area);
            paint_draw_status(p);
        } else if (!p->hold) {
            paint_drop(p);
        }
        break;
    case KEY_LEFT: case KEY_RIGHT: case KEY_UP: case KEY_DOWN:
        /* the arrows nudge the selection a pixel */
        if (p->selected) {
            p->copying = 0;
            paint_move_selection(p, p->sel.x + (m->key == KEY_RIGHT) - (m->key == KEY_LEFT),
                                 p->sel.y + (m->key == KEY_DOWN) - (m->key == KEY_UP));
        }
        break;
    }
}

/* ------------------------------------------------------------------ the pointer */

/* A stroke of the pencil, the brush, the eraser or the airbrush from the last point to x, y. */
static void paint_stroke(Paint *p, int x, int y) {
    int size = paint_size(p);
    uint32_t colour = p->colours[p->button == BUTTON_RIGHT];
    GfxContext saved = paint_on_canvas(p);
    if (p->tool == PAINT_PENCIL) draw_line(p->x0, p->y0, x, y, colour);
    else if (p->tool == PAINT_BRUSH) paint_line(p->x0, p->y0, x, y, size, 1, colour);
    else if (p->tool == PAINT_ERASER) paint_line(p->x0, p->y0, x, y, size, 0, p->colours[1]);
    else if (p->tool == PAINT_SPRAY) paint_spray(x, y, size / 2, colour);
    gfx_restore(saved);
    Rect area = paint_pad(paint_corners(p->x0, p->y0, x, y), size / 2 + 1);
    p->x0 = x;
    p->y0 = y;
    paint_show(p, area);
}

/* Fills the area of one colour around x, y a row at a time: each row's run is found and filled,
 * and the runs touching it in the rows above and below wait on a stack. */
static void paint_flood(Paint *p, int x, int y, uint32_t colour) {
    int w = p->cw, h = p->ch;
    uint32_t *pixels = p->canvas, old = pixels[y * w + x];
    if (old == colour) return;
    int capacity = w * h / 2 + 64, depth = 0;
    int *stack = os_alloc((size_t)capacity * sizeof *stack);
    if (!stack) return;
    stack[depth++] = y * w + x;
    while (depth) {
        int at = stack[--depth], sy = at / w, sx = at % w;
        uint32_t *row = pixels + (long)sy * w;
        if (row[sx] != old) continue;
        int left = sx, right = sx;
        while (left > 0 && row[left - 1] == old) left--;
        while (right < w - 1 && row[right + 1] == old) right++;
        fill_rect32(row + left, 0, right - left + 1, 1, colour);
        for (int dy = -1; dy <= 1; dy += 2) {
            int ny = sy + dy;
            if (ny < 0 || ny >= h) continue;
            const uint32_t *next = pixels + (long)ny * w;
            for (int i = left; i <= right; i++)
                if (next[i] == old && (i == left || next[i - 1] != old) && depth < capacity)
                    stack[depth++] = ny * w + i;
        }
        os_yield_point();       /* a run at a time: a large area takes a while */
    }
    os_free(stack);
}

static void paint_toolbox_click(Paint *p, const Message *m) {
    for (int i = 0; i < PAINT_TOOL_COUNT; i++)
        if (rect_contains(paint_tool_rect(i), m->x, m->y)) {
            paint_choose_tool(p, i);
            return;
        }
    for (int i = 0; i < 3 && PAINT_SIZES[p->tool][0]; i++)
        if (rect_contains(paint_size_rect(i), m->x, m->y)) {
            p->size[p->tool] = i;
            paint_draw_toolbox(p);
            return;
        }
    for (int i = 0; i < 3 && (p->tool == PAINT_RECT || p->tool == PAINT_ELLIPSE); i++)
        if (rect_contains(paint_style_rect(i), m->x, m->y)) {
            p->style = i;
            paint_draw_toolbox(p);
            return;
        }
}

static void paint_palette_click(Paint *p, const Message *m) {
    if (rect_contains(paint_colours_rect(p), m->x, m->y) && m->button == BUTTON_LEFT) {
        /* a click on the two colours swaps them */
        uint32_t first = p->colours[0];
        p->colours[0] = p->colours[1];
        p->colours[1] = first;
        paint_draw_palette(p);
        return;
    }
    for (int i = 0; i < PAINT_COLOURS; i++) {
        if (!rect_contains(paint_swatch_rect(p, i), m->x, m->y)) continue;
        if (m->button == BUTTON_LEFT && m->clicks == 2) {
            paint_edit_colour(p, i);
            return;
        }
        if (m->button == BUTTON_LEFT || m->button == BUTTON_RIGHT) {
            p->colours[m->button == BUTTON_RIGHT] = p->palette[i];
            paint_draw_palette(p);
        }
        return;
    }
}

static void paint_press(Paint *p, const Message *m, int x, int y) {
    if (p->hold) return;
    if (m->y >= paint_view_bottom(p)) {
        paint_palette_click(p, m);
        return;
    }
    if (m->x < PAINT_TOOLBOX_W) {
        paint_toolbox_click(p, m);
        return;
    }
    if (!rect_contains(p->port, m->x, m->y) || (m->button != BUTTON_LEFT && m->button != BUTTON_RIGHT)) return;
    int inside = x >= 0 && y >= 0 && x < p->cw && y < p->ch;
    p->button = m->button;
    switch (p->tool) {
    case PAINT_SELECT:
        if (m->button == BUTTON_RIGHT) {
            int id = os_popup_menu(p->win, m->x, m->y, PAINT_CONTEXT_MENU);
            if (id) paint_command(p, id);
            return;
        }
        if (p->selected && rect_contains(p->sel, x, y)) {
            p->hold = PAINT_HOLD_MOVE;
            p->grab_x = x - p->sel.x;
            p->grab_y = y - p->sel.y;
            p->copying = (m->mods & MOD_CTRL) != 0;
            break;
        }
        paint_drop(p);
        p->hold = PAINT_HOLD_SELECT;
        p->x0 = p->x1 = clamp_int(x, 0, p->cw - 1);
        p->y0 = p->y1 = clamp_int(y, 0, p->ch - 1);
        paint_show(p, paint_pad(paint_band(p), 1));
        break;
    case PAINT_PICK:
        if (!inside) return;
        p->colours[m->button == BUTTON_RIGHT] = p->canvas[y * p->cw + x];
        snprintf(p->message, sizeof p->message, "Picked the colour %06X", (unsigned)p->canvas[y * p->cw + x]);
        p->tool = p->last_tool;
        paint_draw_toolbox(p);
        paint_draw_palette(p);
        break;
    case PAINT_FILL:
        if (!inside) return;
        paint_checkpoint(p);
        paint_flood(p, x, y, p->colours[m->button == BUTTON_RIGHT]);
        paint_show(p, rect(0, 0, p->cw, p->ch));
        break;
    case PAINT_LINE: case PAINT_RECT: case PAINT_ELLIPSE:
        p->hold = PAINT_HOLD_SHAPE;
        p->x0 = p->x1 = x;
        p->y0 = p->y1 = y;
        paint_show(p, paint_shape_bounds(p));
        break;
    default:
        /* the pencil, the brush, the eraser and the airbrush */
        paint_checkpoint(p);
        p->hold = PAINT_HOLD_STROKE;
        p->x0 = x;
        p->y0 = y;
        paint_stroke(p, x, y);
        if (p->tool == PAINT_SPRAY) os_set_timer(p->win, PAINT_SPRAY_TIMER, 40);
    }
    paint_draw_status(p);
}

static void paint_drag(Paint *p, const Message *m, int x, int y) {
    switch (p->hold) {
    case PAINT_HOLD_STROKE:
        if (x != p->x0 || y != p->y0) paint_stroke(p, x, y);
        break;
    case PAINT_HOLD_SHAPE: {
        Rect before = paint_shape_bounds(p);
        p->x1 = x;
        p->y1 = y;
        if (m->mods & MOD_SHIFT) paint_constrain(p);
        paint_show(p, rect_union(before, paint_shape_bounds(p)));
        break;
    }
    case PAINT_HOLD_SELECT: {
        Rect before = paint_pad(paint_band(p), 1);
        p->x1 = clamp_int(x, 0, p->cw - 1);
        p->y1 = clamp_int(y, 0, p->ch - 1);
        paint_show(p, rect_union(before, paint_pad(paint_band(p), 1)));
        break;
    }
    case PAINT_HOLD_MOVE: paint_move_selection(p, x - p->grab_x, y - p->grab_y); break;
    }
    /* the status bar follows the pointer, or the size of what is dragged out */
    int inside = rect_contains(p->port, m->x, m->y) && x >= 0 && y >= 0 && x < p->cw && y < p->ch;
    int px = inside ? x : -1, py = inside ? y : -1;
    if (p->hold || px != p->pointer_x || py != p->pointer_y) {
        p->pointer_x = px;
        p->pointer_y = py;
        paint_draw_status(p);
    }
}

static void paint_release(Paint *p, const Message *m, int x, int y) {
    if (!p->hold || m->button != p->button) return;
    int hold = p->hold;
    p->hold = PAINT_HOLD_NONE;
    if (hold == PAINT_HOLD_STROKE && p->tool == PAINT_SPRAY) os_set_timer(p->win, PAINT_SPRAY_TIMER, 0);
    if (hold == PAINT_HOLD_SHAPE) {
        Rect before = paint_shape_bounds(p);
        p->x1 = x;
        p->y1 = y;
        if (m->mods & MOD_SHIFT) paint_constrain(p);
        paint_checkpoint(p);
        GfxContext saved = paint_on_canvas(p);
        paint_draw_shape(p);
        gfx_restore(saved);
        paint_show(p, rect_union(before, paint_shape_bounds(p)));
    } else if (hold == PAINT_HOLD_SELECT) {
        Rect band = paint_band(p);
        if (band.w > 1 || band.h > 1) {
            p->selected = 1;
            p->sel = band;
        }
        paint_show(p, paint_pad(band, 1));
    }
    paint_draw_status(p);
}

static void paint_scrolled(Paint *p, int sx, int sy) {
    sx = clamp_int(sx, 0, paint_max_sx(p));
    sy = clamp_int(sy, 0, paint_max_sy(p));
    if (sx != p->sx || sy != p->sy) {
        p->sx = sx;
        p->sy = sy;
        paint_draw_view(p, p->port);
    }
    paint_draw_bars(p);
}

static void paint_mouse(Paint *p, const Message *m) {
    if (!p->hold) {
        if (p->has_vbar && scroll_bar_message(&p->vbar, m)) {
            paint_scrolled(p, p->sx, p->vbar.top * PAINT_STEP);
            return;
        }
        if (p->has_hbar && paint_hbar_message(&p->hbar, m)) {
            paint_scrolled(p, p->hbar.top * PAINT_STEP, p->sy);
            return;
        }
    }
    if (m->type == MSG_MOUSE_WHEEL) {
        int step = -m->wheel * 3 * PAINT_STEP;
        if (m->mods & MOD_SHIFT) paint_scrolled(p, p->sx + step, p->sy);
        else paint_scrolled(p, p->sx, p->sy + step);
        return;
    }
    int x = clamp_int(m->x - paint_ox(p), -4096, 8192), y = clamp_int(m->y - paint_oy(p), -4096, 8192);
    if (m->type == MSG_MOUSE_DOWN) paint_press(p, m, x, y);
    else if (m->type == MSG_MOUSE_MOVE) paint_drag(p, m, x, y);
    else if (m->type == MSG_MOUSE_UP) paint_release(p, m, x, y);
}

/* ------------------------------------------------------------------ the process */

static void paint_enable(MenuItem *items, int id, int enabled) {
    for (; items->label; items++)
        if (items->id == id) items->flags = enabled ? 0 : MENU_DISABLED;
}

static void paint_menu_state(Paint *p) {
    paint_enable(p->edit_menu, PAINT_UNDO, p->undo_count > 0);
    paint_enable(p->edit_menu, PAINT_REDO, p->redo_count > 0);
    paint_enable(p->edit_menu, PAINT_CUT, p->selected);
    paint_enable(p->edit_menu, PAINT_COPY, p->selected);
    paint_enable(p->edit_menu, PAINT_CLEAR_SELECTION, p->selected);
}

static void paint_relayout(Paint *p) {
    os_window_pixels(p->win, &p->w, &p->h);
    paint_draw_all(p);
}

void paint_main(const char *argument) {
    Paint *p = os_alloc(sizeof *p);
    if (!p) os_exit(1);
    memcpy(p->palette, PAINT_PALETTE, sizeof p->palette);
    memcpy(p->edit_menu, PAINT_EDIT_MENU, sizeof p->edit_menu);
    p->menus[0] = (Menu){ "File", paint_file_menu };
    p->menus[1] = (Menu){ "Edit", p->edit_menu };
    p->menus[2] = (Menu){ "Image", paint_image_menu };
    p->menus[3] = (Menu){ "Help", paint_help_menu };
    p->colours[0] = BLACK;
    p->colours[1] = WHITE;
    p->tool = p->last_tool = PAINT_PENCIL;
    p->size[PAINT_BRUSH] = p->size[PAINT_ERASER] = p->size[PAINT_SPRAY] = 1;
    p->pointer_x = p->pointer_y = -1;
    copy_text(p->message, sizeof p->message, "Draw with the left or the right button");
    /* a picture to open is read first, so that the window can take its size */
    char problem[160] = "";
    int w = PAINT_NEW_W, h = PAINT_NEW_H;
    uint32_t *pixels = NULL;
    if (argument && argument[0] && os_stat(argument, NULL) == 1)
        pixels = paint_read_bmp(argument, &w, &h, problem, sizeof problem);
    if (!pixels) {
        w = PAINT_NEW_W;
        h = PAINT_NEW_H;
    }
    int client_w = clamp_int(PAINT_TOOLBOX_W + 4 + w + 2 * PAINT_MARGIN, 470, 620);
    int client_h = clamp_int(4 + h + 2 * PAINT_MARGIN + PAINT_PALETTE_H + PAINT_STATUS_H, 332, 400);
    p->win = os_window_create("Paint", &ICON_PAINT, client_w, client_h, WIN_RESIZABLE);
    if (!p->win) os_exit(1);
    os_window_set_menu(p->win, p->menus);
    os_window_min_size(p->win, 300, 280);
    if (pixels) {
        paint_set_picture(p, pixels, w, h);
        paint_opened(p, argument);
    } else {
        if (!paint_new_picture(p)) os_exit(1);
        if (argument && argument[0] && !problem[0]) {
            copy_text(p->path, sizeof p->path, argument);
            snprintf(p->message, sizeof p->message, "New picture %.40s", os_base_name(argument));
        }
    }
    paint_title(p);
    paint_relayout(p);
    paint_menu_state(p);
    if (problem[0]) os_message_box(p->win, "Paint", problem, MB_OK, MB_WARNING);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_KEY: paint_key(p, &m); break;
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: case MSG_MOUSE_WHEEL: paint_mouse(p, &m); break;
        case MSG_COMMAND: paint_command(p, m.id); break;
        case MSG_TIMER:
            if (m.id == PAINT_SPRAY_TIMER && p->hold == PAINT_HOLD_STROKE && p->tool == PAINT_SPRAY)
                paint_stroke(p, p->x0, p->y0);
            break;
        case MSG_RESIZE: paint_relayout(p); break;
        case MSG_SETTINGS: paint_draw_all(p); break;
        case MSG_DROP:
            if (os_app_for_file(m.text) == APP_VIEWER) paint_open_path(p, m.text);
            else os_message_box(p->win, "Paint", "Paint opens .bmp pictures.", MB_OK, MB_INFO);
            break;
        case MSG_CLOSE:
            if (paint_confirm(p)) os_exit(0);
            break;
        default: os_default_message(&m);
        }
        paint_menu_state(p);
    }
}
