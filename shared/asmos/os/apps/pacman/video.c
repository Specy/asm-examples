/*
 * Drawing. The picture is kept twice: `pm_background` holds the maze, dots and text, and
 * `pm_display` shows the background with the sprites on top. Moving a sprite is then two copies:
 * the old place from the background, the new place from the sprite sheet.
 *
 * Whatever changes in pm_display marks its 8 by 8 tiles in `pm_dirty`, and pm_show copies only
 * those into the window, once a tick: a frame of play changes a few small rectangles around the
 * sprites, and the window manager repaints just those on the screen.
 */

/* The tile columns that `width` pixels from x cover, a bit each, as `pm_dirty` keeps them. */
static uint32_t pm_columns(int x, int width) {
    int left = max_int(x, 0), right = min_int(x + width, PM_GAME_W);
    return right <= left ? 0 : (2u << ((right - 1) >> 3)) - (1u << (left >> 3));
}

/* The tiles a rectangle of pm_display covers have changed. */
static void pm_touch(int x, int y, int width, int height) {
    uint32_t columns = pm_columns(x, width);
    int top = max_int(y, 0), bottom = min_int(y + height, PM_GAME_H);
    if (!columns || bottom <= top) return;
    for (int row = top >> 3; row <= (bottom - 1) >> 3; row++) pm_dirty[row] |= columns;
}

/* Whether a tile under the 16 by 16 square at (x, y) has changed since pm_show last ran. */
static int pm_touched(int x, int y) {
    uint32_t columns = pm_columns(x, 16);
    for (int row = max_int(y, 0) >> 3; columns && row <= (min_int(y + 16, PM_GAME_H) - 1) >> 3; row++)
        if (pm_dirty[row] & columns) return 1;
    return 0;
}

/*
 * The changed tiles into the window's picture, and onto the screen: each run of changed tiles in
 * a row, taken down the rows below that have the same run changed too, as one rectangle.
 */
static void pm_show(Window *win) {
    os_window_draw(win);
    for (int row = 0; row < PM_GAME_H / 8; row++) {
        while (pm_dirty[row]) {
            uint32_t bits = pm_dirty[row];
            int first = __builtin_ctz(bits), count = __builtin_ctz(~(bits >> first));
            uint32_t run = ((1u << count) - 1) << first;
            int last_row = row;
            while (last_row + 1 < PM_GAME_H / 8 && (pm_dirty[last_row + 1] & run) == run) {
                pm_dirty[last_row + 1] &= ~run;
                last_row++;
            }
            pm_dirty[row] &= ~run;
            int x = first * 8, y = row * 8, w = count * 8, h = (last_row - row + 1) * 8;
            blit(pm_display + y * PM_GAME_W + x, PM_GAME_W, x, y, w, h);
            os_window_update(win, x, y, w, h);
        }
    }
}

/*
 * One 8 by 8 tile into the background and the display, from cell `cell` of tiles.bmp: its lit
 * pixels in `tint`, or in their own colors when the tint is 0, and the maze picture under its black
 * ones.
 */
static void pm_draw_glyph(int column, int row, int cell, uint32_t tint) {
    const uint32_t *source = pm_tile_sheet + (cell / 16) * 8 * PM_TILES_W + (cell % 16) * 8;
    int first = row * 8 * PM_GAME_W + column * 8;
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) {
            int index = first + y * PM_GAME_W + x;
            uint32_t color = source[y * PM_TILES_W + x];
            if (!color) color = pm_maze_image[index];
            else if (tint) color = tint;
            pm_background[index] = color;
            pm_display[index] = color;
        }
    }
    pm_dirty[row] |= 1u << column;
}

/* The tile as the maze has it now: the maze picture, with the dot or energizer still on it. */
static void pm_redraw_tile(int column, int row) {
    int cell = 0; /* the space: only the maze picture */
    if (row >= PM_MAZE_FIRST_ROW && row <= PM_MAZE_LAST_ROW && column >= 0 && column < PM_MAZE_COLS) {
        char tile = pm_tiles[row - PM_MAZE_FIRST_ROW][column];
        if (tile == '.') cell = PM_TILE_DOT;
        else if (tile == 'o') cell = PM_TILE_ENERGIZER;
    }
    pm_draw_glyph(column, row, cell, 0);
}

static void pm_draw_text(const char *text, int column, int row, uint32_t color) {
    for (; *text; text++, column++) pm_draw_glyph(column, row, *text - 32, color);
}

/* Puts the maze back under some text. */
static void pm_clear_text(int column, int row, int length) {
    for (int i = 0; i < length; i++) pm_redraw_tile(column + i, row);
}

/* The value right-aligned in white, its last digit at `last_column`, leading tiles blank. */
static void pm_draw_number(int value, int last_column, int row, int width, int fewest_digits) {
    for (int drawn = 0; width > 0; width--, last_column--) {
        int cell = 0;
        if (value != 0 || drawn < fewest_digits) {
            cell = '0' - 32 + value % 10;
            value /= 10;
            drawn++;
        }
        pm_draw_glyph(last_column, row, cell, PM_COLOR_WHITE);
    }
}

/* The top-left word of a 16 by 16 cell of sprites.bmp. */
static const uint32_t *pm_sheet_cell(int column, int row) {
    return pm_sprite_sheet + row * 16 * PM_SHEET_W + column * 16;
}

/* The columns of a 16 pixel wide sprite at x that are inside the game: it may be in the tunnel. */
static int pm_first_visible_column(int x) {
    return x < 0 ? -x : 0;
}
static int pm_end_visible_column(int x) {
    return PM_GAME_W - x < 16 ? PM_GAME_W - x : 16;
}

/*
 * The cell's non-black pixels onto the display, top-left corner at (x, y). A sprite wholly inside
 * the game, which is nearly every sprite, takes a loop of a constant 16 the compiler unrolls.
 */
static void pm_draw_sprite(int x, int y, const uint32_t *cell) {
    pm_touch(x, y, 16, 16);
    if (x >= 0 && x <= PM_GAME_W - 16) {
        uint32_t *target = pm_display + y * PM_GAME_W + x;
        for (int row = 0; row < 16; row++, cell += PM_SHEET_W, target += PM_GAME_W)
            for (int column = 0; column < 16; column++)
                if (cell[column]) target[column] = cell[column];
        return;
    }
    int first = pm_first_visible_column(x), last = pm_end_visible_column(x);
    for (int row = 0; row < 16; row++) {
        const uint32_t *source = cell + row * PM_SHEET_W;
        int line = (y + row) * PM_GAME_W + x;
        for (int column = first; column < last; column++)
            if (source[column]) pm_display[line + column] = source[column];
    }
}

/*
 * The background back over the sprite's 16 by 16 square, or the part of it inside the game. The
 * RISC-V version put back only the pixels the sprite covered, as every store to its Screen cost
 * far more than an instruction; here pm_display is ordinary memory, and copy_rect32 copies each
 * row with one instruction.
 */
static void pm_erase_sprite(int x, int y) {
    int first = pm_first_visible_column(x), last = pm_end_visible_column(x);
    if (last <= first) return;
    int index = y * PM_GAME_W + x + first;
    copy_rect32(pm_display + index, PM_GAME_W * 4, pm_background + index, PM_GAME_W * 4, last - first, 16);
    pm_touch(x, y, 16, 16);
}

/* A sprite drawn into the background as well, for the icons at the bottom that never move. */
static void pm_stamp_sprite(int x, int y, const uint32_t *cell) {
    for (int row = 0; row < 16; row++)
        for (int column = 0; column < 16; column++) {
            uint32_t color = cell[row * PM_SHEET_W + column];
            if (!color) continue;
            int index = (y + row) * PM_GAME_W + x + column;
            pm_background[index] = color;
            pm_display[index] = color;
        }
    pm_touch(x, y, 16, 16);
}

/* The maze picture back over a rectangle, in the background and on the display. */
static void pm_restore_rect(int x, int y, int width, int height) {
    int index = y * PM_GAME_W + x;
    copy_rect32(pm_background + index, PM_GAME_W * 4, pm_maze_image + index, PM_GAME_W * 4, width, height);
    copy_rect32(pm_display + index, PM_GAME_W * 4, pm_maze_image + index, PM_GAME_W * 4, width, height);
    pm_touch(x, y, width, height);
}

/* The whole game area from the maze picture, then the dots and energizers left in `pm_tiles`. */
static void pm_draw_maze(void) {
    pm_restore_rect(0, 0, PM_GAME_W, PM_GAME_H);
    for (int row = 0; row < PM_MAZE_ROWS; row++)
        for (int column = 0; column < PM_MAZE_COLS; column++)
            if (pm_tiles[row][column] == '.' || pm_tiles[row][column] == 'o')
                pm_redraw_tile(column, row + PM_MAZE_FIRST_ROW);
}

/* The maze's lines in white, or back in the maze picture's own colors. */
static void pm_flash_walls(int white) {
    for (int i = 0; i < pm_wall_pixel_count; i++) {
        int index = pm_wall_pixels[i];
        pm_background[index] = pm_display[index] = white ? PM_COLOR_FLASH : pm_maze_image[index];
    }
    pm_touch(0, PM_MAZE_TOP, PM_MAZE_IMAGE_W, PM_MAZE_IMAGE_H);
}

/* Shows a sprite this frame, centred on (centre_x, centre_y). */
static void pm_set_slot(int slot, const uint32_t *cell, int centre_x, int centre_y) {
    pm_slots[slot].visible = 1;
    pm_slots[slot].x = centre_x - 7;
    pm_slots[slot].y = centre_y - 7;
    pm_slots[slot].cell = cell;
}

static void pm_hide_slots(void) {
    for (int slot = 0; slot < PM_SLOT_COUNT; slot++) pm_slots[slot].visible = 0;
}

/* Whether two 16 by 16 squares overlap. */
static int pm_overlap(int x0, int y0, int x1, int y1) {
    return abs(x0 - x1) < 16 && abs(y0 - y1) < 16;
}

/*
 * Last frame's sprites off the display, then this frame's on, a later slot over an earlier one.
 * A sprite that neither moved nor changed is left as it is, unless something drawn this frame cut
 * into it: a tile under it, or another sprite erased or drawn over it.
 */
static void pm_render_sprites(void) {
    int changed[PM_SLOT_COUNT];
    for (int slot = 0; slot < PM_SLOT_COUNT; slot++) {
        const struct PmSlot *s = &pm_slots[slot];
        changed[slot] = s->visible != s->was_visible ||
                        (s->visible && (s->x != s->was_x || s->y != s->was_y || s->cell != s->was_cell)) ||
                        (s->was_visible && pm_touched(s->was_x, s->was_y));
    }
    for (int more = 1; more;) {
        more = 0;
        for (int a = 0; a < PM_SLOT_COUNT; a++)
            for (int b = 0; b < PM_SLOT_COUNT; b++) {
                const struct PmSlot *s = &pm_slots[a], *t = &pm_slots[b];
                if (!changed[a] || changed[b] || !t->was_visible) continue;
                if ((s->was_visible && pm_overlap(s->was_x, s->was_y, t->was_x, t->was_y)) ||
                    (s->visible && pm_overlap(s->x, s->y, t->was_x, t->was_y)))
                    changed[b] = more = 1;
            }
    }
    for (int slot = 0; slot < PM_SLOT_COUNT; slot++) {
        struct PmSlot *s = &pm_slots[slot];
        if (changed[slot] && s->was_visible) pm_erase_sprite(s->was_x, s->was_y);
    }
    for (int slot = 0; slot < PM_SLOT_COUNT; slot++) {
        struct PmSlot *s = &pm_slots[slot];
        s->was_visible = s->visible;
        if (!s->visible) continue;
        s->was_x = s->x;
        s->was_y = s->y;
        s->was_cell = s->cell;
        if (changed[slot]) pm_draw_sprite(s->x, s->y, s->cell);
    }
}
