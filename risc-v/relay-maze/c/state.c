/* Game state, picture buffers and difficulty settings. */

/* The pictures. All three share the Screen's layout, so y * GAME_W + x is the same pixel in each. */
static uint32_t background[GAME_W * GAME_H];   /* maze, dots and text, without sprites */
static uint32_t maze_image[GAME_W * GAME_H];   /* maze.bmp, from row MAZE_TOP */
static uint32_t sprite_sheet[SHEET_W * SHEET_H];
static uint32_t tile_sheet[TILES_W * TILES_H];
static int32_t wall_pixels[MAX_WALL_PIXELS];   /* where the maze picture is lit */
static int wall_pixel_count;

/* The maze: maze.txt as read, and this level's copy, which loses its dots. */
static char maze_layout[MAZE_ROWS][MAZE_COLS];
static char tiles[MAZE_ROWS][MAZE_COLS];
static int dots_total;
static int energizers[8][2]; /* column and screen row */
static int energizer_count;

/* The game. */
static int game_state = TITLE;
static int state_timer;       /* frames spent in the current state */
static uint32_t frame_count;
static int level = 1;
static const struct Level *level_info;
static int score, high_score;
static int lives;             /* spare lives, drawn at the bottom left */
static int extra_life_given;
static int dots_left, dots_eaten;
static int mode_index;        /* even: scatter, odd: chase */
static int mode_timer;        /* frames left in this mode */
static int fright_timer;
static int enemies_eaten;      /* since the last energizer */
static struct Enemy *eaten_enemy;
static int global_mode;       /* 1 after a death: one dot counter for all */
static int global_dots;
static int dot_timer;         /* frames since the player last ate a dot */
static int pursuit_enabled = 1;
static int bonus_timer, bonus_score_timer;
static int paused, key_pressed, pause_requested;
static int show_actors;       /* the player and the enemies are on the maze */
static int ready_intro;       /* frames of PLAYER ONE before READY! */
static int energizers_lit = 1, oneup_lit = 1;
static uint32_t rng_state = 0x2545f491u;
static uint32_t next_deadline;
static int deadline_thirds;

static int player_x, player_y, player_dir = LEFT;
static int player_want = LEFT;   /* the direction the player asked for */
static int player_acc;
static int player_stall;         /* frames to stand still after eating */
static int player_moving;
static int player_anim;          /* pixels moved, for the animation */

/* Chaser, Ambusher, Scout, Roamer. */
static struct Enemy enemies[4];
static struct Slot slots[SLOT_COUNT];

static const int dir_dx[4] = { 1, -1, 0, 0 };
static const int dir_dy[4] = { 0, 0, -1, 1 };
static const int choice_order[4] = { UP, LEFT, DOWN, RIGHT }; /* direction order used when distances tie */

/* Where each enemy starts: x, y, direction, state, home x, scatter corner, sprites.bmp row. */
static const struct Enemy enemy_start[4] = {
    { .x = 111, .y = 115, .dir = LEFT, .state = ACTIVE, .home_x = 111,
      .scatter_x = 25, .scatter_y = 0, .sheet_row = 2 },
    { .x = 111, .y = 139, .dir = DOWN, .state = IN_HOUSE, .home_x = 111,
      .scatter_x = 2, .scatter_y = 0, .sheet_row = 3 },
    { .x = 95, .y = 139, .dir = UP, .state = IN_HOUSE, .home_x = 95,
      .scatter_x = 27, .scatter_y = 35, .sheet_row = 4 },
    { .x = 127, .y = 139, .dir = UP, .state = IN_HOUSE, .home_x = 127,
      .scatter_x = 0, .scatter_y = 35, .sheet_row = 5 },
};

/* Original difficulty settings. Speeds are in 1/256 pixel per frame. */
static const int modes_intro[7] = { 360, 1050, 300, 1200, 240, 1500, 180 };
static const int modes_regular[7] = { 300, 1200, 240, 1500, 180, 1800, 120 };
static const int modes_fast[7] = { 240, 1500, 180, 1800, 120, 2100, 60 };

static const struct Level level_table[LEVEL_TABLE_LAST] = {
    { 0, 150, 256, 232, 120, 36, 248, 14, 264, 288, 144, 420, 5, modes_intro, 24, 48 },
    { 1, 350, 272, 248, 128, 40, 264, 18, 280, 304, 152, 360, 5, modes_regular, 16, 40 },
    { 2, 550, 288, 264, 136, 44, 280, 20, 296, 320, 160, 300, 5, modes_regular, 8, 32 },
    { 3, 850, 304, 280, 144, 48, 296, 22, 312, 328, 168, 240, 4, modes_fast, 0, 24 },
    { 4, 1250, 312, 288, 152, 52, 304, 24, 320, 336, 176, 210, 4, modes_fast, 0, 16 },
    { 5, 1750, 320, 296, 160, 56, 312, 26, 328, 344, 184, 180, 3, modes_fast, 0, 8 },
};
