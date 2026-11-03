/* Everything the game remembers, and the arcade's tables. */

/*
 * The pictures, in memory from os_alloc that pm_load_assets fills. The first three share the
 * window's layout, so y * PM_GAME_W + x is the same pixel in each.
 */
static uint32_t *pm_display;      /* what the window shows: the background with the sprites on top */
static uint32_t *pm_background;   /* maze, dots and text, without sprites */
static uint32_t *pm_maze_image;   /* maze.bmp, from row PM_MAZE_TOP */
static uint32_t *pm_sprite_sheet;
static uint32_t *pm_tile_sheet;
static int32_t *pm_wall_pixels;   /* where the maze picture is lit */
static int pm_wall_pixel_count;
static uint32_t pm_dirty[PM_GAME_H / 8];  /* pm_display's 8 by 8 tiles not yet shown, a bit a column */

/* The maze: maze.txt as read, and this level's copy, which loses its dots. */
static char pm_maze_layout[PM_MAZE_ROWS][PM_MAZE_COLS];
static char pm_tiles[PM_MAZE_ROWS][PM_MAZE_COLS];
static int pm_dots_total;
static int pm_energizers[8][2]; /* column and screen row */
static int pm_energizer_count;

/* The game. */
static int pm_game_state = PM_TITLE;
static int pm_state_timer;       /* frames spent in the current state */
static uint32_t pm_frame_count;
static int pm_level = 1;
static const struct PmLevel *pm_level_info;
static int pm_score, pm_high_score;
static int pm_lives;             /* spare lives, drawn at the bottom left */
static int pm_extra_life_given;
static int pm_dots_left, pm_dots_eaten;
static int pm_mode_index;        /* even: scatter, odd: chase */
static int pm_mode_timer;        /* frames left in this mode */
static int pm_fright_timer;
static int pm_ghosts_eaten;      /* since the last energizer */
static struct PmGhost *pm_eaten_ghost;
static int pm_global_mode;       /* 1 after a death: one dot counter for all */
static int pm_global_dots;
static int pm_dot_timer;         /* frames since Pac-Man last ate a dot */
static int pm_elroy_enabled = 1;
static int pm_fruit_timer, pm_fruit_score_timer;
static int pm_paused, pm_key_pressed, pm_pause_requested;
static int pm_show_actors;       /* Pac-Man and the ghosts are on the maze */
static int pm_ready_intro;       /* frames of PLAYER ONE before READY! */
static int pm_energizers_lit = 1, pm_oneup_lit = 1;
static uint32_t pm_rng_state = 0x2545f491u;

static int pac_x, pac_y, pac_dir = PM_LEFT;
static int pac_want = PM_LEFT;   /* the direction the player asked for */
static int pac_acc;
static int pac_stall;         /* frames to stand still after eating */
static int pac_anim;          /* pixels moved, for the mouth */

/* Blinky, Pinky, Inky, Clyde. */
static struct PmGhost pm_ghosts[4];
static struct PmSlot pm_slots[PM_SLOT_COUNT];

static const int pm_dir_dx[4] = { 1, -1, 0, 0 };
static const int pm_dir_dy[4] = { 0, 0, -1, 1 };
/* the arcade's tie-break order */
static const int pm_choice_order[4] = { PM_UP, PM_LEFT, PM_DOWN, PM_RIGHT };

/* Where each ghost starts: x, y, direction, state, home x, scatter corner, sprites.bmp row. */
static const struct PmGhost pm_ghost_start[4] = {
    { .x = 111, .y = 115, .dir = PM_LEFT, .state = PM_ACTIVE, .home_x = 111,
      .scatter_x = 25, .scatter_y = 0, .sheet_row = 2 },
    { .x = 111, .y = 139, .dir = PM_DOWN, .state = PM_IN_HOUSE, .home_x = 111,
      .scatter_x = 2, .scatter_y = 0, .sheet_row = 3 },
    { .x = 95, .y = 139, .dir = PM_UP, .state = PM_IN_HOUSE, .home_x = 95,
      .scatter_x = 27, .scatter_y = 35, .sheet_row = 4 },
    { .x = 127, .y = 139, .dir = PM_UP, .state = PM_IN_HOUSE, .home_x = 127,
      .scatter_x = 0, .scatter_y = 35, .sheet_row = 5 },
};

/* Scatter and chase durations in frames (60 a second), scatter first; the eighth lasts for ever. */
static const int pm_modes_level1[7] = { 420, 1200, 420, 1200, 300, 1200, 300 };
static const int pm_modes_level2[7] = { 420, 1200, 420, 1200, 300, 61980, 1 };
static const int pm_modes_level5[7] = { 300, 1200, 300, 1200, 300, 62220, 1 };

/*
 * The Pac-Man Dossier's table A.1, one row a level. Speeds are 1/256 pixel a frame, where 320 is
 * the arcade's 100%.
 *   fruit points  pac ghost tunnel  e1 e1spd e2 e2spd  fpac fghost frames flashes modes inky clyde
 */
static const struct PmLevel pm_level_table[PM_LEVEL_TABLE_LAST] = {
    { 0, 100,  256, 240, 128,  20, 256, 10, 272,  288, 160, 360, 5, pm_modes_level1, 30, 60 },
    { 1, 300,  288, 272, 144,  30, 288, 15, 304,  304, 176, 300, 5, pm_modes_level2, 0, 50 },
    { 2, 500,  288, 272, 144,  40, 288, 20, 304,  304, 176, 240, 5, pm_modes_level2, 0, 0 },
    { 2, 500,  288, 272, 144,  40, 288, 20, 304,  304, 176, 180, 5, pm_modes_level2, 0, 0 },
    { 3, 700,  320, 304, 160,  40, 320, 20, 336,  320, 192, 120, 5, pm_modes_level5, 0, 0 },
    { 3, 700,  320, 304, 160,  50, 320, 25, 336,  320, 192, 300, 5, pm_modes_level5, 0, 0 },
    { 4, 1000, 320, 304, 160,  50, 320, 25, 336,  320, 192, 120, 5, pm_modes_level5, 0, 0 },
    { 4, 1000, 320, 304, 160,  50, 320, 25, 336,  320, 192, 120, 5, pm_modes_level5, 0, 0 },
    { 5, 2000, 320, 304, 160,  60, 320, 30, 336,  320, 192, 60,  3, pm_modes_level5, 0, 0 },
    { 5, 2000, 320, 304, 160,  60, 320, 30, 336,  320, 192, 300, 5, pm_modes_level5, 0, 0 },
    { 6, 3000, 320, 304, 160,  60, 320, 30, 336,  320, 192, 120, 5, pm_modes_level5, 0, 0 },
    { 6, 3000, 320, 304, 160,  80, 320, 40, 336,  320, 192, 60,  3, pm_modes_level5, 0, 0 },
    { 7, 5000, 320, 304, 160,  80, 320, 40, 336,  320, 192, 60,  3, pm_modes_level5, 0, 0 },
    { 7, 5000, 320, 304, 160,  80, 320, 40, 336,  320, 192, 180, 5, pm_modes_level5, 0, 0 },
    { 7, 5000, 320, 304, 160, 100, 320, 50, 336,  320, 192, 60,  3, pm_modes_level5, 0, 0 },
    { 7, 5000, 320, 304, 160, 100, 320, 50, 336,  320, 192, 60,  3, pm_modes_level5, 0, 0 },
    { 7, 5000, 320, 304, 160, 100, 320, 50, 336,  320, 192, 0,   0, pm_modes_level5, 0, 0 },
    { 7, 5000, 320, 304, 160, 100, 320, 50, 336,  320, 192, 60,  3, pm_modes_level5, 0, 0 },
    { 7, 5000, 320, 304, 160, 120, 320, 60, 336,  320, 192, 0,   0, pm_modes_level5, 0, 0 },
    { 7, 5000, 320, 304, 160, 120, 320, 60, 336,  320, 192, 0,   0, pm_modes_level5, 0, 0 },
    { 7, 5000, 288, 304, 160, 120, 320, 60, 336,  320, 192, 0,   0, pm_modes_level5, 0, 0 },
};
