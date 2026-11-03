/*
 * Pac-Man: the arcade game at the arcade's own 224 by 288 pixels, the C version of the RISC-V
 * example Project's Pac-Man. Its Files are in apps/pacman/, as they were there, and the game is
 * the same game, its names prefixed with pm_; what changed is what it stands on. On RISC-V it drew
 * on a memory-mapped Screen, read characters from the keyboard's registers, waited out each frame
 * with sim_sleep and played its sound on the editor's Synth. Here it draws into a picture that
 * pm_show copies into a window, MSG_KEY brings the keys, a timer runs the frames, and synth.c makes
 * the Synth's sound into samples for /dev/dsp. The pictures and the maze are Files of the Project,
 * in games/pacman/.
 *
 * The game keeps everything in variables of its Files, as it did on RISC-V, and every process of
 * this application would share them: so Pac-Man runs once, and a second start only says that it
 * is running already. A start after the last one ended puts the variables back as they began.
 */

#include "pacman/pacman.h"
#include "pacman/state.c"
#include "pacman/assets.c"
#include "pacman/video.c"
#include "pacman/maze.c"
#include "pacman/synth.c"
#include "pacman/sound.c"
#include "pacman/player.c"
#include "pacman/ghosts.c"
#include "pacman/game.c"

enum { PM_NEW_GAME = 1, PM_PAUSE, PM_SOUND, PM_EXIT, PM_KEYS };
enum { PM_TIMER = 1 };

/* The Sound item's check changes, in this array itself: there is only one Pac-Man. */
static MenuItem pm_game_menu[] = {
    { "New Game", PM_NEW_GAME, 0, "F2", NULL },
    { "Pause", PM_PAUSE, 0, "P", NULL },
    { "Sound", PM_SOUND, MENU_CHECKED, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", PM_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem pm_help_menu[] = {
    { "Keys...", PM_KEYS, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const Menu pm_menus[] = { { "Game", pm_game_menu }, { "Help", pm_help_menu }, { NULL, NULL } };

/* The Game menu's Sound item shows whether the sound is on. */
static void pm_check_sound(void) {
    pm_game_menu[2].flags = pm_sound_on ? MENU_CHECKED : 0;
}

static Window *pm_window;
static long pm_last_tick;     /* os_now_ms at the last tick */
static long pm_owed;          /* time not yet run as frames, in 1/60 ms: a frame is 1000 */
static long pm_pcm_tried;     /* when os_pcm_open was last tried */

/*
 * Every variable of the game as it begins, for a start after an earlier Pac-Man ended: the
 * variables keep what that one left in them, pointers to its freed pictures among them.
 */
static void pm_start_globals(void) {
    pm_display = pm_background = pm_maze_image = pm_sprite_sheet = pm_tile_sheet = NULL;
    pm_wall_pixels = NULL;
    pm_wall_pixel_count = 0;
    memset(pm_dirty, 0, sizeof pm_dirty);
    pm_dots_total = pm_energizer_count = 0;
    pm_game_state = PM_TITLE;
    pm_state_timer = 0;
    pm_frame_count = 0;
    pm_level = 1;
    pm_level_info = NULL;
    pm_score = pm_high_score = pm_lives = pm_extra_life_given = 0;
    pm_dots_left = pm_dots_eaten = pm_mode_index = pm_mode_timer = pm_fright_timer = 0;
    pm_ghosts_eaten = 0;
    pm_eaten_ghost = NULL;
    pm_global_mode = pm_global_dots = pm_dot_timer = 0;
    pm_elroy_enabled = 1;
    pm_fruit_timer = pm_fruit_score_timer = 0;
    pm_paused = pm_key_pressed = pm_pause_requested = 0;
    pm_show_actors = pm_ready_intro = 0;
    pm_energizers_lit = pm_oneup_lit = 1;
    pm_rng_state = 0x2545f491u;
    pac_x = pac_y = 0;
    pac_dir = pac_want = PM_LEFT;
    pac_acc = pac_stall = pac_anim = 0;
    memset(pm_ghosts, 0, sizeof pm_ghosts);
    memset(pm_slots, 0, sizeof pm_slots);
    memset(pm_voices, 0, sizeof pm_voices);
    pm_background_sound = NULL;
    pm_chomp_turn = 0;
    pm_sound_frame = 0;
    memset(pm_channels, 0, sizeof pm_channels);
    pm_sound_on = 1;
    pm_pcm_ready = pm_pcm_capacity = pm_sample_count = pm_sample_rest = pm_quiet_frames = 0;
    pm_samples_written = 0;
    pm_check_sound();
}

/* Whether a Pac-Man that started before this one is still running. */
static int pm_running_already(void) {
    ProcessInfo list[32];
    int count = min_int(os_processes(list, 32), 32), self = os_getpid();
    for (int i = 0; i < count; i++)
        if (list[i].pid < self && strcmp(list[i].name, APPS[APP_PACMAN].name) == 0) return 1;
    return 0;
}

/* /dev/dsp for the sound, unless another process has it: then it is tried again a second later. */
static void pm_pcm_try(void) {
    long now = os_now_ms();
    if (pm_pcm_ready || !pm_sound_on || (pm_pcm_tried && now - pm_pcm_tried < 1000)) return;
    pm_pcm_tried = now;
    if (os_pcm_open(PM_RATE) != 0) return;
    pm_pcm_ready = 1;
    pm_pcm_capacity = os_pcm_space();
    pm_samples_written = 0;
}

/*
 * One tick of the timer: as many frames as the time since the last one holds, 60 a second, but
 * at most PM_MOST_FRAMES: a slower machine slows the game rather than jump it. While another
 * window has the keyboard, as a message box of this one does, the game stands still.
 */
static void pm_tick(void) {
    long now = os_now_ms();
    pm_owed += (now - pm_last_tick) * 60;
    pm_last_tick = now;
    if (!os_window_focused(pm_window)) {
        pm_owed = 0;
        return;
    }
    pm_pcm_try();
    int frames = (int)(pm_owed / 1000);
    if (frames >= PM_MOST_FRAMES) {
        frames = PM_MOST_FRAMES;
        pm_owed = 0;
    } else {
        pm_owed -= frames * 1000L;
    }
    for (int i = 0; i < frames; i++) {
        pm_update_game();
        pm_render_sprites();
        pm_update_sounds();
    }
    pm_show(pm_window);
    pm_pcm_flush();
}

/* The window lost the keyboard: a game in play pauses, and waits for P. */
static void pm_blur(void) {
    if (pm_game_state != PM_PLAY || pm_paused) return;
    pm_set_paused(1);
    pm_render_sprites();
    pm_show(pm_window);
}

static void pm_quit(void) {
    os_pcm_close();
    os_exit(0);
}

static void pm_command(int id) {
    switch (id) {
    case PM_NEW_GAME:
        pm_key_pressed = 0;
        pm_start_game();
        break;
    case PM_PAUSE: pm_pause_requested = 1; break;
    case PM_SOUND:
        /* off, synth.c makes silence, and then nothing: /dev/dsp falls quiet but stays this game's */
        pm_sound_on = !pm_sound_on;
        pm_check_sound();
        pm_pcm_tried = 0;
        break;
    case PM_EXIT: pm_quit(); break;
    case PM_KEYS:
        pm_blur();
        os_message_box(pm_window, "Pac-Man keys",
                       "Move: the arrow keys, W A S D, Z Q S D, or 8 4 5 6 on the number pad.\n"
                       "Start: Space or Enter.\nPause: P, and P again to go on.\nNew game: F2.\n"
                       "The game pauses when its window loses the keyboard.",
                       MB_OK, MB_INFO);
        break;
    }
}

void pacman_main(const char *argument) {
    (void)argument;
    if (pm_running_already()) {
        os_message_box(NULL, "Pac-Man", "Pac-Man is running already: there can be only one game at a time.",
                       MB_OK, MB_INFO);
        os_exit(0);
    }
    pm_start_globals();
    pm_window = os_window_create("Pac-Man", &ICON_PACMAN, PM_GAME_W, PM_GAME_H, 0);
    if (!pm_window) os_exit(1);
    os_window_set_menu(pm_window, pm_menus);
    os_window_draw(pm_window);
    fill_rect(0, 0, PM_GAME_W, PM_GAME_H, BLACK);
    draw_text(PM_GAME_W / 2 - text_width("Loading...") / 2, PM_GAME_H / 2 - 6, "Loading...", WHITE);
    os_window_update_all(pm_window);
    if (!pm_load_assets(pm_window)) os_exit(1);
    pm_init_sounds();
    pm_rng_state ^= (uint32_t)os_now_ms(); /* a different game every time */
    if (!pm_rng_state) pm_rng_state = 1;
    pm_enter_title();
    pm_show(pm_window);
    pm_pcm_tried = 0;
    pm_pcm_try();
    pm_last_tick = os_now_ms();
    pm_owed = 0;
    os_set_timer(pm_window, PM_TIMER, 16);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_TIMER: pm_tick(); break;
        case MSG_KEY:
            if (m.mods & (MOD_CTRL | MOD_ALT)) break;
            if (m.key == KEY_F2) pm_command(PM_NEW_GAME);
            else pm_key(&m);
            break;
        case MSG_COMMAND: pm_command(m.id); break;
        case MSG_BLUR: pm_blur(); break;
        case MSG_CLOSE: case MSG_QUIT: pm_quit(); break;
        default: os_default_message(&m);
        }
    }
}
