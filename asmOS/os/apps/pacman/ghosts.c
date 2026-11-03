/*
 * The four ghosts, after the arcade as The Pac-Man Dossier describes it. At every tile centre a
 * ghost picks the open direction, never straight back, whose next tile is nearest its target
 * tile, trying up, left, down, right in that order on a tie. Only the targets differ:
 *
 *   Blinky (red)     Pac-Man's own tile.
 *   Pinky (pink)     four tiles ahead of Pac-Man; when he faces up, also four to the left, as the
 *                    arcade's arithmetic overflow made it.
 *   Inky (cyan)      the tile two ahead of Pac-Man (and two left when he faces up), doubled away
 *                    from Blinky.
 *   Clyde (orange)   Pac-Man's tile while more than eight tiles away, else his own corner.
 *
 * In scatter mode each heads for its corner instead; frightened, they turn at random. Modes change
 * on a timetable, and every change turns the ghosts around.
 */

static void pm_eat_ghost(struct PmGhost *ghost);

/* Each ghost at its start, not frightened. Their dot counters are kept. */
static void pm_reset_ghosts(void) {
    for (int i = 0; i < 4; i++) {
        int dots = pm_ghosts[i].dots;
        pm_ghosts[i] = pm_ghost_start[i];
        pm_ghosts[i].dots = dots;
    }
}

static void pm_clear_ghost_counters(void) {
    for (int i = 0; i < 4; i++) pm_ghosts[i].dots = 0;
}

/* The ghost next in line to leave the house, Pinky, then Inky, then Clyde, or none. */
static struct PmGhost *pm_waiting_ghost(int *number) {
    for (int i = 1; i < 4; i++)
        if (pm_ghosts[i].state == PM_IN_HOUSE) {
            if (number) *number = i;
            return &pm_ghosts[i];
        }
    return 0;
}

/*
 * Lets the waiting ghost out when its dot counter says so, or when Pac-Man has gone four seconds
 * (three from level 5) without eating. After a death one shared counter releases Pinky at 7 dots
 * and Inky at 17; Clyde still in the house at 32 switches it off.
 */
static void pm_release_ghosts(void) {
    int number;
    struct PmGhost *ghost = pm_waiting_ghost(&number);
    if (!ghost) return;
    int release = 0;
    if (++pm_dot_timer >= (pm_level <= 4 ? 240 : 180)) {
        pm_dot_timer = 0;
        release = 1;
    } else if (pm_global_mode) {
        if (number == 1 && pm_global_dots == 7) release = 1;
        else if (number == 2 && pm_global_dots == 17) release = 1;
        else if (number == 3 && pm_global_dots == 32) pm_global_mode = 0;
    } else {
        int limit = number == 2 ? pm_level_info->inky_dots : number == 3 ? pm_level_info->clyde_dots : 0;
        release = ghost->dots >= limit;
    }
    if (release) ghost->state = PM_LEAVING;
}

/*
 * This frame's speed, in 1/256 pixel. Slow in the tunnel, slow and blue when frightened, and Blinky
 * speeds up twice as the dots run out ("Cruise Elroy").
 */
static int pm_ghost_speed(const struct PmGhost *ghost, int number) {
    switch (ghost->state) {
    case PM_EYES:
    case PM_ENTERING:
        return PM_EYES_SPEED;
    case PM_IN_HOUSE:
        return PM_HOUSE_SPEED;
    case PM_LEAVING:
        return PM_LEAVING_SPEED;
    }
    int column = ghost->x >> 3, row = ghost->y >> 3;
    if (row == PM_TUNNEL_ROW && (column <= 5 || column >= 22)) return pm_level_info->tunnel_speed;
    if (ghost->fright) return pm_level_info->fright_ghost_speed;
    if (number == 0 && pm_elroy_enabled) {
        if (pm_dots_left <= pm_level_info->elroy2_dots) return pm_level_info->elroy2_speed;
        if (pm_dots_left <= pm_level_info->elroy1_dots) return pm_level_info->elroy1_speed;
    }
    return pm_level_info->ghost_speed;
}

/* The tile a ghost is heading for. */
static void pm_ghost_target(const struct PmGhost *ghost, int number, int *target_x, int *target_y) {
    int chase = pm_mode_index & 1;
    /* Blinky as Elroy chases while the others scatter */
    if (!chase && number == 0 && pm_elroy_enabled && pm_dots_left <= pm_level_info->elroy1_dots) chase = 1;
    int pac_column = pac_x >> 3, pac_row = pac_y >> 3;
    int dx = pm_dir_dx[pac_dir], dy = pm_dir_dy[pac_dir];
    *target_x = ghost->scatter_x;
    *target_y = ghost->scatter_y;
    if (!chase) return;
    switch (number) {
    case 0: /* Blinky */
        *target_x = pac_column;
        *target_y = pac_row;
        break;
    case 1: /* Pinky */
        *target_x = pac_column + 4 * dx - (pac_dir == PM_UP ? 4 : 0);
        *target_y = pac_row + 4 * dy;
        break;
    case 2: { /* Inky: Blinky's tile, mirrored through the pivot */
        int pivot_x = pac_column + 2 * dx - (pac_dir == PM_UP ? 2 : 0);
        int pivot_y = pac_row + 2 * dy;
        *target_x = 2 * pivot_x - (pm_ghosts[0].x >> 3);
        *target_y = 2 * pivot_y - (pm_ghosts[0].y >> 3);
        break;
    }
    case 3: { /* Clyde */
        int far_x = (ghost->x >> 3) - pac_column, far_y = (ghost->y >> 3) - pac_row;
        if (far_x * far_x + far_y * far_y >= 64) {
            *target_x = pac_column;
            *target_y = pac_row;
        }
        break;
    }
    }
}

/* A ghost at a tile centre picks where to go next. */
static void pm_choose_direction(struct PmGhost *ghost, int number) {
    int column = ghost->x >> 3, row = ghost->y >> 3;
    int back = ghost->dir ^ 1;
    if (ghost->state == PM_EYES && row == 14 && (column == 13 || column == 14)) {
        ghost->dir = column == 13 ? PM_RIGHT : PM_LEFT; /* eyes beside the door turn to it */
        return;
    }
    if (ghost->state == PM_ACTIVE && ghost->fright) {
        /* frightened: a random way that is open, else on round the tie-break order */
        int start = pm_random_number() & 3;
        for (int i = 0; i < 4; i++) {
            int dir = pm_choice_order[(start + i) & 3];
            if (dir != back && pm_is_open(column + pm_dir_dx[dir], row + pm_dir_dy[dir])) {
                ghost->dir = dir;
                return;
            }
        }
        ghost->dir = back;
        return;
    }
    int target_x = 13, target_y = 14; /* eyes head home */
    if (ghost->state != PM_EYES) pm_ghost_target(ghost, number, &target_x, &target_y);
    int best = -1, best_distance = 0;
    for (int i = 0; i < 4; i++) {
        int dir = pm_choice_order[i];
        int next_x = column + pm_dir_dx[dir], next_y = row + pm_dir_dy[dir];
        if (dir == back || !pm_is_open(next_x, next_y)) continue;
        /* no turning up just above the ghost house or Pac-Man's start, eyes excepted */
        if (dir == PM_UP && ghost->state != PM_EYES && (row == 14 || row == 26) &&
            (column == 12 || column == 15))
            continue;
        int distance = (next_x - target_x) * (next_x - target_x) +
                       (next_y - target_y) * (next_y - target_y);
        if (best < 0 || distance < best_distance) { /* a tie keeps the earlier direction */
            best = dir;
            best_distance = distance;
        }
    }
    ghost->dir = best >= 0 ? best : back; /* a dead end: back the way it came */
}

/* One pixel of a ghost. */
static void pm_ghost_step(struct PmGhost *ghost, int number) {
    switch (ghost->state) {
    case PM_IN_HOUSE: /* bob up and down */
        if (ghost->dir == PM_UP) {
            if (--ghost->y <= PM_HOUSE_TOP_Y) ghost->dir = PM_DOWN;
        } else {
            if (++ghost->y >= PM_HOUSE_BOTTOM_Y) ghost->dir = PM_UP;
        }
        return;
    case PM_LEAVING: /* to the middle, across to the door, up and out */
        if (ghost->x != PM_HOUSE_DOOR_X) {
            if (ghost->y != PM_HOUSE_MID_Y) {
                ghost->dir = ghost->y < PM_HOUSE_MID_Y ? PM_DOWN : PM_UP;
                ghost->y += pm_toward(ghost->y, PM_HOUSE_MID_Y);
            } else {
                ghost->dir = ghost->x < PM_HOUSE_DOOR_X ? PM_RIGHT : PM_LEFT;
                ghost->x += pm_toward(ghost->x, PM_HOUSE_DOOR_X);
            }
        } else if (--ghost->y > PM_HOUSE_DOOR_Y) {
            ghost->dir = PM_UP;
        } else {
            ghost->dir = PM_LEFT; /* out: they always set off to the left */
            ghost->state = PM_ACTIVE;
            if (number == 3) pm_elroy_enabled = 1; /* Clyde out again: Blinky's Elroy resumes */
        }
        return;
    case PM_ENTERING: /* eyes: down into the house, across to home */
        if (ghost->y < PM_HOUSE_MID_Y) {
            ghost->y++;
            ghost->dir = PM_DOWN;
        } else if (ghost->x != ghost->home_x) {
            ghost->dir = ghost->x < ghost->home_x ? PM_RIGHT : PM_LEFT;
            ghost->x += pm_toward(ghost->x, ghost->home_x);
        } else {
            ghost->state = PM_LEAVING; /* home: a ghost again, and straight back out */
            ghost->fright = 0;
        }
        return;
    }
    /* in the maze: decide at each tile centre, then move */
    if ((ghost->x & 7) == 3 && (ghost->y & 7) == 3) pm_choose_direction(ghost, number);
    ghost->x += pm_dir_dx[ghost->dir];
    ghost->y += pm_dir_dy[ghost->dir];
    if (ghost->x < -8) ghost->x += 240; /* through the tunnel */
    if (ghost->x > 231) ghost->x -= 240;
    if (ghost->state == PM_EYES && ghost->x == PM_HOUSE_DOOR_X && ghost->y == PM_HOUSE_DOOR_Y) {
        ghost->state = PM_ENTERING; /* eyes above the door go in */
        ghost->dir = PM_DOWN;
    }
}

/* One frame of each ghost. */
static void pm_update_ghosts(void) {
    for (int number = 0; number < 4; number++) {
        struct PmGhost *ghost = &pm_ghosts[number];
        ghost->acc += pm_ghost_speed(ghost, number);
        int steps = ghost->acc >> 8;
        ghost->acc &= 255;
        while (steps-- > 0) pm_ghost_step(ghost, number);
    }
}

/* Every ghost in the maze turns around, as at every change of mode. */
static void pm_reverse_ghosts(void) {
    for (int i = 0; i < 4; i++)
        if (pm_ghosts[i].state == PM_ACTIVE) pm_ghosts[i].dir ^= 1;
}

/*
 * Pac-Man ate an energizer. The ghosts turn around and, while the level still has a frightened
 * time, turn blue: those in the house too, not the eyes.
 */
static void pm_energize(void) {
    pm_ghosts_eaten = 0;
    pm_reverse_ghosts();
    if (!pm_level_info->fright_frames) return;
    pm_fright_timer = pm_level_info->fright_frames;
    for (int i = 0; i < 4; i++)
        if (pm_ghosts[i].state != PM_EYES && pm_ghosts[i].state != PM_ENTERING) pm_ghosts[i].fright = 1;
}

/*
 * Frightened time, then the scatter and chase timetable, which stands still while the ghosts are
 * blue, then the fruit.
 */
static void pm_update_timers(void) {
    if (pm_fright_timer) {
        if (--pm_fright_timer == 0)
            for (int i = 0; i < 4; i++) pm_ghosts[i].fright = 0; /* blue no more */
    } else if (pm_mode_index < 7 && --pm_mode_timer == 0) {
        /* the eighth mode, chase, never ends */
        if (++pm_mode_index < 7) pm_mode_timer = pm_level_info->modes[pm_mode_index];
        pm_reverse_ghosts();
    }
    if (pm_fruit_timer) pm_fruit_timer--;
    if (pm_fruit_score_timer) pm_fruit_score_timer--;
}

/* A ghost on Pac-Man's tile either is eaten or catches him. */
static void pm_check_collisions(void) {
    int column = pac_x >> 3, row = pac_y >> 3;
    for (int i = 0; i < 4; i++) {
        struct PmGhost *ghost = &pm_ghosts[i];
        if (ghost->state != PM_ACTIVE || ghost->x >> 3 != column || ghost->y >> 3 != row) continue;
        if (ghost->fright) {
            pm_eat_ghost(ghost);
        } else {
            pm_stop_sounds(); /* the game stands still in silence */
            pm_enter_state(PM_DYING);
        }
        return;
    }
}

/*
 * 200, 400, 800 then 1600 points, and the game stops for a second to show them where the ghost
 * was. The ghost becomes a pair of eyes.
 */
static void pm_eat_ghost(struct PmGhost *ghost) {
    pm_ghosts_eaten++;
    pm_add_score(100 << pm_ghosts_eaten);
    ghost->state = PM_EYES;
    ghost->fright = 0;
    pm_eaten_ghost = ghost;
    pm_enter_state(PM_EAT_GHOST);
    pm_play_sound(PM_CHANNEL_3, pm_sound_ghost);
}

/*
 * The cell of sprites.bmp for a ghost now. Its skirt changes every eight frames; frightened, it
 * flashes white for the last few seconds.
 */
static const uint32_t *pm_ghost_frame(const struct PmGhost *ghost) {
    int skirt = (pm_frame_count >> 3) & 1;
    if (ghost->state == PM_EYES || ghost->state == PM_ENTERING) return pm_sheet_cell(4 + ghost->dir, 6);
    if (ghost->fright) {
        int flashing = pm_fright_timer <= pm_level_info->flashes * 28; /* 14 frames white, 14 blue */
        int white = flashing && pm_fright_timer % 28 < 14;
        return pm_sheet_cell(skirt + (white ? 2 : 0), 6);
    }
    return pm_sheet_cell(ghost->dir * 2 + skirt, ghost->sheet_row);
}

/* The next number of a xorshift generator. */
static uint32_t pm_random_number(void) {
    pm_rng_state ^= pm_rng_state << 13;
    pm_rng_state ^= pm_rng_state >> 17;
    pm_rng_state ^= pm_rng_state << 5;
    return pm_rng_state;
}
