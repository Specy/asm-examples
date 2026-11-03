/*
 * PAC-MAN in C: what every part of the game shares. apps/pacman.c includes this header and then
 * each of the other .c Files of this folder, so the game is compiled as one unit with the other
 * applications; the prototypes at the end let a part call a function that a later part defines.
 * Every name the game declares starts with pm_ or PM_ (Pac-Man's own pac_ and PAC_ excepted), to
 * keep it apart from the other applications' names in that unit.
 */
#ifndef PM_PACMAN_H
#define PM_PACMAN_H

/* The game's picture: the arcade's own 224 by 288 pixels, a word each. */
#define PM_GAME_W 224
#define PM_GAME_H 288

/* The most frames one tick of the timer runs, in apps/pacman.c: a slower machine slows the game. */
#define PM_MOST_FRAMES 4

/* The maze: 28 by 31 tiles of 8 by 8 pixels, starting 3 tile rows down, under the score. */
#define PM_MAZE_COLS 28
#define PM_MAZE_ROWS 31
#define PM_MAZE_FIRST_ROW 3
#define PM_MAZE_LAST_ROW 33
#define PM_TUNNEL_ROW 17
#define PM_MAZE_IMAGE_W 224
#define PM_MAZE_IMAGE_H 248
#define PM_MAZE_TOP 24

/* The pictures: sprites.bmp has 16 by 9 cells of 16 by 16, tiles.bmp 16 by 5 cells of 8 by 8. */
#define PM_SHEET_W 256
#define PM_SHEET_H 144
#define PM_TILES_W 128
#define PM_TILES_H 40
#define PM_TILE_DOT 64
#define PM_TILE_ENERGIZER 65
#define PM_MAX_WALL_PIXELS 32768

#define PM_COLOR_WHITE 0xffffffu
#define PM_COLOR_YELLOW 0xffff00u
#define PM_COLOR_RED 0xff0000u
#define PM_COLOR_CYAN 0x00ffffu
#define PM_COLOR_ORANGE 0xffb852u
#define PM_COLOR_FLASH 0xdedeffu

/* Opposite directions differ in bit 0 only, so d ^ 1 turns one around. */
enum PmDirection { PM_RIGHT, PM_LEFT, PM_UP, PM_DOWN };

enum PmGameState {
    PM_TITLE,      /* the maze and PUSH SPACE: any key starts a game */
    PM_READY,      /* READY! for two seconds, after PLAYER ONE at the start of a game */
    PM_PLAY,       /* the game itself */
    PM_EAT_GHOST,  /* a second's pause showing a ghost's points */
    PM_DYING,      /* a second's pause, then Pac-Man shrivels away and pops */
    PM_LEVEL_DONE, /* a second's pause, then the maze flashes */
    PM_GAME_OVER   /* GAME OVER for five seconds, or until a key */
};

enum PmGhostState {
    PM_IN_HOUSE, /* waiting in the ghost house, bobbing */
    PM_LEAVING,  /* on its way out through the door */
    PM_ACTIVE,   /* in the maze: chasing, scattering or frightened */
    PM_EYES,     /* eaten: only the eyes, heading home */
    PM_ENTERING  /* the eyes going down into the house */
};

/* The ghost house and the starting places, in pixels: a tile's centre is 8 * tile + 3. */
#define PM_HOUSE_DOOR_X 111
#define PM_HOUSE_DOOR_Y 115
#define PM_HOUSE_MID_Y 139
#define PM_HOUSE_TOP_Y 135
#define PM_HOUSE_BOTTOM_Y 143
#define PAC_START_X 111
#define PAC_START_Y 211
#define PM_FRUIT_X 111
#define PM_FRUIT_Y 163

/* Speeds are in 1/256 pixel a frame: 320, 1.25 pixels, is the arcade's 100%. */
#define PM_EYES_SPEED 384
#define PM_HOUSE_SPEED 128
#define PM_LEAVING_SPEED 160

struct PmGhost {
    int x, y;             /* centre pixel */
    int dir;              /* enum PmDirection */
    int state;            /* enum PmGhostState */
    int fright;           /* 1 while blue */
    int acc;              /* speed accumulator, 1/256 pixel */
    int home_x;           /* where it waits in the house */
    int scatter_x;        /* its corner, in tiles */
    int scatter_y;
    int dots;             /* its own dot counter */
    int sheet_row;        /* its row in sprites.bmp */
};

/* One sprite on the picture: what to draw this frame, and what was drawn last frame. */
struct PmSlot {
    int visible, x, y;
    const uint32_t *cell;
    int was_visible, was_x, was_y;
    const uint32_t *was_cell;
};
#define PM_SLOT_COUNT 6
#define PM_SLOT_FRUIT 0
#define PM_SLOT_PACMAN 1

/* One level of The Pac-Man Dossier's table A.1. */
struct PmLevel {
    int fruit, fruit_points;
    int pac_speed, ghost_speed, tunnel_speed;
    int elroy1_dots, elroy1_speed, elroy2_dots, elroy2_speed;
    int fright_pac_speed, fright_ghost_speed, fright_frames, flashes;
    const int *modes; /* scatter and chase durations in frames, scatter first */
    int inky_dots, clyde_dots;
};
#define PM_LEVEL_TABLE_LAST 21

/* The sound: three channels play at once, as the arcade's three voices did (sound.c), on the
 * small wavetable synthesizer of synth.c. */
enum PmSoundChannel {
    PM_CHANNEL_1, /* the start music's bass, the extra life */
    PM_CHANNEL_2, /* the start music's tune, the siren, the frightened ghosts, the eyes going home */
    PM_CHANNEL_3, /* the dots, the fruit, a ghost eaten, the death */
    PM_CHANNEL_COUNT
};

/* assets.c */
static int pm_load_assets(Window *win);

/* video.c */
static void pm_draw_glyph(int column, int row, int cell, uint32_t tint);
static void pm_redraw_tile(int column, int row);
static void pm_draw_text(const char *text, int column, int row, uint32_t color);
static void pm_clear_text(int column, int row, int length);
static void pm_draw_number(int value, int last_column, int row, int width, int fewest_digits);
static const uint32_t *pm_sheet_cell(int column, int row);
static void pm_stamp_sprite(int x, int y, const uint32_t *cell);
static void pm_restore_rect(int x, int y, int width, int height);
static void pm_draw_maze(void);
static void pm_flash_walls(int white);
static void pm_set_slot(int slot, const uint32_t *cell, int centre_x, int centre_y);
static void pm_hide_slots(void);
static void pm_render_sprites(void);
static void pm_show(Window *win);

/* maze.c */
static int pm_tile_at(int column, int row);
static int pm_is_open(int column, int row);
static void pm_reset_maze(void);
static void pm_draw_energizers(int lit);
static void pm_eat_tile(int column, int row);

/* player.c */
static void pm_reset_pacman(void);
static void pm_update_pacman(void);
static const uint32_t *pm_pacman_frame(void);

/* ghosts.c */
static void pm_reset_ghosts(void);
static void pm_clear_ghost_counters(void);
static struct PmGhost *pm_waiting_ghost(int *number);
static void pm_release_ghosts(void);
static void pm_update_ghosts(void);
static void pm_energize(void);
static void pm_update_timers(void);
static void pm_check_collisions(void);
static const uint32_t *pm_ghost_frame(const struct PmGhost *ghost);
static uint32_t pm_random_number(void);

/* game.c */
static void pm_enter_state(int state);
static void pm_enter_title(void);
static void pm_start_game(void);
static void pm_update_game(void);
static void pm_add_score(int points);
static void pm_key(const Message *message);

/* synth.c */
static void pm_synth_render(void);

/* sound.c */
static void pm_init_sounds(void);
static void pm_play_sound(int channel, const uint8_t *effect);
static void pm_play_song(int channel, const uint8_t *song);
static void pm_stop_sounds(void);
static void pm_play_chomp(void);
static void pm_update_sounds(void);

#endif
