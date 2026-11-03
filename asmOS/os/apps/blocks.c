/*
 * Blocks: pieces of four squares fall into a well ten squares wide and twenty deep. The arrows
 * move them, Up turns them, Down drops them faster and Space drops them at once; a row filled from
 * side to side goes, and every ten rows the pieces fall faster. The music is Korobeiniki, a
 * Russian folk song of the nineteenth century, on the MIDI synthesizer. The best score is kept in
 * blocks.ini.
 *
 * The game moves on a timer, by the time that really passed, and never spins. What each square of
 * the well shows is worked out from the game and compared with what the window shows, so a step
 * redraws only the squares that changed.
 */

enum { BLOCKS_COLUMNS = 10, BLOCKS_ROWS = 20, BLOCKS_CELL = 17 };
enum { BLOCKS_WELL_X = 12, BLOCKS_WELL_Y = 12, BLOCKS_PANEL_X = BLOCKS_WELL_X + BLOCKS_COLUMNS * BLOCKS_CELL + 16,
       BLOCKS_PANEL_W = 112, BLOCKS_W = BLOCKS_PANEL_X + BLOCKS_PANEL_W + 12,
       BLOCKS_H = 2 * BLOCKS_WELL_Y + BLOCKS_ROWS * BLOCKS_CELL };
enum { BLOCKS_READY, BLOCKS_PLAYING, BLOCKS_CLEARING, BLOCKS_PAUSED, BLOCKS_OVER };
enum { BLOCKS_NEW = 1, BLOCKS_PAUSE, BLOCKS_MUSIC, BLOCKS_EXIT, BLOCKS_HOW, BLOCKS_ABOUT };
enum { BLOCKS_TICK = 1, BLOCKS_TICK_MS = 20 };
/* Milliseconds: how long a piece may rest on the stack before it locks, how long full rows flash,
 * and how long a side arrow is held before the piece slides, then how often it slides. */
enum { BLOCKS_LOCK_MS = 450, BLOCKS_LOCK_MOVES = 15, BLOCKS_FLASH_MS = 320, BLOCKS_SLIDE_DELAY = 170,
       BLOCKS_SLIDE_RATE = 50, BLOCKS_SOFT_MS = 35 };
/* What a square shows: 0 empty, 1 to 7 a piece's colour, 8 to 14 the shadow of where a piece will
 * land, then the grey of a stack that lost, the white of a row going, and "not drawn yet". */
enum { BLOCKS_GHOST = 7, BLOCKS_GREY = 15, BLOCKS_FLASH = 16, BLOCKS_UNKNOWN = 255 };
/* The MIDI channels: the tune, its bass and the sound effects; 9 is the drums. */
enum { BLOCKS_TUNE_CHANNEL = 6, BLOCKS_BASS_CHANNEL = 7, BLOCKS_EFFECT_CHANNEL = 8, BLOCKS_LOOKAHEAD = 60 };
enum { BLOCKS_GAME_ITEMS = 8, BLOCKS_HELP_ITEMS = 4, BLOCKS_PAUSE_ITEM = 1, BLOCKS_MUSIC_ITEM = 3 };

static const MenuItem BLOCKS_GAME_MENU[BLOCKS_GAME_ITEMS] = {
    { "New Game", BLOCKS_NEW, 0, "F2", NULL },
    { "Pause", BLOCKS_PAUSE, 0, "P", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Music", BLOCKS_MUSIC, MENU_CHECKED, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", BLOCKS_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem BLOCKS_HELP_MENU[BLOCKS_HELP_ITEMS] = {
    { "How to Play", BLOCKS_HOW, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "About Blocks", BLOCKS_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

/* A piece: its four squares in a box `size` squares wide, turned a quarter at a time within it. */
typedef struct {
    int size;
    signed char cells[4][2];
    uint32_t colour;
} BlocksPiece;

static const BlocksPiece BLOCKS_PIECES[7] = {
    { 4, { { 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 1 } }, 0x37b3a6 },   /* the bar, jade */
    { 2, { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } }, 0xe3ae1c },   /* the square, amber */
    { 3, { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } }, 0xd6497a },   /* the tee, rose */
    { 3, { { 1, 0 }, { 2, 0 }, { 0, 1 }, { 1, 1 } }, 0x7cb342 },   /* the ess, lime */
    { 3, { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 2, 1 } }, 0x3f7fd9 },   /* the zed, cobalt */
    { 3, { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } }, 0x9a62d3 },   /* the jay, amethyst */
    { 3, { { 2, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 } }, 0xe8783a },   /* the ell, copper */
};

/* Milliseconds a piece takes to fall one row, by level. */
static const int BLOCKS_SPEED[] = { 800, 700, 610, 530, 450, 380, 320, 260, 210, 170, 135, 110, 90, 75, 60 };
static const int BLOCKS_LINE_SCORE[5] = { 0, 100, 300, 500, 800 };

/* Korobeiniki, the folk tune: its notes (0 a rest) and their lengths in eighths, eight bars of
 * four beats. Under it, a bass on the roots of the chords, one bar each. */
typedef struct {
    unsigned char note, eighths;
} BlocksNote;
static const BlocksNote BLOCKS_TUNE[] = {
    { 76, 2 }, { 71, 1 }, { 72, 1 }, { 74, 2 }, { 72, 1 }, { 71, 1 },
    { 69, 2 }, { 69, 1 }, { 72, 1 }, { 76, 2 }, { 74, 1 }, { 72, 1 },
    { 71, 3 }, { 72, 1 }, { 74, 2 }, { 76, 2 },
    { 72, 2 }, { 69, 2 }, { 69, 2 }, { 0, 2 },
    { 0, 1 }, { 74, 2 }, { 77, 1 }, { 81, 2 }, { 79, 1 }, { 77, 1 },
    { 76, 3 }, { 72, 1 }, { 76, 2 }, { 74, 1 }, { 72, 1 },
    { 71, 2 }, { 71, 1 }, { 72, 1 }, { 74, 2 }, { 76, 2 },
    { 72, 2 }, { 69, 2 }, { 69, 2 }, { 0, 2 },
};
enum { BLOCKS_TUNE_NOTES = sizeof BLOCKS_TUNE / sizeof BLOCKS_TUNE[0], BLOCKS_TUNE_EIGHTHS = 64 };
static const unsigned char BLOCKS_BASS[8] = { 40, 45, 40, 45, 38, 36, 40, 45 };   /* E A E A D C E A */

enum { BLOCKS_WELL_BACK = 0x10182a, BLOCKS_WELL_DOT = 0x24304a, BLOCKS_INK = 0xffc94a };

typedef struct {
    Window *win;
    unsigned char well[BLOCKS_ROWS][BLOCKS_COLUMNS];    /* 0, or the piece + 1 that left a square there */
    unsigned char shown[BLOCKS_ROWS][BLOCKS_COLUMNS];   /* what the window shows of each square */
    unsigned char full[BLOCKS_ROWS];                    /* the rows going */
    int state, resume;                                  /* resume: the state a pause goes back to */
    int piece, rotation, px, py, next;
    int bag[7], bag_left;
    long score, high;
    int lines, level, new_high;
    int fall_ms, lock_ms, lock_moves, clear_ms;
    unsigned char held[(KEY_MAX + 8) / 8];              /* the keys held, a bit each, to tell repeats */
    int slide, slide_ms;                                /* slide: -1 or 1 while a side arrow is held */
    long last_ms;
    int changed;
    long shown_score, shown_high;
    int shown_lines, shown_level, shown_next;
    /* the music, on a clock of its own that stops while the game does */
    int music, music_playing;
    long music_clock, tune_at, bass_at;
    int tune_index, bass_step;
    MenuItem game_menu[BLOCKS_GAME_ITEMS], help_menu[BLOCKS_HELP_ITEMS];
    Menu menus[3];
} BlocksGame;

/* ------------------------------------------------------------------ the pieces */

static void blocks_cells(int piece, int rotation, int cells[4][2]) {
    const BlocksPiece *p = &BLOCKS_PIECES[piece];
    for (int k = 0; k < 4; k++) {
        int x = p->cells[k][0], y = p->cells[k][1];
        for (int turn = 0; turn < (rotation & 3); turn++) {
            int was = x;
            x = p->size - 1 - y;
            y = was;
        }
        cells[k][0] = x;
        cells[k][1] = y;
    }
}

/* Whether a piece fits at px, py: inside the walls and the floor, on empty squares. Above the
 * well is free. */
static int blocks_fits(const BlocksGame *b, int piece, int rotation, int px, int py) {
    int cells[4][2];
    blocks_cells(piece, rotation, cells);
    for (int k = 0; k < 4; k++) {
        int x = px + cells[k][0], y = py + cells[k][1];
        if (x < 0 || x >= BLOCKS_COLUMNS || y >= BLOCKS_ROWS) return 0;
        if (y >= 0 && b->well[y][x]) return 0;
    }
    return 1;
}

/* The pieces come in bags of all seven, shuffled, so none is long in coming. */
static int blocks_from_bag(BlocksGame *b) {
    if (!b->bag_left) {
        for (int i = 0; i < 7; i++) b->bag[i] = i;
        for (int i = 6; i > 0; i--) {
            int j = rand() % (i + 1), was = b->bag[i];
            b->bag[i] = b->bag[j];
            b->bag[j] = was;
        }
        b->bag_left = 7;
    }
    return b->bag[--b->bag_left];
}

static int blocks_landing(const BlocksGame *b) {
    int y = b->py;
    while (blocks_fits(b, b->piece, b->rotation, b->px, y + 1)) y++;
    return y;
}

/* ------------------------------------------------------------------ sound */

static void blocks_controller(int channel, int controller) {
    unsigned char bytes[3] = { (unsigned char)(0xb0 | channel), (unsigned char)controller, 0 };
    os_midi_send(bytes, 3);
}

static void blocks_music_stop(BlocksGame *b) {
    if (!b->music_playing) return;
    b->music_playing = 0;
    blocks_controller(BLOCKS_TUNE_CHANNEL, 123);
    blocks_controller(BLOCKS_BASS_CHANNEL, 123);
}

/* The music plays while the game does, if the Music item is checked. */
static void blocks_music_follow(BlocksGame *b) {
    int playing = b->music && (b->state == BLOCKS_PLAYING || b->state == BLOCKS_CLEARING);
    if (!playing) {
        blocks_music_stop(b);
    } else if (!b->music_playing) {
        b->music_playing = 1;
        os_midi_program(BLOCKS_TUNE_CHANNEL, 21);    /* Accordion */
        os_midi_program(BLOCKS_BASS_CHANNEL, 33);    /* Electric Bass (finger) */
        b->tune_at = b->bass_at = b->music_clock;
    }
}

/* Moves the music on by `ms` and sends the notes that start soon, each with its exact delay. The
 * tempo rises with the level. */
static void blocks_music_run(BlocksGame *b, int ms) {
    if (!b->music_playing) return;
    b->music_clock += ms;
    int eighth = 30000 / min_int(200, 140 + 6 * (b->level - 1));
    long until = b->music_clock + BLOCKS_LOOKAHEAD;
    while (b->tune_at < until) {
        const BlocksNote *note = &BLOCKS_TUNE[b->tune_index];
        int length = note->eighths * eighth;
        if (note->note)
            os_midi_note(BLOCKS_TUNE_CHANNEL, note->note, 92, max_int(0, (int)(b->tune_at - b->music_clock)),
                         length * 7 / 8);
        b->tune_at += length;
        b->tune_index = (b->tune_index + 1) % BLOCKS_TUNE_NOTES;
    }
    while (b->bass_at < until) {
        int odd = b->bass_step & 1, note = BLOCKS_BASS[b->bass_step / 8] + (odd ? 12 : 0);
        os_midi_note(BLOCKS_BASS_CHANNEL, note, odd ? 62 : 82, max_int(0, (int)(b->bass_at - b->music_clock)),
                     eighth * 3 / 4);
        b->bass_at += eighth;
        b->bass_step = (b->bass_step + 1) % BLOCKS_TUNE_EIGHTHS;
    }
}

static void blocks_sound_lines(int count) {
    static const int notes[5] = { 84, 88, 91, 96, 100 };
    if (!os_settings()->sounds) return;
    os_midi_program(BLOCKS_EFFECT_CHANNEL, 9);      /* Glockenspiel */
    for (int i = 0; i < count + 1; i++) os_midi_note(BLOCKS_EFFECT_CHANNEL, notes[i], 100, i * 55, 300);
}

static void blocks_sound_drop(void) {
    if (!os_settings()->sounds) return;
    os_midi_note(9, 35, 120, 0, 120);
    os_midi_note(9, 42, 70, 0, 60);
}

static void blocks_sound_over(void) {
    if (!os_settings()->sounds) return;
    os_midi_program(BLOCKS_EFFECT_CHANNEL, 58);     /* Tuba */
    for (int i = 0; i < 4; i++) os_midi_note(BLOCKS_EFFECT_CHANNEL, 52 - i, 100, i * 300, i == 3 ? 900 : 260);
}

/* ------------------------------------------------------------------ the high score */

static void blocks_load(BlocksGame *b) {
    long size;
    char *data = os_read_file("blocks.ini", &size);
    b->music = 1;
    if (!data) return;
    for (char *line = data; line && *line;) {
        char *end = strchr(line, '\n');
        long high;
        int music;
        if (sscanf(line, "high=%ld", &high) == 1) b->high = high > 0 ? high : 0;
        if (sscanf(line, "music=%d", &music) == 1) b->music = music != 0;
        line = end ? end + 1 : NULL;
    }
    os_free(data);
}

static void blocks_save(BlocksGame *b) {
    char text[64];
    int length = snprintf(text, sizeof text, "high=%ld\nmusic=%d\n", b->high, b->music);
    os_write_file("blocks.ini", text, length);
}

/* ------------------------------------------------------------------ drawing */

/* A colour mixed with another, `part` 256ths of the way. */
static uint32_t blocks_mix(uint32_t from, uint32_t to, int part) {
    uint32_t mixed = 0;
    for (int shift = 0; shift < 24; shift += 8) {
        int a = from >> shift & 255, c = to >> shift & 255;
        mixed |= (uint32_t)(a + (c - a) * part / 256) << shift;
    }
    return mixed;
}

/* A square at x, y: a bevelled tile with a lighter facet in the middle, or the dotted dark of
 * the empty well, or a piece's outline where it will land. */
static void blocks_draw_square(int x, int y, int look) {
    enum { C = BLOCKS_CELL };
    if (look == 0 || (look > BLOCKS_GHOST && look <= 2 * BLOCKS_GHOST)) {
        fill_rect(x, y, C, C, BLOCKS_WELL_BACK);
        if (look) {
            uint32_t colour = BLOCKS_PIECES[look - 1 - BLOCKS_GHOST].colour;
            frame_rect(x + 1, y + 1, C - 2, C - 2, blocks_mix(colour, BLOCKS_WELL_BACK, 110));
        } else {
            put_pixel(x + C / 2, y + C / 2, BLOCKS_WELL_DOT);
        }
        return;
    }
    uint32_t colour = look == BLOCKS_GREY ? 0x6c7080
                      : look == BLOCKS_FLASH ? 0xf4f4f4 : BLOCKS_PIECES[look - 1].colour;
    uint32_t light = blocks_mix(colour, WHITE, 120), dark = blocks_mix(colour, BLACK, 110);
    fill_rect(x, y, C, C, colour);
    fill_rect(x, y, C, 2, light);
    fill_rect(x, y, 2, C, light);
    fill_rect(x, y + C - 2, C, 2, dark);
    fill_rect(x + C - 2, y + 1, 2, C - 1, dark);
    fill_rect(x + 5, y + 5, C - 10, C - 10, blocks_mix(colour, WHITE, 60));
    put_pixel(x + 5, y + 5, light);
}

/* What every square of the well should show now. */
static void blocks_compose(const BlocksGame *b, unsigned char want[BLOCKS_ROWS][BLOCKS_COLUMNS]) {
    int hidden = b->state == BLOCKS_PAUSED || b->state == BLOCKS_READY;
    for (int y = 0; y < BLOCKS_ROWS; y++)
        for (int x = 0; x < BLOCKS_COLUMNS; x++) {
            int look = hidden ? 0 : b->well[y][x];
            if (look && b->state == BLOCKS_OVER) look = BLOCKS_GREY;
            if (b->state == BLOCKS_CLEARING && b->full[y] && b->clear_ms / 80 % 2 == 0) look = BLOCKS_FLASH;
            want[y][x] = (unsigned char)look;
        }
    if (b->state != BLOCKS_PLAYING) return;
    int cells[4][2], landing = blocks_landing(b);
    blocks_cells(b->piece, b->rotation, cells);
    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < 4; k++) {
            int x = b->px + cells[k][0], y = (pass ? b->py : landing) + cells[k][1];
            if (y >= 0) want[y][x] = (unsigned char)(b->piece + 1 + (pass ? 0 : BLOCKS_GHOST));
        }
}

/* A number in a sunken field of the panel, under its label. */
static void blocks_draw_value(int y, const char *label, long value) {
    char text[24];
    snprintf(text, sizeof text, "%ld", value);
    fill_rect(BLOCKS_PANEL_X, y, BLOCKS_PANEL_W, 16, FACE);
    draw_text_bold(BLOCKS_PANEL_X + 2, y + 1, label, NAVY);
    draw_field(BLOCKS_PANEL_X, y + 16, BLOCKS_PANEL_W, 22, BLOCKS_WELL_BACK);
    draw_text_bold(BLOCKS_PANEL_X + BLOCKS_PANEL_W - 8 - text_width(text), y + 21, text, BLOCKS_INK);
}

static void blocks_draw_next(const BlocksGame *b) {
    enum { BOX_Y = BLOCKS_WELL_Y + 16, BOX_H = 64 };
    draw_text_bold(BLOCKS_PANEL_X + 2, BLOCKS_WELL_Y + 1, "NEXT", NAVY);
    draw_field(BLOCKS_PANEL_X, BOX_Y, BLOCKS_PANEL_W, BOX_H, BLOCKS_WELL_BACK);
    if (b->state == BLOCKS_READY) return;
    int cells[4][2], left = 4, right = 0, top = 4, bottom = 0;
    blocks_cells(b->next, 0, cells);
    for (int k = 0; k < 4; k++) {
        left = min_int(left, cells[k][0]);
        right = max_int(right, cells[k][0]);
        top = min_int(top, cells[k][1]);
        bottom = max_int(bottom, cells[k][1]);
    }
    int x = BLOCKS_PANEL_X + (BLOCKS_PANEL_W - (right - left + 1) * BLOCKS_CELL) / 2 - left * BLOCKS_CELL;
    int y = BOX_Y + (BOX_H - (bottom - top + 1) * BLOCKS_CELL) / 2 - top * BLOCKS_CELL;
    for (int k = 0; k < 4; k++)
        blocks_draw_square(x + cells[k][0] * BLOCKS_CELL, y + cells[k][1] * BLOCKS_CELL, b->next + 1);
}

/* A box over the middle of the well, for the title, the pause and the end of a game. */
static void blocks_draw_banner(BlocksGame *b) {
    const char *lines[5] = { NULL };
    char score[32], high[32];
    int count = 0;
    if (b->state == BLOCKS_READY) {
        lines[count++] = "B L O C K S";
        lines[count++] = "";
        lines[count++] = "Press Space";
        lines[count++] = "to start";
    } else if (b->state == BLOCKS_PAUSED) {
        lines[count++] = "PAUSED";
        lines[count++] = "";
        lines[count++] = "P to go on";
    } else if (b->state == BLOCKS_OVER) {
        snprintf(score, sizeof score, "Score %ld", b->score);
        snprintf(high, sizeof high, "%s", b->new_high ? "New high score!" : "");
        lines[count++] = "GAME OVER";
        lines[count++] = score;
        lines[count++] = high;
        lines[count++] = "F2 to play again";
    } else {
        return;
    }
    int w = BLOCKS_COLUMNS * BLOCKS_CELL - 16, h = count * 16 + 20;
    int x = BLOCKS_WELL_X + 8, y = BLOCKS_WELL_Y + (BLOCKS_ROWS * BLOCKS_CELL - h) / 2;
    fill_rect(x, y, w, h, 0x1c2848);
    frame_rect(x, y, w, h, BLOCKS_INK);
    frame_rect(x + 2, y + 2, w - 4, h - 4, blocks_mix(BLOCKS_INK, 0x1c2848, 128));
    for (int i = 0; i < count; i++) {
        uint32_t ink = i == 0 ? BLOCKS_INK : WHITE;
        int tx = x + (w - text_width(lines[i])) / 2, ty = y + 11 + i * 16;
        if (i == 0) draw_text_bold(tx, ty, lines[i], ink);
        else draw_text(tx, ty, lines[i], ink);
    }
    /* the squares under the box are drawn again when it goes */
    for (int row = (y - BLOCKS_WELL_Y) / BLOCKS_CELL; row <= (y + h - 1 - BLOCKS_WELL_Y) / BLOCKS_CELL; row++)
        memset(b->shown[row], BLOCKS_UNKNOWN, BLOCKS_COLUMNS);
    os_window_update(b->win, x, y, w, h);
}

/* Draws whatever differs from what the window shows, and shows it. */
static void blocks_refresh(BlocksGame *b) {
    unsigned char want[BLOCKS_ROWS][BLOCKS_COLUMNS];
    blocks_compose(b, want);
    os_window_draw(b->win);
    Rect dirty = rect(0, 0, 0, 0);
    for (int y = 0; y < BLOCKS_ROWS; y++)
        for (int x = 0; x < BLOCKS_COLUMNS; x++) {
            if (want[y][x] == b->shown[y][x]) continue;
            Rect r = rect(BLOCKS_WELL_X + x * BLOCKS_CELL, BLOCKS_WELL_Y + y * BLOCKS_CELL, BLOCKS_CELL,
                          BLOCKS_CELL);
            blocks_draw_square(r.x, r.y, want[y][x]);
            b->shown[y][x] = want[y][x];
            dirty = rect_union(dirty, r);
        }
    if (!rect_empty(dirty)) os_window_update(b->win, dirty.x, dirty.y, dirty.w, dirty.h);
    int next = b->state == BLOCKS_READY ? -1 : b->next;
    if (next != b->shown_next) {
        blocks_draw_next(b);
        b->shown_next = next;
        os_window_update(b->win, BLOCKS_PANEL_X, BLOCKS_WELL_Y, BLOCKS_PANEL_W, 80);
    }
    const struct {
        long value, *shown;
    } values[2] = { { b->score, &b->shown_score }, { b->high, &b->shown_high } };
    for (int i = 0; i < 2; i++) {
        if (values[i].value == *values[i].shown) continue;
        int y = BLOCKS_WELL_Y + 90 + i * 46;
        blocks_draw_value(y, i ? "HIGH SCORE" : "SCORE", values[i].value);
        *values[i].shown = values[i].value;
        os_window_update(b->win, BLOCKS_PANEL_X, y, BLOCKS_PANEL_W, 38);
    }
    if (b->lines != b->shown_lines || b->level != b->shown_level) {
        int y = BLOCKS_WELL_Y + 182;
        blocks_draw_value(y, "LINES", b->lines);
        blocks_draw_value(y + 46, "LEVEL", b->level);
        b->shown_lines = b->lines;
        b->shown_level = b->level;
        os_window_update(b->win, BLOCKS_PANEL_X, y, BLOCKS_PANEL_W, 84);
    }
    b->changed = 0;
}

static void blocks_draw_all(BlocksGame *b) {
    static const char *const keys[5] = { "\x7f \x81  Move", "\x80    Turn", "\x82    Faster", "Space Drop",
                                         "P     Pause" };
    os_window_draw(b->win);
    fill_rect(0, 0, BLOCKS_W, BLOCKS_H, FACE);
    bevel_sunken(BLOCKS_WELL_X - 3, BLOCKS_WELL_Y - 3, BLOCKS_COLUMNS * BLOCKS_CELL + 6,
                 BLOCKS_ROWS * BLOCKS_CELL + 6);
    hline(BLOCKS_WELL_X - 1, BLOCKS_WELL_Y - 1, BLOCKS_COLUMNS * BLOCKS_CELL + 2, BLACK);
    for (int i = 0; i < 5; i++) draw_text(BLOCKS_PANEL_X + 2, BLOCKS_H - 12 - 13 * (5 - i) + 2, keys[i], SHADOW);
    memset(b->shown, BLOCKS_UNKNOWN, sizeof b->shown);
    b->shown_score = b->shown_high = -1;
    b->shown_lines = b->shown_level = b->shown_next = -2;
    blocks_refresh(b);
    blocks_draw_banner(b);
    os_window_update_all(b->win);
}

/* ------------------------------------------------------------------ the game */

static void blocks_check_menu(BlocksGame *b) {
    MenuItem *pause = &b->game_menu[BLOCKS_PAUSE_ITEM], *music = &b->game_menu[BLOCKS_MUSIC_ITEM];
    int can_pause = b->state == BLOCKS_PLAYING || b->state == BLOCKS_CLEARING || b->state == BLOCKS_PAUSED;
    pause->flags = (b->state == BLOCKS_PAUSED ? MENU_CHECKED : 0) | (can_pause ? 0 : MENU_DISABLED);
    music->flags = b->music ? MENU_CHECKED : 0;
}

/* Changes state: the timer runs only while the game moves, and the music follows. */
static void blocks_set_state(BlocksGame *b, int state) {
    b->state = state;
    int moving = state == BLOCKS_PLAYING || state == BLOCKS_CLEARING;
    os_set_timer(b->win, BLOCKS_TICK, moving ? BLOCKS_TICK_MS : 0);
    b->last_ms = os_now_ms();
    blocks_music_follow(b);
    blocks_check_menu(b);
}

static void blocks_game_over(BlocksGame *b) {
    blocks_set_state(b, BLOCKS_OVER);
    blocks_sound_over();
    b->new_high = b->score > b->high;
    if (b->new_high) {
        b->high = b->score;
        blocks_save(b);
    }
    blocks_refresh(b);
    blocks_draw_banner(b);
}

/* The next piece comes in at the top; answers 0 when it has no room, which ends the game. */
static int blocks_spawn(BlocksGame *b) {
    int cells[4][2], top = 4;
    b->piece = b->next;
    b->next = blocks_from_bag(b);
    b->rotation = 0;
    blocks_cells(b->piece, 0, cells);
    for (int k = 0; k < 4; k++) top = min_int(top, cells[k][1]);
    b->px = (BLOCKS_COLUMNS - BLOCKS_PIECES[b->piece].size) / 2;
    b->py = -top;
    b->fall_ms = b->lock_ms = b->lock_moves = 0;
    b->changed = 1;
    return blocks_fits(b, b->piece, b->rotation, b->px, b->py);
}

static void blocks_new_game(BlocksGame *b) {
    memset(b->well, 0, sizeof b->well);
    memset(b->full, 0, sizeof b->full);
    b->score = b->lines = 0;
    b->level = 1;
    b->new_high = 0;
    b->bag_left = 0;
    b->music_clock = 0;
    b->tune_index = b->bass_step = 0;
    blocks_music_stop(b);
    b->next = blocks_from_bag(b);
    blocks_spawn(b);
    blocks_set_state(b, BLOCKS_PLAYING);
    blocks_draw_all(b);
}

/* The piece becomes part of the stack; full rows start to flash, or the next piece comes. */
static void blocks_lock(BlocksGame *b) {
    int cells[4][2], above = 0, count = 0;
    blocks_cells(b->piece, b->rotation, cells);
    for (int k = 0; k < 4; k++) {
        int x = b->px + cells[k][0], y = b->py + cells[k][1];
        if (y < 0) above = 1;
        else b->well[y][x] = (unsigned char)(b->piece + 1);
    }
    for (int y = 0; y < BLOCKS_ROWS; y++) {
        int filled = 0;
        for (int x = 0; x < BLOCKS_COLUMNS; x++) filled += b->well[y][x] != 0;
        b->full[y] = filled == BLOCKS_COLUMNS;
        count += b->full[y];
    }
    b->changed = 1;
    if (count) {
        b->clear_ms = 0;
        blocks_sound_lines(count);
        blocks_set_state(b, BLOCKS_CLEARING);
    } else if (above || !blocks_spawn(b)) {
        blocks_game_over(b);
    }
}

/* The flashing rows go, the rows above come down, and the score counts them. */
static void blocks_remove_rows(BlocksGame *b) {
    int count = 0;
    for (int y = BLOCKS_ROWS - 1, to = BLOCKS_ROWS - 1; y >= 0; y--) {
        if (b->full[y]) {
            count++;
            continue;
        }
        if (to != y) memcpy(b->well[to], b->well[y], BLOCKS_COLUMNS);
        to--;
    }
    for (int y = 0; y < count; y++) memset(b->well[y], 0, BLOCKS_COLUMNS);
    memset(b->full, 0, sizeof b->full);
    b->score += (long)BLOCKS_LINE_SCORE[min_int(count, 4)] * b->level;
    b->lines += count;
    b->level = 1 + b->lines / 10;
    b->changed = 1;
    blocks_set_state(b, BLOCKS_PLAYING);
    if (!blocks_spawn(b)) blocks_game_over(b);
}

/* Moves the piece, or turns it with a step to the side or up if the wall or the stack is in
 * the way; answers whether it could. A piece resting on the stack waits again before it locks,
 * a few times at most. */
static int blocks_move(BlocksGame *b, int dx, int turn) {
    static const int kicks[][2] = { { 0, 0 }, { -1, 0 }, { 1, 0 }, { -2, 0 }, { 2, 0 }, { 0, -1 } };
    int rotation = (b->rotation + turn + 4) & 3, tries = turn ? 6 : 1;
    for (int i = 0; i < tries; i++) {
        int x = b->px + dx + kicks[i][0], y = b->py + kicks[i][1];
        if (abs(kicks[i][0]) == 2 && b->piece != 0) continue;
        if (!blocks_fits(b, b->piece, rotation, x, y)) continue;
        b->px = x;
        b->py = y;
        b->rotation = rotation;
        if (b->lock_ms && b->lock_moves++ < BLOCKS_LOCK_MOVES) b->lock_ms = 0;
        b->changed = 1;
        return 1;
    }
    return 0;
}

static void blocks_hard_drop(BlocksGame *b) {
    int landing = blocks_landing(b);
    b->score += 2 * (landing - b->py);
    b->py = landing;
    blocks_sound_drop();
    blocks_lock(b);
}

/* Gravity, the soft drop while Down is held, and the side arrows' slide. */
static void blocks_step(BlocksGame *b, int ms) {
    int focused = os_window_focused(b->win);
    if (b->slide) {
        if (!focused || !os_key_down(b->slide < 0 ? KEY_LEFT : KEY_RIGHT)) {
            b->slide = 0;
        } else {
            for (b->slide_ms += ms; b->slide_ms >= BLOCKS_SLIDE_DELAY; b->slide_ms -= BLOCKS_SLIDE_RATE)
                blocks_move(b, b->slide, 0);
        }
    }
    int soft = focused && os_key_down(KEY_DOWN);
    int interval = BLOCKS_SPEED[min_int(b->level, (int)(sizeof BLOCKS_SPEED / sizeof BLOCKS_SPEED[0])) - 1];
    if (soft) interval = min_int(interval, BLOCKS_SOFT_MS);
    b->fall_ms += ms;
    while (b->fall_ms >= interval && blocks_fits(b, b->piece, b->rotation, b->px, b->py + 1)) {
        b->fall_ms -= interval;
        b->py++;
        b->lock_ms = 0;
        b->score += soft;
        b->changed = 1;
    }
    if (blocks_fits(b, b->piece, b->rotation, b->px, b->py + 1)) return;
    b->fall_ms = 0;
    b->lock_ms += ms;
    if (b->lock_ms >= BLOCKS_LOCK_MS) blocks_lock(b);
}

/* Keys let go of while the window did not have the keyboard, whose release it never heard of. */
static void blocks_forget_released(BlocksGame *b) {
    for (int i = 0; i < (int)sizeof b->held; i++)
        for (int bit = 0; bit < 8 && b->held[i]; bit++)
            if (b->held[i] >> bit & 1 && !os_key_down(i * 8 + bit)) b->held[i] &= (unsigned char)~(1 << bit);
}

static void blocks_tick(BlocksGame *b) {
    long now = os_now_ms();
    int ms = clamp_int((int)(now - b->last_ms), 0, 100);
    b->last_ms = now;
    blocks_forget_released(b);
    blocks_music_run(b, ms);
    if (b->state == BLOCKS_CLEARING) {
        b->clear_ms += ms;
        b->changed = 1;
        if (b->clear_ms >= BLOCKS_FLASH_MS) blocks_remove_rows(b);
    } else if (b->state == BLOCKS_PLAYING) {
        blocks_step(b, ms);
    }
    if (b->changed) blocks_refresh(b);
}

static void blocks_pause(BlocksGame *b) {
    if (b->state == BLOCKS_PAUSED) {
        blocks_set_state(b, b->resume);
        blocks_refresh(b);
    } else if (b->state == BLOCKS_PLAYING || b->state == BLOCKS_CLEARING) {
        b->resume = b->state;
        b->slide = 0;
        blocks_set_state(b, BLOCKS_PAUSED);
        blocks_refresh(b);
        blocks_draw_banner(b);
    }
}

/* A key pressed. A key already held is the keyboard repeating it, which does nothing: the side
 * arrows slide by their own timing, and Down is followed while held. */
static void blocks_key(BlocksGame *b, const Message *m) {
    if (m->key < 0 || m->key > KEY_MAX) return;
    unsigned char *byte = &b->held[m->key / 8], bit = (unsigned char)(1 << m->key % 8);
    int repeat = *byte & bit;
    *byte |= bit;
    if (repeat) return;
    if (m->key == KEY_F2) {
        blocks_new_game(b);
        return;
    }
    if (m->key == KEY_P || m->key == KEY_PAUSE) {
        blocks_pause(b);
        return;
    }
    /* Space starts the first game, but not the next: it is the key hammered as a game ends */
    if ((b->state == BLOCKS_READY && m->key == KEY_SPACE) || (b->state != BLOCKS_PLAYING && m->key == KEY_ENTER)) {
        if (b->state == BLOCKS_READY || b->state == BLOCKS_OVER) blocks_new_game(b);
        return;
    }
    if (m->key == KEY_LEFT || m->key == KEY_RIGHT) {
        /* held through the flash of full rows, an arrow slides the next piece */
        b->slide = m->key == KEY_LEFT ? -1 : 1;
        b->slide_ms = 0;
    }
    if (b->state != BLOCKS_PLAYING) return;
    switch (m->key) {
    case KEY_LEFT: case KEY_RIGHT: blocks_move(b, b->slide, 0); break;
    case KEY_UP: blocks_move(b, 0, 1); break;
    case KEY_Z: blocks_move(b, 0, -1); break;
    case KEY_SPACE: blocks_hard_drop(b); break;
    default: return;
    }
    if (b->changed) blocks_refresh(b);
}

static void blocks_key_up(BlocksGame *b, const Message *m) {
    if (m->key < 0 || m->key > KEY_MAX) return;
    b->held[m->key / 8] &= (unsigned char)~(1 << m->key % 8);
    if ((m->key == KEY_LEFT && b->slide < 0) || (m->key == KEY_RIGHT && b->slide > 0)) {
        /* the other arrow, if still held, takes over */
        int other = m->key == KEY_LEFT ? KEY_RIGHT : KEY_LEFT;
        b->slide = b->held[other / 8] >> other % 8 & 1 ? -b->slide : 0;
        b->slide_ms = 0;
    }
}

static void blocks_quit(BlocksGame *b) {
    blocks_music_stop(b);
    blocks_controller(BLOCKS_EFFECT_CHANNEL, 123);
    os_exit(0);
}

static void blocks_command(BlocksGame *b, int id) {
    switch (id) {
    case BLOCKS_NEW: blocks_new_game(b); break;
    case BLOCKS_PAUSE: blocks_pause(b); break;
    case BLOCKS_MUSIC:
        b->music = !b->music;
        blocks_music_follow(b);
        blocks_check_menu(b);
        blocks_save(b);
        break;
    case BLOCKS_EXIT: blocks_quit(b); break;
    case BLOCKS_HOW:
        os_message_box(b->win, "How to Play",
                       "Fill rows from wall to wall: a full row goes, and every ten rows the pieces fall "
                       "faster.\n\n"
                       "Left and Right move the piece, Up turns it (Z the other way), Down drops it faster and "
                       "Space drops it at once.\n\nP pauses, F2 starts a new game.",
                       MB_OK, MB_INFO);
        break;
    case BLOCKS_ABOUT:
        os_message_box(b->win, "About Blocks",
                       "Blocks for asmOS.\nThe music is Korobeiniki, a Russian folk song.\n"
                       "The high score is kept in blocks.ini.",
                       MB_OK, MB_INFO);
        break;
    }
}

void blocks_main(const char *argument) {
    (void)argument;
    BlocksGame *b = os_alloc(sizeof *b);
    if (!b) os_exit(1);
    memcpy(b->game_menu, BLOCKS_GAME_MENU, sizeof b->game_menu);
    memcpy(b->help_menu, BLOCKS_HELP_MENU, sizeof b->help_menu);
    b->menus[0] = (Menu){ "Game", b->game_menu };
    b->menus[1] = (Menu){ "Help", b->help_menu };
    blocks_load(b);
    srand((unsigned)os_now_ms() * 2654435761u ^ (unsigned)os_getpid());
    b->win = os_window_create("Blocks", &ICON_BLOCKS, BLOCKS_W, BLOCKS_H, 0);
    if (!b->win) os_exit(1);
    os_window_set_menu(b->win, b->menus);
    b->level = 1;
    blocks_set_state(b, BLOCKS_READY);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_KEY: blocks_key(b, &m); break;
        case MSG_KEY_UP: blocks_key_up(b, &m); break;
        case MSG_TIMER: blocks_tick(b); break;
        case MSG_COMMAND: blocks_command(b, m.id); break;
        case MSG_RESIZE: blocks_draw_all(b); break;
        case MSG_BLUR:
            /* the game waits while another window has the keyboard */
            if (b->state == BLOCKS_PLAYING || b->state == BLOCKS_CLEARING) blocks_pause(b);
            memset(b->held, 0, sizeof b->held);
            break;
        case MSG_FOCUS: memset(b->held, 0, sizeof b->held); break;
        case MSG_CLOSE: case MSG_QUIT: blocks_quit(b); break;
        default: os_default_message(&m);
        }
    }
}
