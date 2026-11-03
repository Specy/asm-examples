/*
 * The maze as the game plays it: `pm_tiles` is this level's copy of maze.txt, which loses its dots
 * as Pac-Man eats them. Positions are screen tiles: the maze's row 0 is tile row 3, under the
 * score.
 */

/*
 * The maze's character at a tile. Above and below the maze is wall; left and right of it is wall
 * too, except on the tunnel row, which goes on for ever.
 */
static int pm_tile_at(int column, int row) {
    if (row < PM_MAZE_FIRST_ROW || row > PM_MAZE_LAST_ROW) return '#';
    if (column < 0 || column >= PM_MAZE_COLS) return row == PM_TUNNEL_ROW ? ' ' : '#';
    return pm_tiles[row - PM_MAZE_FIRST_ROW][column];
}

/*
 * Whether Pac-Man or a ghost in the maze may walk onto a tile. The ghost house door is not open:
 * only eyes and ghosts leaving go through it, on their own paths.
 */
static int pm_is_open(int column, int row) {
    int tile = pm_tile_at(column, row);
    return tile == ' ' || tile == '.' || tile == 'o';
}

/* A full maze for a new level, and where its energizers are, for their blinking. */
static void pm_reset_maze(void) {
    pm_energizer_count = 0;
    for (int row = 0; row < PM_MAZE_ROWS; row++)
        for (int column = 0; column < PM_MAZE_COLS; column++) {
            char tile = pm_tiles[row][column] = pm_maze_layout[row][column];
            if (tile == 'o' && pm_energizer_count < 8) {
                pm_energizers[pm_energizer_count][0] = column;
                pm_energizers[pm_energizer_count][1] = row + PM_MAZE_FIRST_ROW;
                pm_energizer_count++;
            }
        }
    pm_dots_left = pm_dots_total;
    pm_dots_eaten = 0;
}

/* The energizers still in the maze, lit or dark, for their blinking. */
static void pm_draw_energizers(int lit) {
    for (int i = 0; i < pm_energizer_count; i++) {
        int column = pm_energizers[i][0], row = pm_energizers[i][1];
        if (pm_tile_at(column, row) != 'o') continue; /* eaten already */
        if (lit) pm_redraw_tile(column, row);
        else pm_draw_glyph(column, row, 0, 0); /* the maze picture alone */
    }
}

/* The dot or energizer at a tile is gone. */
static void pm_eat_tile(int column, int row) {
    pm_tiles[row - PM_MAZE_FIRST_ROW][column] = ' ';
    pm_redraw_tile(column, row);
}
