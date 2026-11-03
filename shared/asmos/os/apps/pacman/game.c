/*
 * The game's states, from the title to the game over, and everything drawn around the maze: the
 * score, the lives, the fruit. Also the keyboard.
 */

static void pm_draw_hud(void);
static void pm_draw_lives(void);
static void pm_begin_life(int intro);

static void pm_enter_state(int state) {
    pm_game_state = state;
    pm_state_timer = 0;
}

/* A full maze, the scores of the last game, and PUSH SPACE. */
static void pm_enter_title(void) {
    pm_level = 0; /* no lives and no fruit at the bottom */
    pm_lives = 0;
    pm_reset_maze();
    pm_draw_maze();
    pm_draw_hud();
    pm_draw_text("PUSH SPACE", 9, 14, PM_COLOR_ORANGE);
    pm_key_pressed = 0;
    pm_enter_state(PM_TITLE);
}

/* A full maze, and the level's settings. `intro` shows PLAYER ONE first. */
static void pm_start_level(int intro) {
    pm_level_info = &pm_level_table[(pm_level < PM_LEVEL_TABLE_LAST ? pm_level : PM_LEVEL_TABLE_LAST) - 1];
    pm_reset_maze();
    pm_clear_ghost_counters();
    pm_global_mode = 0;
    pm_elroy_enabled = 1;
    pm_fruit_timer = pm_fruit_score_timer = 0;
    pm_draw_maze();
    pm_draw_hud();
    pm_begin_life(intro);
}

/* Three Pac-Men, one playing and two spare, from level 1. */
static void pm_start_game(void) {
    pm_score = 0;
    pm_extra_life_given = 0;
    pm_lives = 2;
    pm_level = 1;
    pm_start_level(1);
}

/*
 * Everyone back at the start, and READY!, after PLAYER ONE when `intro` says so: then the start
 * music plays, and the two last as long as it does.
 */
static void pm_begin_life(int intro) {
    pm_stop_sounds();
    pm_reset_pacman();
    pm_reset_ghosts();
    pm_mode_index = 0;
    pm_mode_timer = pm_level_info->modes[0];
    pm_fright_timer = pm_ghosts_eaten = pm_dot_timer = pm_paused = 0;
    pm_energizers_lit = 1;
    pm_draw_energizers(1);
    pm_draw_text("READY!", 11, 20, PM_COLOR_YELLOW);
    pm_show_actors = !intro; /* no one on the maze until PLAYER ONE goes */
    pm_ready_intro = intro ? 136 : 0;
    if (intro) {
        pm_draw_text("PLAYER ONE", 9, 14, PM_COLOR_CYAN);
        pm_play_song(PM_CHANNEL_1, pm_music_bass);
        pm_play_song(PM_CHANNEL_2, pm_music_melody);
    }
    pm_enter_state(PM_READY);
}

static void pm_ready_update(void) {
    pm_state_timer++;
    if (pm_ready_intro && pm_state_timer == pm_ready_intro) {
        pm_clear_text(9, 14, 10); /* PLAYER ONE goes and the actors come */
        pm_show_actors = 1;
    } else if (pm_state_timer >= pm_ready_intro + 120) {
        pm_clear_text(11, 20, 6);
        pm_enter_state(PM_PLAY);
    }
}

/* Pauses the game, or lets it go on, with PAUSED where READY! was while it stands still. */
static void pm_set_paused(int paused) {
    pm_paused = paused;
    if (pm_paused) pm_draw_text("PAUSED", 11, 20, PM_COLOR_YELLOW);
    else pm_clear_text(11, 20, 6);
}

static void pm_play_update(void) {
    if (pm_pause_requested) {
        pm_pause_requested = 0;
        pm_set_paused(!pm_paused);
    }
    if (pm_paused) return;
    pm_update_pacman();
    if (pm_game_state != PM_PLAY) return;
    pm_check_collisions();
    if (pm_game_state != PM_PLAY) return;
    pm_update_ghosts();
    pm_check_collisions();
    if (pm_game_state != PM_PLAY) return;
    pm_release_ghosts();
    pm_update_timers();
    if (pm_frame_count % 10 == 0) pm_draw_energizers(pm_energizers_lit = !pm_energizers_lit);
}

/*
 * The arcade's death: everything still for a second, Pac-Man alone for half a second, then nine
 * frames of shrivelling to the falling wobble, and the pop as he vanishes.
 */
static void pm_dying_update(void) {
    int t = ++pm_state_timer;
    if (t == 90) pm_play_sound(PM_CHANNEL_3, pm_sound_death);
    if (t == 225) pm_play_sound(PM_CHANNEL_3, pm_sound_pop);
    if (t < 300) return;
    if (!pm_lives) {
        pm_draw_text("GAME  OVER", 9, 20, PM_COLOR_RED);
        pm_enter_state(PM_GAME_OVER);
        return;
    }
    pm_lives--;
    pm_draw_lives();
    pm_global_mode = 1; /* the house now runs on one shared counter */
    pm_global_dots = 0;
    pm_elroy_enabled = 0; /* until Clyde is out again */
    pm_fruit_timer = pm_fruit_score_timer = 0;
    pm_begin_life(0);
}

static void pm_level_done_update(void) {
    int t = ++pm_state_timer;
    if (t < 60) return;
    if (t < 156) {
        /* every 12 frames, white then blue, four times */
        if ((t - 60) % 12 == 0) pm_flash_walls(((t - 60) / 12 & 1) == 0);
        return;
    }
    pm_flash_walls(0);
    pm_level++;
    pm_start_level(0);
}

static void pm_game_over_update(void) {
    int t = ++pm_state_timer;
    if (t < 60) {
        pm_key_pressed = 0; /* keys from the last second of play do not count */
        return;
    }
    if (pm_key_pressed || t >= 300) pm_enter_title();
}

/* The score, the extra life at 10,000 and the high score. */
static void pm_add_score(int points) {
    pm_score += points;
    if (!pm_extra_life_given && pm_score >= 10000) {
        pm_extra_life_given = 1;
        pm_lives++;
        pm_draw_lives();
        pm_play_sound(PM_CHANNEL_1, pm_sound_extra_life);
    }
    if (pm_score > pm_high_score) {
        pm_high_score = pm_score;
        pm_draw_number(pm_high_score, 16, 1, 7, 2);
    }
    pm_draw_number(pm_score, 6, 1, 7, 2);
}

/* A Pac-Man for each spare life, five at most, at the bottom left. */
static void pm_draw_lives(void) {
    pm_restore_rect(0, 272, 112, 16);
    for (int i = 0; i < pm_lives && i < 5; i++) pm_stamp_sprite(16 + 16 * i, 272, pm_sheet_cell(3, 0));
}

/* The fruit of this level and up to six before it, newest on the right. */
static void pm_draw_level_fruits(void) {
    pm_restore_rect(112, 272, 112, 16);
    for (int i = 0; i < 7 && pm_level - i > 0; i++) {
        int number = pm_level - i < PM_LEVEL_TABLE_LAST ? pm_level - i : PM_LEVEL_TABLE_LAST;
        pm_stamp_sprite(192 - 16 * i, 272, pm_sheet_cell(pm_level_table[number - 1].fruit, 7));
    }
}

/* The two score rows at the top and the icons at the bottom. */
static void pm_draw_hud(void) {
    pm_draw_text("1UP", 3, 0, PM_COLOR_WHITE);
    pm_oneup_lit = 1;
    pm_draw_text("HIGH SCORE", 9, 0, PM_COLOR_WHITE);
    pm_draw_number(pm_score, 6, 1, 7, 2);
    pm_draw_number(pm_high_score, 16, 1, 7, pm_high_score ? 2 : 0); /* blank until there is one */
    pm_draw_lives();
    pm_draw_level_fruits();
}

/* 1UP blinks while a game is on and stays lit otherwise. */
static void pm_blink_1up(void) {
    int lit = pm_game_state == PM_TITLE || pm_game_state == PM_GAME_OVER || !((pm_frame_count >> 4) & 1);
    if (lit == pm_oneup_lit) return;
    pm_oneup_lit = lit;
    if (lit) pm_draw_text("1UP", 3, 0, PM_COLOR_WHITE);
    else pm_clear_text(3, 0, 3);
}

/* Which sprites this frame shows, and where. */
static void pm_compose_sprites(void) {
    pm_hide_slots();
    if (pm_game_state == PM_TITLE || pm_game_state == PM_GAME_OVER) return;
    int fruit = 0;
    int pacman = 0; /* 0 none, 1 round, 2 moving, 3 dying */
    int ghosts_shown = 0;
    switch (pm_game_state) {
    case PM_READY:
        pacman = ghosts_shown = pm_show_actors;
        break;
    case PM_PLAY:
        fruit = ghosts_shown = 1;
        pacman = 2;
        break;
    case PM_EAT_GHOST: /* Pac-Man is hidden while the points show */
        fruit = ghosts_shown = 1;
        break;
    case PM_DYING: /* a second still, then Pac-Man alone */
        fruit = ghosts_shown = pm_state_timer < 60;
        pacman = pm_state_timer < 60 ? 2 : 3;
        break;
    case PM_LEVEL_DONE: /* round, the ghosts gone after a second */
        pacman = 1;
        ghosts_shown = pm_state_timer < 60;
        break;
    }
    if (fruit && pm_fruit_timer)
        pm_set_slot(PM_SLOT_FRUIT, pm_sheet_cell(pm_level_info->fruit, 7), PM_FRUIT_X, PM_FRUIT_Y);
    else if (fruit && pm_fruit_score_timer) /* or its points */
        pm_set_slot(PM_SLOT_FRUIT, pm_sheet_cell(4 + pm_level_info->fruit, 8), PM_FRUIT_X, PM_FRUIT_Y);
    const uint32_t *pacman_cell = 0;
    if (pacman == 1) pacman_cell = pm_sheet_cell(0, 0);
    else if (pacman == 2) pacman_cell = pm_pacman_frame();
    else if (pacman == 3 && pm_state_timer < 255) { /* the first, nine of 15 frames, then the pop */
        int cell = pm_state_timer < 90 ? 0 : pm_state_timer < 225 ? 1 + (pm_state_timer - 90) / 15 : 10;
        pacman_cell = pm_sheet_cell(cell, 1);
    }
    if (pacman_cell) pm_set_slot(PM_SLOT_PACMAN, pacman_cell, pac_x, pac_y);
    if (!ghosts_shown) return;
    for (int number = 0; number < 4; number++) {
        const struct PmGhost *ghost = &pm_ghosts[number];
        const uint32_t *cell = pm_game_state == PM_EAT_GHOST && ghost == pm_eaten_ghost
                                   ? pm_sheet_cell(pm_ghosts_eaten - 1, 8) /* its points */
                                   : pm_ghost_frame(ghost);
        pm_set_slot(5 - number, cell, ghost->x, ghost->y); /* Blinky last, on top */
    }
}

/* One frame of whichever state the game is in, then the sprites it shows. */
static void pm_update_game(void) {
    if (!pm_paused) pm_frame_count++; /* a paused game stands still, skirts and all */
    if (pm_game_state != PM_PLAY) pm_pause_requested = 0; /* P only means something during play */
    switch (pm_game_state) {
    case PM_TITLE:
        if (pm_key_pressed) {
            pm_key_pressed = 0;
            pm_start_game();
        }
        break;
    case PM_READY:
        pm_ready_update();
        break;
    case PM_PLAY:
        pm_play_update();
        break;
    case PM_EAT_GHOST:
        if (++pm_state_timer >= 60) pm_enter_state(PM_PLAY);
        break;
    case PM_DYING:
        pm_dying_update();
        break;
    case PM_LEVEL_DONE:
        pm_level_done_update();
        break;
    case PM_GAME_OVER:
        pm_game_over_update();
        break;
    }
    pm_blink_1up();
    pm_compose_sprites();
}

/*
 * A key pressed, as MSG_KEY brings it between frames. The last direction key wins: the arrows,
 * W A S D, Z Q S D, or 8 4 5 6 (and 2) on the number pad. P pauses. Any key that types a
 * character, Space and Enter among them, or an arrow, is the key the title and the game over wait
 * for.
 */
static void pm_key(const Message *message) {
    int key = message->ch;
    switch (message->key) {
    case KEY_UP: key = 'w'; break;
    case KEY_LEFT: key = 'a'; break;
    case KEY_DOWN: key = 's'; break;
    case KEY_RIGHT: key = 'd'; break;
    }
    if (!key) return;
    pm_key_pressed = 1;
    if (key >= 'A' && key <= 'Z') key += 'a' - 'A';
    switch (key) {
    case 'w': case 'z': case '8':
        pac_want = PM_UP;
        break;
    case 'a': case 'q': case '4':
        pac_want = PM_LEFT;
        break;
    case 's': case '2': case '5':
        pac_want = PM_DOWN;
        break;
    case 'd': case '6':
        pac_want = PM_RIGHT;
        break;
    case 'p':
        pm_pause_requested = 1;
        break;
    }
}
