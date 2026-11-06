/*
 * the player himself. He moves one pixel at a time, as many pixels a frame as his speed allows, and
 * keeps going until a wall stops him. The player's last direction is remembered and taken at the
 * first opening, so a key can be pressed before the turn.
 *
 * The player may turn a few pixels before or after a tile's centre: he then moves
 * diagonally, along the new direction and back onto the centre line at once, which is why he
 * corners faster than the enemies.
 */

static void dot_eaten(void);

static void reset_player(void) {
    player_x = PLAYER_START_X;
    player_y = PLAYER_START_Y;
    player_dir = player_want = LEFT;
    player_acc = player_stall = player_anim = player_moving = 0;
}

/* One step toward `target`: -1, 0 or 1. */
static int toward(int from, int target) {
    return from < target ? 1 : from > target ? -1 : 0;
}

/* One pixel of movement. */
static void player_step(void) {
    int column = player_x >> 3, row = player_y >> 3;
    if (player_want != player_dir) {
        /* turning back is always allowed; a side turn needs an open tile that way */
        if (player_want == (player_dir ^ 1) ||
            is_open(column + dir_dx[player_want], row + dir_dy[player_want]))
            player_dir = player_want;
    }
    int dx = dir_dx[player_dir], dy = dir_dy[player_dir];
    int centre_x = column * 8 + 3, centre_y = row * 8 + 3;
    int moved = 0;
    if (dx) {
        /* along a row: on to the centre, or past it when the next tile is open */
        if (player_x != centre_x || is_open(column + dx, row)) {
            player_x += dx;
            moved = 1;
        }
        if (player_y != centre_y) { /* back onto the row's centre line */
            player_y += toward(player_y, centre_y);
            moved = 1;
        }
    } else {
        if (player_y != centre_y || is_open(column, row + dy)) {
            player_y += dy;
            moved = 1;
        }
        if (player_x != centre_x) {
            player_x += toward(player_x, centre_x);
            moved = 1;
        }
    }
    /* out of one side of the tunnel, in at the other */
    if (player_x < -8) player_x += 240;
    if (player_x > 231) player_x -= 240;
    player_moving = moved;
    if (moved) player_anim++;
    else player_anim = 0;
}

/* Whatever is on the player's tile: a dot, an energizer, the bonus. */
static void player_eat(void) {
    int column = player_x >> 3, row = player_y >> 3;
    int tile = tile_at(column, row);
    if (tile == '.' || tile == 'o') {
        int energizer = tile == 'o';
        player_stall = energizer ? 3 : 1; /* he stops for a frame after a dot, three after these */
        add_score(energizer ? 50 : 10);
        if (energizer) energize();
        eat_tile(column, row);
        dot_eaten();
        play_chomp();
    } else if (bonus_timer && row == 20 && (column == 13 || column == 14)) {
        bonus_timer = 0;
        bonus_score_timer = 120; /* its points show for two seconds */
        add_score(level_info->bonus_points);
        play_sound(CHANNEL_3, sound_bonus);
    }
}

/* One frame of the player. */
static void update_player(void) {
    if (player_stall) {
        player_stall--;
        return;
    }
    int speed = fright_timer ? level_info->fright_player_speed : level_info->player_speed;
    player_acc += speed;
    int steps = player_acc >> 8; /* whole pixels this frame, the rest carried */
    player_acc &= 255;
    while (steps-- > 0 && game_state == PLAY) { /* the last dot ends the level */
        player_step();
        player_eat();
    }
}

/* The counting that follows every dot and energizer. */
static void dot_eaten(void) {
    dots_left--;
    dots_eaten++;
    dot_timer = 0;
    if (dots_eaten == 70 || dots_eaten == 170) bonus_timer = 570; /* nine and a half seconds */
    if (global_mode) {
        global_dots++;
    } else {
        struct Enemy *enemy = waiting_enemy(0);
        if (enemy) enemy->dots++;
    }
    if (dots_left == 0) enter_state(LEVEL_DONE);
}

/* A complete walking cycle keeps the same facing direction, including its idle pose. */
static const uint32_t *player_frame(void) {
    static const int phases[4] = { 0, 1, 0, 2 };
    int phase = player_moving ? phases[(player_anim >> 2) & 3] : 0;
    return sheet_cell(player_dir * 3 + phase, 0);
}
