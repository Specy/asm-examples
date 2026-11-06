/*
 * At each tile centre an enemy chooses the legal forward tile nearest its target.
 * The chaser follows the player, the ambusher aims ahead, the scout approaches from
 * another angle, and the roamer switches between pursuit and its corner. Temporary
 * power-ups reverse them and make them vulnerable. Defeated enemies return to the pen.
 */
static void eat_enemy(struct Enemy *enemy);

/* Each enemy at its start, not frightened. Their dot counters are kept. */
static void reset_enemies(void) {
    for (int i = 0; i < 4; i++) {
        int dots = enemies[i].dots;
        enemies[i] = enemy_start[i];
        enemies[i].dots = dots;
    }
}

static void clear_enemy_counters(void) {
    for (int i = 0; i < 4; i++) enemies[i].dots = 0;
}

/* The enemy next in line to leave the house, Ambusher, then Scout, then Roamer, or none. */
static struct Enemy *waiting_enemy(int *number) {
    for (int i = 1; i < 4; i++)
        if (enemies[i].state == IN_HOUSE) {
            if (number) *number = i;
            return &enemies[i];
        }
    return 0;
}

/*
 * Lets the waiting enemy out when its dot counter says so, or when the player has gone four seconds
 * (three from level 5) without eating. After a death one shared counter releases Ambusher at 7 dots
 * and Scout at 17; Roamer still in the house at 32 switches it off.
 */
static void release_enemies(void) {
    int number;
    struct Enemy *enemy = waiting_enemy(&number);
    if (!enemy) return;
    int release = 0;
    if (++dot_timer >= (level <= 4 ? 240 : 180)) {
        dot_timer = 0;
        release = 1;
    } else if (global_mode) {
        if (number == 1 && global_dots == 7) release = 1;
        else if (number == 2 && global_dots == 17) release = 1;
        else if (number == 3 && global_dots == 32) global_mode = 0;
    } else {
        int limit = number == 2 ? level_info->scout_dots : number == 3 ? level_info->roamer_dots : 0;
        release = enemy->dots >= limit;
    }
    if (release) enemy->state = LEAVING;
}

/*
 * This frame's speed, in 1/256 pixel. Slow in the tunnel, slow and blue when frightened, and Chaser
 * speeds up twice as the dots run out ("Cruise pursuit").
 */
static int enemy_speed(const struct Enemy *enemy, int number) {
    switch (enemy->state) {
    case EYES:
    case ENTERING:
        return EYES_SPEED;
    case IN_HOUSE:
        return HOUSE_SPEED;
    case LEAVING:
        return LEAVING_SPEED;
    }
    int column = enemy->x >> 3, row = enemy->y >> 3;
    if (row == TUNNEL_ROW && (column <= 5 || column >= 22)) return level_info->tunnel_speed;
    if (enemy->fright) return level_info->fright_enemy_speed;
    if (number == 0 && pursuit_enabled) {
        if (dots_left <= level_info->pursuit2_dots) return level_info->pursuit2_speed;
        if (dots_left <= level_info->pursuit1_dots) return level_info->pursuit1_speed;
    }
    return level_info->enemy_speed;
}

/* The tile a enemy is heading for. */
static void enemy_target(const struct Enemy *enemy, int number, int *target_x, int *target_y) {
    int chase = mode_index & 1;
    /* Chaser as pursuit chases while the others scatter */
    if (!chase && number == 0 && pursuit_enabled && dots_left <= level_info->pursuit1_dots) chase = 1;
    int player_column = player_x >> 3, player_row = player_y >> 3;
    int dx = dir_dx[player_dir], dy = dir_dy[player_dir];
    *target_x = enemy->scatter_x;
    *target_y = enemy->scatter_y;
    if (!chase) return;
    switch (number) {
    case 0: /* Chaser */
        *target_x = player_column;
        *target_y = player_row;
        break;
    case 1: /* Ambusher */
        *target_x = player_column + 4 * dx - (player_dir == UP ? 4 : 0);
        *target_y = player_row + 4 * dy;
        break;
    case 2: { /* Scout: Chaser's tile, mirrored through the pivot */
        int pivot_x = player_column + 2 * dx - (player_dir == UP ? 2 : 0);
        int pivot_y = player_row + 2 * dy;
        *target_x = 2 * pivot_x - (enemies[0].x >> 3);
        *target_y = 2 * pivot_y - (enemies[0].y >> 3);
        break;
    }
    case 3: { /* Roamer */
        int far_x = (enemy->x >> 3) - player_column, far_y = (enemy->y >> 3) - player_row;
        if (far_x * far_x + far_y * far_y >= 64) {
            *target_x = player_column;
            *target_y = player_row;
        }
        break;
    }
    }
}

/* A enemy at a tile centre picks where to go next. */
static void choose_direction(struct Enemy *enemy, int number) {
    int column = enemy->x >> 3, row = enemy->y >> 3;
    int back = enemy->dir ^ 1;
    if (enemy->state == EYES && row == 14 && (column == 13 || column == 14)) {
        enemy->dir = column == 13 ? RIGHT : LEFT; /* eyes beside the door turn to it */
        return;
    }
    if (enemy->state == ACTIVE && enemy->fright) {
        /* frightened: a random way that is open, else on round the tie-break order */
        int start = random_number() & 3;
        for (int i = 0; i < 4; i++) {
            int dir = choice_order[(start + i) & 3];
            if (dir != back && is_open(column + dir_dx[dir], row + dir_dy[dir])) {
                enemy->dir = dir;
                return;
            }
        }
        enemy->dir = back;
        return;
    }
    int target_x = 13, target_y = 14; /* eyes head home */
    if (enemy->state != EYES) enemy_target(enemy, number, &target_x, &target_y);
    int best = -1, best_distance = 0;
    for (int i = 0; i < 4; i++) {
        int dir = choice_order[i];
        int next_x = column + dir_dx[dir], next_y = row + dir_dy[dir];
        if (dir == back || !is_open(next_x, next_y)) continue;
        /* no turning up just above the enemy house or the player's start, eyes excepted */
        if (dir == UP && enemy->state != EYES && (row == 14 || row == 26) &&
            (column == 12 || column == 15))
            continue;
        int distance = (next_x - target_x) * (next_x - target_x) +
                       (next_y - target_y) * (next_y - target_y);
        if (best < 0 || distance < best_distance) { /* a tie keeps the earlier direction */
            best = dir;
            best_distance = distance;
        }
    }
    enemy->dir = best >= 0 ? best : back; /* a dead end: back the way it came */
}

/* One pixel of a enemy. */
static void enemy_step(struct Enemy *enemy, int number) {
    switch (enemy->state) {
    case IN_HOUSE: /* bob up and down */
        if (enemy->dir == UP) {
            if (--enemy->y <= HOUSE_TOP_Y) enemy->dir = DOWN;
        } else {
            if (++enemy->y >= HOUSE_BOTTOM_Y) enemy->dir = UP;
        }
        return;
    case LEAVING: /* to the middle, across to the door, up and out */
        if (enemy->x != HOUSE_DOOR_X) {
            if (enemy->y != HOUSE_MID_Y) {
                enemy->dir = enemy->y < HOUSE_MID_Y ? DOWN : UP;
                enemy->y += toward(enemy->y, HOUSE_MID_Y);
            } else {
                enemy->dir = enemy->x < HOUSE_DOOR_X ? RIGHT : LEFT;
                enemy->x += toward(enemy->x, HOUSE_DOOR_X);
            }
        } else if (--enemy->y > HOUSE_DOOR_Y) {
            enemy->dir = UP;
        } else {
            enemy->dir = LEFT; /* out: they always set off to the left */
            enemy->state = ACTIVE;
            if (number == 3) pursuit_enabled = 1; /* Roamer out again: Chaser's pursuit resumes */
        }
        return;
    case ENTERING: /* eyes: down into the house, across to home */
        if (enemy->y < HOUSE_MID_Y) {
            enemy->y++;
            enemy->dir = DOWN;
        } else if (enemy->x != enemy->home_x) {
            enemy->dir = enemy->x < enemy->home_x ? RIGHT : LEFT;
            enemy->x += toward(enemy->x, enemy->home_x);
        } else {
            enemy->state = LEAVING; /* home: a enemy again, and straight back out */
            enemy->fright = 0;
        }
        return;
    }
    /* in the maze: decide at each tile centre, then move */
    if ((enemy->x & 7) == 3 && (enemy->y & 7) == 3) choose_direction(enemy, number);
    enemy->x += dir_dx[enemy->dir];
    enemy->y += dir_dy[enemy->dir];
    if (enemy->x < -8) enemy->x += 240; /* through the tunnel */
    if (enemy->x > 231) enemy->x -= 240;
    if (enemy->state == EYES && enemy->x == HOUSE_DOOR_X && enemy->y == HOUSE_DOOR_Y) {
        enemy->state = ENTERING; /* eyes above the door go in */
        enemy->dir = DOWN;
    }
}

/* One frame of each enemy. */
static void update_enemies(void) {
    for (int number = 0; number < 4; number++) {
        struct Enemy *enemy = &enemies[number];
        enemy->acc += enemy_speed(enemy, number);
        int steps = enemy->acc >> 8;
        enemy->acc &= 255;
        while (steps-- > 0) enemy_step(enemy, number);
    }
}

/* Every enemy in the maze turns around, as at every change of mode. */
static void reverse_enemies(void) {
    for (int i = 0; i < 4; i++)
        if (enemies[i].state == ACTIVE) enemies[i].dir ^= 1;
}

/*
 * the player ate an energizer. The enemies turn around and, while the level still has a frightened
 * time, turn blue: those in the house too, not the eyes.
 */
static void energize(void) {
    enemies_eaten = 0;
    reverse_enemies();
    if (!level_info->fright_frames) return;
    fright_timer = level_info->fright_frames;
    for (int i = 0; i < 4; i++)
        if (enemies[i].state != EYES && enemies[i].state != ENTERING) enemies[i].fright = 1;
}

/*
 * Frightened time, then the scatter and chase timetable, which stands still while the enemies are
 * blue, then the bonus.
 */
static void update_timers(void) {
    if (fright_timer) {
        if (--fright_timer == 0)
            for (int i = 0; i < 4; i++) enemies[i].fright = 0; /* blue no more */
    } else if (mode_index < 7 && --mode_timer == 0) {
        /* the eighth mode, chase, never ends */
        if (++mode_index < 7) mode_timer = level_info->modes[mode_index];
        reverse_enemies();
    }
    if (bonus_timer) bonus_timer--;
    if (bonus_score_timer) bonus_score_timer--;
}

/* A enemy on the player's tile either is eaten or catches him. */
static void check_collisions(void) {
    int column = player_x >> 3, row = player_y >> 3;
    for (int i = 0; i < 4; i++) {
        struct Enemy *enemy = &enemies[i];
        if (enemy->state != ACTIVE || enemy->x >> 3 != column || enemy->y >> 3 != row) continue;
        if (enemy->fright) {
            eat_enemy(enemy);
        } else {
            stop_sounds(); /* the game stands still in silence */
            enter_state(DYING);
        }
        return;
    }
}

/*
 * 200, 400, 800 then 1600 points, and the game stops for a second to show them where the enemy
 * was. The enemy becomes a pair of eyes.
 */
static void eat_enemy(struct Enemy *enemy) {
    enemies_eaten++;
    add_score(100 << enemies_eaten);
    enemy->state = EYES;
    enemy->fright = 0;
    eaten_enemy = enemy;
    enter_state(EAT_ENEMY);
    play_sound(CHANNEL_3, sound_enemy);
}

/*
 * The cell of sprites.bmp for a enemy now. Its skirt changes every eight frames; frightened, it
 * flashes white for the last few seconds.
 */
static const uint32_t *enemy_frame(const struct Enemy *enemy) {
    int skirt = (frame_count >> 3) & 1;
    if (enemy->state == EYES || enemy->state == ENTERING) return sheet_cell(4 + enemy->dir, 6);
    if (enemy->fright) {
        int flashing = fright_timer <= level_info->flashes * 28; /* 14 frames white, 14 blue */
        int white = flashing && fright_timer % 28 < 14;
        return sheet_cell(skirt + (white ? 2 : 0), 6);
    }
    return sheet_cell(enemy->dir * 2 + skirt, enemy->sheet_row);
}

/* The next number of a xorshift generator. */
static uint32_t random_number(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
