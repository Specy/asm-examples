/*
 * Loading the game's pictures and maze from the Project's Files, with os_read_file and
 * os_bmp_decode. Paths start at the Project's root: the game reads games/pacman/.
 *
 * The pictures are BMP files, which os_bmp_decode unpacks into words of 0x00RRGGBB, the window's
 * own format, so drawing them is a copy. Black is transparent in sprites.bmp, so a sprite needs no
 * mask.
 */

/* Says which File is wrong and how, in a message box; answers 0 for pm_load_assets to give up. */
static int pm_fail(Window *win, const char *name, const char *problem) {
    char text[200];
    snprintf(text, sizeof text, "%s: %s", name, problem);
    os_message_box(win, "Pac-Man", text, MB_OK, MB_WARNING);
    return 0;
}

/*
 * The BMP File `name` as a picture of exactly `width` by `height` pixels, in os_alloc'd memory, or
 * NULL after saying what is wrong with it.
 */
static uint32_t *pm_read_bmp(Window *win, const char *name, int width, int height) {
    long length;
    char *file = os_read_file(name, &length);
    if (!file) {
        pm_fail(win, name, "cannot open it. It has to be a File of this Project.");
        return NULL;
    }
    int w = 0, h = 0;
    uint32_t *picture = os_bmp_decode(file, length, &w, &h);
    os_free(file);
    if (!picture) {
        pm_fail(win, name, "save it as a 24 or 32 bit BMP, without compression.");
        return NULL;
    }
    if (w != width || h != height) {
        char problem[80];
        snprintf(problem, sizeof problem, "the picture has the wrong size. It must be %d by %d pixels.",
                 width, height);
        pm_fail(win, name, problem);
        os_free(picture);
        return NULL;
    }
    return picture;
}

static int pm_bad_line(Window *win, const char *name, int line) {
    char problem[80];
    snprintf(problem, sizeof problem, "every line must be 28 characters long; this one is not: line %d",
             line);
    return pm_fail(win, name, problem);
}

/*
 * maze.txt into pm_maze_layout: 31 lines of 28 characters. '#' is a wall, '.' a dot, 'o' an
 * energizer, ' ' an empty corridor and '-' the ghost house door; any other character is a wall
 * too. Blank lines and carriage returns are skipped. Answers 0 after saying what is wrong.
 */
static int pm_parse_maze(Window *win, const char *name, const char *text, long length) {
    int row = 0, column = 0, line = 1;
    pm_dots_total = 0;
    for (long i = 0; i < length; i++) {
        char c = text[i];
        if (c == '\r') continue;
        if (c == '\n') {
            if (column != 0 && column != PM_MAZE_COLS) return pm_bad_line(win, name, line);
            if (column != 0) row++; /* a blank line counts for nothing */
            column = 0;
            line++;
            continue;
        }
        if (row >= PM_MAZE_ROWS) return pm_fail(win, name, "the maze must be 31 lines of 28 characters.");
        if (column >= PM_MAZE_COLS) return pm_bad_line(win, name, line);
        pm_maze_layout[row][column++] = c;
        if (c == '.' || c == 'o') pm_dots_total++;
    }
    if (column != 0 && column != PM_MAZE_COLS) return pm_bad_line(win, name, line);
    if (column != 0) row++; /* the last line had no newline */
    if (row != PM_MAZE_ROWS) return pm_fail(win, name, "the maze must be 31 lines of 28 characters.");
    return 1;
}

/* Where every lit pixel of the maze picture is, so the end-of-level flash repaints only those. */
static void pm_collect_wall_pixels(void) {
    pm_wall_pixel_count = 0;
    for (int index = PM_MAZE_TOP * PM_GAME_W; index < (PM_MAZE_TOP + PM_MAZE_IMAGE_H) * PM_GAME_W; index++)
        if (pm_maze_image[index] && pm_wall_pixel_count < PM_MAX_WALL_PIXELS)
            pm_wall_pixels[pm_wall_pixel_count++] = index;
}

/*
 * Reads games/pacman/maze.txt, maze.bmp, sprites.bmp and tiles.bmp. A File that is missing or not
 * what the game expects is named in a message box, with what is wrong, and the answer is 0.
 */
static int pm_load_assets(Window *win) {
    static const char maze_txt[] = "games/pacman/maze.txt", maze_bmp[] = "games/pacman/maze.bmp";
    static const char sprites[] = "games/pacman/sprites.bmp", tiles_bmp[] = "games/pacman/tiles.bmp";
    long length;
    char *text = os_read_file(maze_txt, &length);
    if (!text) return pm_fail(win, maze_txt, "cannot open it. It has to be a File of this Project.");
    int parsed = pm_parse_maze(win, maze_txt, text, length);
    os_free(text);
    if (!parsed) return 0;
    pm_display = os_alloc(PM_GAME_W * PM_GAME_H * sizeof *pm_display);
    pm_background = os_alloc(PM_GAME_W * PM_GAME_H * sizeof *pm_background);
    pm_maze_image = os_alloc(PM_GAME_W * PM_GAME_H * sizeof *pm_maze_image);
    pm_wall_pixels = os_alloc(PM_MAX_WALL_PIXELS * sizeof *pm_wall_pixels);
    if (!pm_display || !pm_background || !pm_maze_image || !pm_wall_pixels)
        return pm_fail(win, "Pac-Man", "there is not enough memory.");
    uint32_t *maze = pm_read_bmp(win, maze_bmp, PM_MAZE_IMAGE_W, PM_MAZE_IMAGE_H);
    if (!maze) return 0;
    copy_rect32(pm_maze_image + PM_MAZE_TOP * PM_GAME_W, PM_GAME_W * 4, maze, PM_MAZE_IMAGE_W * 4,
                PM_MAZE_IMAGE_W, PM_MAZE_IMAGE_H);
    os_free(maze);
    pm_sprite_sheet = pm_read_bmp(win, sprites, PM_SHEET_W, PM_SHEET_H);
    if (!pm_sprite_sheet) return 0;
    pm_tile_sheet = pm_read_bmp(win, tiles_bmp, PM_TILES_W, PM_TILES_H);
    if (!pm_tile_sheet) return 0;
    pm_collect_wall_pixels();
    return 1;
}
