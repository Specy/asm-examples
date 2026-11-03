/*
 * Minesweeper: squares hide mines, and an opened square shows how many of its eight neighbours
 * hide one. A left click opens a square, a right click flags one, and a click on an opened number
 * whose mines are all flagged opens the squares left around it. The first square opened is never
 * a mine. The Game menu has three levels, and the best time of each is kept in mines.ini with the
 * level last played.
 *
 * What each square shows is worked out from the game and compared with what the window shows, so
 * a click redraws only the squares that changed, each a copy of one of a few small pictures drawn
 * once at the start.
 */

enum { MINES_CELL = 16, MINES_MARGIN = 8, MINES_HEADER = 36, MINES_EDGE = 3 };
enum { MINES_MAX_COLUMNS = 30, MINES_MAX_ROWS = 16, MINES_MAX = MINES_MAX_COLUMNS * MINES_MAX_ROWS };
enum { MINES_READY, MINES_PLAYING, MINES_LOST, MINES_WON };
enum { MINES_NEW = 1, MINES_BEGINNER, MINES_INTERMEDIATE, MINES_EXPERT, MINES_BEST, MINES_EXIT,
       MINES_HOW, MINES_ABOUT };
enum { MINES_TICK = 1 };
/* What a square looks like: 0 to 8 an opened square and its number, then the rest. A square held
 * down under the pointer looks like an empty opened one, 0. */
enum { MINES_COVERED = 9, MINES_FLAG, MINES_MINE, MINES_BOOM, MINES_WRONG, MINES_LOOKS };
enum { MINES_LEVELS = 3, MINES_GAME_ITEMS = 10, MINES_HELP_ITEMS = 4, MINES_LEVEL_ITEM = 2 };
enum { MINES_COUNTER_W = 41, MINES_COUNTER_H = 24, MINES_FACE = 26 };

typedef struct {
    const char *name, *key;
    int columns, rows, mines;
} MinesLevel;

static const MinesLevel MINES_LEVEL[MINES_LEVELS] = {
    { "Beginner", "beginner", 9, 9, 10 },
    { "Intermediate", "intermediate", 16, 16, 40 },
    { "Expert", "expert", 30, 16, 99 },
};

static const MenuItem MINES_GAME_MENU[MINES_GAME_ITEMS] = {
    { "New", MINES_NEW, 0, "F2", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Beginner", MINES_BEGINNER, 0, NULL, NULL },
    { "Intermediate", MINES_INTERMEDIATE, 0, NULL, NULL },
    { "Expert", MINES_EXPERT, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Best Times...", MINES_BEST, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", MINES_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem MINES_HELP_MENU[MINES_HELP_ITEMS] = {
    { "How to Play", MINES_HOW, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "About Minesweeper", MINES_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

static const uint32_t MINES_NUMBER_COLOURS[9] = { 0, 0x0000ff, 0x008000, 0xff0000, 0x000080,
                                                  0x800000, 0x008080, 0x000000, 0x808080 };
/* The segments of each digit on the counters, a to g as bits 0 to 6, and the minus sign. */
static const unsigned char MINES_SEGMENTS[10] = { 0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f };
enum { MINES_MINUS = 0x40 };

typedef struct {
    int seconds;
    char name[32];
} MinesRecord;

typedef struct {
    Window *win;
    int w, h;                    /* the client area */
    int level, columns, rows, mine_count;
    unsigned char mine[MINES_MAX], open[MINES_MAX], flag[MINES_MAX], near[MINES_MAX];
    int state, flags, opened, seconds;
    long started;
    int boom;                    /* the mine that went off */
    int pressing, pressed;       /* the left button is held on the grid, over square `pressed` or -1 */
    ButtonTracker face;
    /* what the window shows now, so that only what differs is drawn again */
    unsigned char shown[MINES_MAX];
    int shown_face, shown_left, shown_seconds;
    uint32_t tiles[MINES_LOOKS][MINES_CELL * MINES_CELL];
    MinesRecord best[MINES_LEVELS];
    char player[32];
    MenuItem game_menu[MINES_GAME_ITEMS], help_menu[MINES_HELP_ITEMS];
    Menu menus[3];
} MinesGame;

static void mines_size(int level, int *w, int *h) {
    *w = 2 * MINES_MARGIN + MINES_EDGE + MINES_LEVEL[level].columns * MINES_CELL + 6;
    *h = 3 * MINES_MARGIN + MINES_EDGE + MINES_HEADER + MINES_LEVEL[level].rows * MINES_CELL + 6;
}
static int mines_grid_x(void) { return MINES_EDGE + MINES_MARGIN + 3; }
static int mines_grid_y(void) { return MINES_EDGE + 2 * MINES_MARGIN + MINES_HEADER + 3; }
static Rect mines_face_rect(const MinesGame *g) {
    return rect((g->w + MINES_EDGE - MINES_FACE) / 2, MINES_EDGE + MINES_MARGIN + 5, MINES_FACE, MINES_FACE);
}
static Rect mines_square_rect(const MinesGame *g, int i) {
    return rect(mines_grid_x() + i % g->columns * MINES_CELL, mines_grid_y() + i / g->columns * MINES_CELL,
                MINES_CELL, MINES_CELL);
}
static int mines_square_at(const MinesGame *g, int x, int y) {
    x -= mines_grid_x();
    y -= mines_grid_y();
    if (x < 0 || y < 0 || x >= g->columns * MINES_CELL || y >= g->rows * MINES_CELL) return -1;
    return y / MINES_CELL * g->columns + x / MINES_CELL;
}
static int mines_touching(const MinesGame *g, int a, int b) {
    return abs(a / g->columns - b / g->columns) <= 1 && abs(a % g->columns - b % g->columns) <= 1;
}
/* The squares around square i, into `around`; answers how many. */
static int mines_around(const MinesGame *g, int i, int *around) {
    int row = i / g->columns, column = i % g->columns, count = 0;
    for (int dr = -1; dr <= 1; dr++)
        for (int dc = -1; dc <= 1; dc++) {
            int r = row + dr, c = column + dc;
            if ((dr || dc) && r >= 0 && r < g->rows && c >= 0 && c < g->columns)
                around[count++] = r * g->columns + c;
        }
    return count;
}

/* ------------------------------------------------------------------ sounds */

static void mines_sound_boom(void) {
    if (!os_settings()->sounds) return;
    os_midi_program(11, 127);     /* Gunshot, played low */
    os_midi_note(11, 36, 127, 0, 900);
    os_midi_note(9, 36, 127, 0, 400);
    os_midi_note(9, 35, 127, 30, 400);
    os_midi_note(9, 49, 110, 40, 1500);
    os_midi_note(9, 57, 100, 60, 1500);
}

static void mines_sound_win(void) {
    static const int notes[] = { 72, 76, 79, 84 };
    if (!os_settings()->sounds) return;
    os_midi_program(10, 56);      /* Trumpet */
    for (int i = 0; i < 4; i++) os_midi_note(10, notes[i], 100, i * 130, i == 3 ? 700 : 120);
    os_midi_note(10, 76, 80, 390, 700);
    os_midi_note(10, 79, 80, 390, 700);
}

static void mines_quit(void) {
    for (int channel = 9; channel <= 11; channel++) {
        unsigned char off[3] = { (unsigned char)(0xb0 | channel), 123, 0 };
        os_midi_send(off, 3);
    }
    os_exit(0);
}

/* ------------------------------------------------------------------ the best times */

static void mines_load(MinesGame *g) {
    for (int level = 0; level < MINES_LEVELS; level++) {
        g->best[level].seconds = 999;
        copy_text(g->best[level].name, sizeof g->best[level].name, "Anonymous");
    }
    copy_text(g->player, sizeof g->player, "Anonymous");
    long size;
    char *data = os_read_file("mines.ini", &size);
    if (!data) return;
    for (char *line = data; *line;) {
        char *end = strchr(line, '\n');
        if (end) *end = 0;
        int value;
        if (sscanf(line, "level=%d", &value) == 1) g->level = clamp_int(value, 0, MINES_LEVELS - 1);
        for (int level = 0; level < MINES_LEVELS; level++) {
            size_t length = strlen(MINES_LEVEL[level].key);
            char name[32] = "";
            if (strncmp(line, MINES_LEVEL[level].key, length) || line[length] != '=') continue;
            if (sscanf(line + length + 1, "%d %31[^\r\n]", &value, name) >= 1) {
                g->best[level].seconds = clamp_int(value, 1, 999);
                if (name[0]) copy_text(g->best[level].name, sizeof g->best[level].name, name);
            }
        }
        if (!end) break;
        line = end + 1;
    }
    os_free(data);
}

static void mines_save(MinesGame *g) {
    char text[256];
    int length = snprintf(text, sizeof text, "level=%d\n", g->level);
    for (int level = 0; level < MINES_LEVELS; level++)
        length += snprintf(text + length, sizeof text - length, "%s=%d %s\n", MINES_LEVEL[level].key,
                           g->best[level].seconds, g->best[level].name);
    os_write_file("mines.ini", text, length);
}

static void mines_show_best(MinesGame *g) {
    char text[200];
    int length = 0;
    for (int level = 0; level < MINES_LEVELS; level++)
        length += snprintf(text + length, sizeof text - length, "%-13s%3d seconds  %.18s\n",
                           MINES_LEVEL[level].name, g->best[level].seconds, g->best[level].name);
    os_message_box(g->win, "Fastest Mine Sweepers", text, MB_OK, MB_INFO);
}

/* After a win: a new best time asks for the winner's name, then shows the table. */
static void mines_record(MinesGame *g) {
    MinesRecord *best = &g->best[g->level];
    if (g->seconds >= best->seconds) return;
    char prompt[128], name[32];
    snprintf(prompt, sizeof prompt,
             "You have the fastest time for the %s level, %d seconds. Please type your name.",
             MINES_LEVEL[g->level].key, g->seconds);
    copy_text(name, sizeof name, g->player);
    if (!os_input_box(g->win, "Minesweeper", prompt, name, sizeof name) || !name[0])
        copy_text(name, sizeof name, "Anonymous");
    copy_text(g->player, sizeof g->player, name);
    best->seconds = g->seconds;
    copy_text(best->name, sizeof best->name, name);
    mines_save(g);
    mines_show_best(g);
}

/* ------------------------------------------------------------------ the game */

static void mines_new_game(MinesGame *g) {
    const MinesLevel *level = &MINES_LEVEL[g->level];
    g->columns = level->columns;
    g->rows = level->rows;
    g->mine_count = level->mines;
    memset(g->mine, 0, sizeof g->mine);
    memset(g->open, 0, sizeof g->open);
    memset(g->flag, 0, sizeof g->flag);
    memset(g->near, 0, sizeof g->near);
    g->state = MINES_READY;
    g->flags = g->opened = g->seconds = 0;
    g->boom = -1;
    g->pressing = 0;
    g->pressed = -1;
    os_set_timer(g->win, MINES_TICK, 0);
}

/* The mines go in once the first square is chosen, never on it or around it. */
static void mines_lay(MinesGame *g, int safe) {
    int squares = g->columns * g->rows, laid = 0;
    srand((unsigned)os_now_ms() * 2654435761u ^ (unsigned)os_getpid());
    while (laid < g->mine_count) {
        int i = rand() % squares;
        if (g->mine[i] || mines_touching(g, i, safe)) continue;
        g->mine[i] = 1;
        laid++;
    }
    int around[8];
    for (int i = 0; i < squares; i++) {
        int count = mines_around(g, i, around);
        for (int k = 0; k < count; k++) g->near[i] += g->mine[around[k]];
    }
}

/* Opens a square, and every square around an empty one, without recursion: a square is opened as
 * it goes on the stack, so it goes on at most once. */
static void mines_flood(MinesGame *g, int start) {
    int stack[MINES_MAX], depth = 0, around[8];
    g->open[start] = 1;
    g->opened++;
    stack[depth++] = start;
    while (depth) {
        int i = stack[--depth];
        if (g->near[i]) continue;
        int count = mines_around(g, i, around);
        for (int k = 0; k < count; k++) {
            int next = around[k];
            if (g->open[next] || g->flag[next]) continue;
            g->open[next] = 1;
            g->opened++;
            stack[depth++] = next;
        }
    }
}

static void mines_lose(MinesGame *g, int i) {
    g->state = MINES_LOST;
    g->boom = i;
    os_set_timer(g->win, MINES_TICK, 0);
    mines_sound_boom();
}

/* Every safe square open: the mines left get their flags. */
static void mines_check_win(MinesGame *g) {
    if (g->state != MINES_PLAYING || g->opened != g->columns * g->rows - g->mine_count) return;
    g->state = MINES_WON;
    for (int i = 0; i < g->columns * g->rows; i++) g->flag[i] = g->mine[i];
    g->flags = g->mine_count;
    g->seconds = clamp_int((int)((os_now_ms() - g->started) / 1000) + 1, 1, 999);
    os_set_timer(g->win, MINES_TICK, 0);
    mines_sound_win();
}

static void mines_open_square(MinesGame *g, int i) {
    if (g->open[i] || g->flag[i]) return;
    if (g->state == MINES_READY) {
        mines_lay(g, i);
        g->state = MINES_PLAYING;
        g->started = os_now_ms();
        g->seconds = 1;
        os_set_timer(g->win, MINES_TICK, 250);
    }
    if (g->mine[i]) {
        mines_lose(g, i);
        return;
    }
    mines_flood(g, i);
    mines_check_win(g);
}

/* A click on an opened number with as many flags around it: the other squares around open. */
static void mines_chord(MinesGame *g, int i) {
    int around[8], count = mines_around(g, i, around), flags = 0;
    if (!g->open[i] || !g->near[i]) return;
    for (int k = 0; k < count; k++) flags += g->flag[around[k]];
    if (flags != g->near[i]) return;
    for (int k = 0; k < count && g->state == MINES_PLAYING; k++) {
        int next = around[k];
        if (g->open[next] || g->flag[next]) continue;
        if (g->mine[next]) mines_lose(g, next);
        else mines_flood(g, next);
    }
    mines_check_win(g);
}

/* ------------------------------------------------------------------ drawing */

static void mines_draw_mine(void) {
    fill_circle(8, 8, 4, BLACK);
    hline(2, 8, 13, BLACK);
    vline(8, 2, 13, BLACK);
    put_pixel(4, 4, BLACK);
    put_pixel(12, 4, BLACK);
    put_pixel(4, 12, BLACK);
    put_pixel(12, 12, BLACK);
    fill_rect(6, 6, 2, 2, WHITE);
}

static void mines_draw_flag(void) {
    fill_rect(4, 3, 5, 5, RED);
    fill_rect(9, 4, 1, 3, RED);
    vline(9, 3, 9, BLACK);
    fill_rect(6, 11, 6, 1, BLACK);
    fill_rect(4, 12, 10, 2, BLACK);
}

/* One of the pictures a square may show, drawn at 0, 0. */
static void mines_draw_tile(int look) {
    enum { C = MINES_CELL };
    if (look >= MINES_COVERED && look != MINES_MINE && look != MINES_BOOM) {
        fill_rect(0, 0, C, C, FACE);
        hline(0, 0, C - 1, WHITE);
        hline(0, 1, C - 2, WHITE);
        vline(0, 0, C - 1, WHITE);
        vline(1, 0, C - 2, WHITE);
        hline(1, C - 1, C - 1, SHADOW);
        hline(2, C - 2, C - 2, SHADOW);
        vline(C - 1, 1, C - 1, SHADOW);
        vline(C - 2, 2, C - 2, SHADOW);
        if (look == MINES_FLAG || look == MINES_WRONG) mines_draw_flag();
        if (look == MINES_WRONG) {
            draw_line(2, 2, 13, 13, RED);
            draw_line(13, 2, 2, 13, RED);
        }
        return;
    }
    fill_rect(0, 0, C, C, look == MINES_BOOM ? RED : FACE);
    hline(0, 0, C, SHADOW);
    vline(0, 0, C, SHADOW);
    if (look == MINES_MINE || look == MINES_BOOM) {
        mines_draw_mine();
    } else if (look > 0) {
        char digit[2] = { (char)('0' + look), 0 };
        draw_text_bold(4, 2, digit, MINES_NUMBER_COLOURS[look]);
    }
}

static void mines_make_tiles(MinesGame *g) {
    GfxContext saved = gfx_save();
    for (int look = 0; look < MINES_LOOKS; look++) {
        gfx_target(g->tiles[look], MINES_CELL, MINES_CELL, MINES_CELL);
        mines_draw_tile(look);
    }
    gfx_restore(saved);
}

static int mines_look(const MinesGame *g, int i) {
    int lost = g->state == MINES_LOST, playing = g->state == MINES_READY || g->state == MINES_PLAYING;
    if (g->open[i]) return g->near[i];
    if (g->flag[i]) return lost && !g->mine[i] ? MINES_WRONG : MINES_FLAG;
    if (lost && g->mine[i]) return i == g->boom ? MINES_BOOM : MINES_MINE;
    if (playing && g->pressing && g->pressed >= 0 &&
        (i == g->pressed || (g->open[g->pressed] && mines_touching(g, i, g->pressed))))
        return 0;
    return MINES_COVERED;
}

/* The face: 0 smiling, 1 worried while a square is held, 2 sunglasses, 3 crossed eyes; plus 4
 * while the face itself is held down. */
static int mines_face_look(const MinesGame *g) {
    int look = g->state == MINES_WON ? 2 : g->state == MINES_LOST ? 3 : g->pressing ? 1 : 0;
    return look + (button_down(&g->face, 0) ? 4 : 0);
}

static void mines_draw_face(const MinesGame *g, int look) {
    Rect face = mines_face_rect(g);
    int down = look >= 4;
    if (down) {
        fill_rect(face.x, face.y, face.w, face.h, FACE);
        frame_rect(face.x, face.y, face.w, face.h, SHADOW);
    } else {
        draw_button(face.x, face.y, face.w, face.h, "", 0);
    }
    int cx = face.x + 13 + down, cy = face.y + 13 + down;
    fill_circle(cx, cy, 9, BLACK);
    fill_circle(cx, cy, 8, YELLOW);
    switch (look & 3) {
    case 3:
        for (int i = -1; i <= 1; i++) {
            put_pixel(cx - 4 + i, cy - 3 + i, BLACK);
            put_pixel(cx - 4 + i, cy - 3 - i, BLACK);
            put_pixel(cx + 3 + i, cy - 3 + i, BLACK);
            put_pixel(cx + 3 + i, cy - 3 - i, BLACK);
        }
        hline(cx - 3, cy + 3, 7, BLACK);
        put_pixel(cx - 4, cy + 4, BLACK);
        put_pixel(cx + 4, cy + 4, BLACK);
        break;
    case 2:
        hline(cx - 7, cy - 3, 15, BLACK);
        fill_rect(cx - 6, cy - 3, 5, 3, BLACK);
        fill_rect(cx + 2, cy - 3, 5, 3, BLACK);
        hline(cx - 3, cy + 4, 7, BLACK);
        put_pixel(cx - 4, cy + 3, BLACK);
        put_pixel(cx + 4, cy + 3, BLACK);
        break;
    default:
        fill_rect(cx - 4, cy - 4, 2, 2, BLACK);
        fill_rect(cx + 3, cy - 4, 2, 2, BLACK);
        if (look & 1) {
            draw_circle(cx, cy + 4, 2, BLACK);
        } else {
            hline(cx - 3, cy + 4, 7, BLACK);
            put_pixel(cx - 4, cy + 3, BLACK);
            put_pixel(cx + 4, cy + 3, BLACK);
        }
    }
}

/* A counter of three red digits of seven segments, the unlit ones dark. */
static void mines_draw_counter(int x, int y, int value) {
    char digits[8];
    snprintf(digits, sizeof digits, "%03d", clamp_int(value, -99, 999));
    fill_rect(x, y, MINES_COUNTER_W, MINES_COUNTER_H, BLACK);
    bevel_thin_sunken(x, y, MINES_COUNTER_W, MINES_COUNTER_H);
    for (int k = 0; k < 3; k++) {
        int segments = digits[k] == '-' ? MINES_MINUS : MINES_SEGMENTS[digits[k] - '0'];
        int dx = x + 3 + k * 13, dy = y + 2;
        /* a to g: top, top right, bottom right, bottom, bottom left, top left, middle */
        const Rect parts[7] = { rect(dx + 2, dy, 7, 2), rect(dx + 9, dy + 2, 2, 7), rect(dx + 9, dy + 11, 2, 7),
                                rect(dx + 2, dy + 18, 7, 2), rect(dx, dy + 11, 2, 7), rect(dx, dy + 2, 2, 7),
                                rect(dx + 2, dy + 9, 7, 2) };
        for (int s = 0; s < 7; s++)
            fill_rect(parts[s].x, parts[s].y, parts[s].w, parts[s].h, segments >> s & 1 ? RED : 0x400000);
    }
}

/* Draws whatever differs from what the window shows, and shows it. */
static void mines_refresh(MinesGame *g) {
    os_window_draw(g->win);
    Rect dirty = rect(0, 0, 0, 0);
    for (int i = 0; i < g->columns * g->rows; i++) {
        int look = mines_look(g, i);
        if (look == g->shown[i]) continue;
        Rect r = mines_square_rect(g, i);
        blit(g->tiles[look], MINES_CELL, r.x, r.y, MINES_CELL, MINES_CELL);
        g->shown[i] = (unsigned char)look;
        dirty = rect_union(dirty, r);
    }
    int face = mines_face_look(g);
    if (face != g->shown_face) {
        mines_draw_face(g, face);
        g->shown_face = face;
        dirty = rect_union(dirty, mines_face_rect(g));
    }
    int left = g->mine_count - g->flags, y = MINES_EDGE + MINES_MARGIN + 6;
    if (left != g->shown_left) {
        mines_draw_counter(MINES_EDGE + MINES_MARGIN + 6, y, left);
        g->shown_left = left;
        dirty = rect_union(dirty, rect(MINES_EDGE + MINES_MARGIN + 6, y, MINES_COUNTER_W, MINES_COUNTER_H));
    }
    if (g->seconds != g->shown_seconds) {
        int x = g->w - MINES_MARGIN - 6 - MINES_COUNTER_W;
        mines_draw_counter(x, y, g->seconds);
        g->shown_seconds = g->seconds;
        dirty = rect_union(dirty, rect(x, y, MINES_COUNTER_W, MINES_COUNTER_H));
    }
    if (!rect_empty(dirty)) os_window_update(g->win, dirty.x, dirty.y, dirty.w, dirty.h);
}

static void mines_draw_all(MinesGame *g) {
    os_window_pixels(g->win, &g->w, &g->h);
    os_window_draw(g->win);
    fill_rect(0, 0, g->w, g->h, FACE);
    fill_rect(0, 0, g->w, MINES_EDGE, WHITE);
    fill_rect(0, 0, MINES_EDGE, g->h, WHITE);
    int inside = g->w - MINES_EDGE - 2 * MINES_MARGIN;
    bevel_sunken(MINES_EDGE + MINES_MARGIN, MINES_EDGE + MINES_MARGIN, inside, MINES_HEADER);
    bevel_sunken(mines_grid_x() - 3, mines_grid_y() - 3, g->columns * MINES_CELL + 6, g->rows * MINES_CELL + 6);
    memset(g->shown, 0xff, sizeof g->shown);
    g->shown_face = g->shown_left = g->shown_seconds = -1000;
    mines_refresh(g);
    os_window_update_all(g->win);
}

/* ------------------------------------------------------------------ levels and windows */

static void mines_check_level(MinesGame *g) {
    for (int level = 0; level < MINES_LEVELS; level++) {
        MenuItem *item = &g->game_menu[MINES_LEVEL_ITEM + level];
        item->flags = level == g->level ? item->flags | MENU_CHECKED : item->flags & ~MENU_CHECKED;
    }
}

/* Opens the window at the size of the level, in place of the one before if there is one: windows
 * cannot be resized by their process, so a new level gets a new window. */
static int mines_open_window(MinesGame *g, int flags) {
    int w, h;
    mines_size(g->level, &w, &h);
    Window *win = os_window_create("Minesweeper", &ICON_MINES, w, h, flags);
    if (!win) return 0;
    os_window_set_menu(win, g->menus);
    if (g->win) os_window_close(g->win);
    g->win = win;
    g->w = w;
    g->h = h;
    return 1;
}

static void mines_set_level(MinesGame *g, int level) {
    int before = g->level;
    g->level = level;
    if (level != before) {
        if (!mines_open_window(g, WIN_CENTER)) {
            g->level = before;
            os_beep();
            return;
        }
        mines_check_level(g);
        mines_save(g);
    }
    mines_new_game(g);
    mines_draw_all(g);
}

/* ------------------------------------------------------------------ the pointer, keys and menus */

static void mines_mouse(MinesGame *g, const Message *m) {
    Rect face = mines_face_rect(g);
    int changed, clicked = track_buttons(&g->face, &face, 1, m, &changed);
    if (clicked == 0) {
        mines_new_game(g);
        mines_refresh(g);
        return;
    }
    if (changed || g->face.pressed >= 0) {
        mines_refresh(g);
        return;
    }
    int i = mines_square_at(g, m->x, m->y);
    int over = g->state == MINES_LOST || g->state == MINES_WON;
    if (m->type == MSG_MOUSE_DOWN && m->button == BUTTON_RIGHT) {
        if (i < 0 || over || g->open[i]) return;
        g->flag[i] = !g->flag[i];
        g->flags += g->flag[i] ? 1 : -1;
    } else if (m->type == MSG_MOUSE_DOWN && m->button == BUTTON_LEFT) {
        if (i < 0 || over) return;
        g->pressing = 1;
        g->pressed = i;
    } else if (m->type == MSG_MOUSE_MOVE && g->pressing) {
        if (i == g->pressed) return;
        g->pressed = i;
    } else if (m->type == MSG_MOUSE_UP && m->button == BUTTON_LEFT && g->pressing) {
        g->pressing = 0;
        g->pressed = -1;
        if (i >= 0 && g->open[i]) mines_chord(g, i);
        else if (i >= 0) mines_open_square(g, i);
        mines_refresh(g);
        if (g->state == MINES_WON) mines_record(g);
        return;
    } else {
        return;
    }
    mines_refresh(g);
}

static void mines_command(MinesGame *g, int id) {
    switch (id) {
    case MINES_NEW:
        mines_new_game(g);
        mines_refresh(g);
        break;
    case MINES_BEGINNER: case MINES_INTERMEDIATE: case MINES_EXPERT:
        mines_set_level(g, id - MINES_BEGINNER);
        break;
    case MINES_BEST: mines_show_best(g); break;
    case MINES_EXIT: mines_quit(); break;
    case MINES_HOW:
        os_message_box(g->win, "How to Play",
                       "Find the mines without opening any of them.\n\n"
                       "Left click opens a square; its number says how many mines touch it.\n"
                       "Right click flags a square you think hides a mine.\n"
                       "Click an opened number whose mines are all flagged to open the rest around it.\n\n"
                       "F2 or the face starts a new game.",
                       MB_OK, MB_INFO);
        break;
    case MINES_ABOUT:
        os_message_box(g->win, "About Minesweeper",
                       "Minesweeper for asmOS.\nThree levels, with the best time of each kept in mines.ini.",
                       MB_OK, MB_INFO);
        break;
    }
}

void mines_main(const char *argument) {
    (void)argument;
    MinesGame *g = os_alloc(sizeof *g);
    if (!g) os_exit(1);
    memcpy(g->game_menu, MINES_GAME_MENU, sizeof g->game_menu);
    memcpy(g->help_menu, MINES_HELP_MENU, sizeof g->help_menu);
    g->menus[0] = (Menu){ "Game", g->game_menu };
    g->menus[1] = (Menu){ "Help", g->help_menu };
    g->face.pressed = -1;
    mines_load(g);
    mines_check_level(g);
    mines_make_tiles(g);
    if (!mines_open_window(g, 0)) os_exit(1);
    mines_new_game(g);
    for (;;) {
        Message m;
        os_get_message(&m);
        /* what is still queued for a window a new level replaced */
        if (m.win && m.win != g->win && m.type != MSG_QUIT) continue;
        switch (m.type) {
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: mines_mouse(g, &m); break;
        case MSG_KEY:
            if (m.key == KEY_F2) mines_command(g, MINES_NEW);
            break;
        case MSG_COMMAND: mines_command(g, m.id); break;
        case MSG_TIMER: {
            int seconds = clamp_int((int)((os_now_ms() - g->started) / 1000) + 1, 1, 999);
            if (g->state == MINES_PLAYING && seconds != g->seconds) {
                g->seconds = seconds;
                mines_refresh(g);
            }
            break;
        }
        case MSG_RESIZE: mines_draw_all(g); break;
        case MSG_CLOSE: case MSG_QUIT: mines_quit(); break;
        default: os_default_message(&m);
        }
    }
}
