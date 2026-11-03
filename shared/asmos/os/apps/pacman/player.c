/*
 * Pac-Man himself. He moves one pixel at a time, as many pixels a frame as his speed allows, and
 * keeps going until a wall stops him. The player's last direction is remembered and taken at the
 * first opening, so a key can be pressed before the turn.
 *
 * Like the arcade, he may turn a few pixels before or after a tile's centre: he then moves
 * diagonally, along the new direction and back onto the centre line at once, which is why he
 * corners faster than the ghosts.
 */

static void pm_dot_eaten(void);

static void pm_reset_pacman(void) {
    pac_x = PAC_START_X;
    pac_y = PAC_START_Y;
    pac_dir = pac_want = PM_LEFT;
    pac_acc = pac_stall = pac_anim = 0;
}

/* One step toward `target`: -1, 0 or 1. */
static int pm_toward(int from, int target) {
    return from < target ? 1 : from > target ? -1 : 0;
}

/* One pixel of movement. */
static void pac_step(void) {
    int column = pac_x >> 3, row = pac_y >> 3;
    if (pac_want != pac_dir) {
        /* turning back is always allowed; a side turn needs an open tile that way */
        if (pac_want == (pac_dir ^ 1) ||
            pm_is_open(column + pm_dir_dx[pac_want], row + pm_dir_dy[pac_want]))
            pac_dir = pac_want;
    }
    int dx = pm_dir_dx[pac_dir], dy = pm_dir_dy[pac_dir];
    int centre_x = column * 8 + 3, centre_y = row * 8 + 3;
    int moved = 0;
    if (dx) {
        /* along a row: on to the centre, or past it when the next tile is open */
        if (pac_x != centre_x || pm_is_open(column + dx, row)) {
            pac_x += dx;
            moved = 1;
        }
        if (pac_y != centre_y) { /* back onto the row's centre line */
            pac_y += pm_toward(pac_y, centre_y);
            moved = 1;
        }
    } else {
        if (pac_y != centre_y || pm_is_open(column, row + dy)) {
            pac_y += dy;
            moved = 1;
        }
        if (pac_x != centre_x) {
            pac_x += pm_toward(pac_x, centre_x);
            moved = 1;
        }
    }
    /* out of one side of the tunnel, in at the other */
    if (pac_x < -8) pac_x += 240;
    if (pac_x > 231) pac_x -= 240;
    if (moved) pac_anim++;
}

/* Whatever is on Pac-Man's tile: a dot, an energizer, the fruit. */
static void pac_eat(void) {
    int column = pac_x >> 3, row = pac_y >> 3;
    int tile = pm_tile_at(column, row);
    if (tile == '.' || tile == 'o') {
        int energizer = tile == 'o';
        pac_stall = energizer ? 3 : 1; /* he stops for a frame after a dot, three after these */
        pm_add_score(energizer ? 50 : 10);
        if (energizer) pm_energize();
        pm_eat_tile(column, row);
        pm_dot_eaten();
        pm_play_chomp();
    } else if (pm_fruit_timer && row == 20 && (column == 13 || column == 14)) {
        pm_fruit_timer = 0;
        pm_fruit_score_timer = 120; /* its points show for two seconds */
        pm_add_score(pm_level_info->fruit_points);
        pm_play_sound(PM_CHANNEL_3, pm_sound_fruit);
    }
}

/* One frame of Pac-Man. */
static void pm_update_pacman(void) {
    if (pac_stall) {
        pac_stall--;
        return;
    }
    int speed = pm_fright_timer ? pm_level_info->fright_pac_speed : pm_level_info->pac_speed;
    pac_acc += speed;
    int steps = pac_acc >> 8; /* whole pixels this frame, the rest carried */
    pac_acc &= 255;
    while (steps-- > 0 && pm_game_state == PM_PLAY) { /* the last dot ends the level */
        pac_step();
        pac_eat();
    }
}

/* The counting that follows every dot and energizer. */
static void pm_dot_eaten(void) {
    pm_dots_left--;
    pm_dots_eaten++;
    pm_dot_timer = 0;
    if (pm_dots_eaten == 70 || pm_dots_eaten == 170) pm_fruit_timer = 570; /* nine and a half seconds */
    if (pm_global_mode) {
        pm_global_dots++;
    } else {
        struct PmGhost *ghost = pm_waiting_ghost(0);
        if (ghost) ghost->dots++;
    }
    if (pm_dots_left == 0) pm_enter_state(PM_LEVEL_DONE);
}

/*
 * The cell of sprites.bmp for Pac-Man now. His mouth goes half open, open, half open, shut as he
 * moves, and stays as it was while he stands still.
 */
static const uint32_t *pm_pacman_frame(void) {
    static const int mouth[4] = { 1, 2, 1, 0 }; /* half, open, half, shut */
    int opening = mouth[(pac_anim >> 1) & 3];
    return pm_sheet_cell(opening ? pac_dir * 2 + opening : 0, 0);
}
