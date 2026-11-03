/*
 * Where a frame's time goes, for Game > Frame Times: the milliseconds and the instructions the parts
 * of DOOM that DOOM_PROF times took, added up in doom_prof and doom_prof_instructions until the
 * overlay shows them, once a second. Over many frames a sum of os_now_ms differences is the time
 * spent, give or take its rounding, which averages out; os_instructions is the emulator's own count.
 * While Frame Times is off DOOM_PROF only makes its call.
 */
#ifndef ASMOS_PROF_H
#define ASMOS_PROF_H

enum {
    DOOM_PROF_BSP,      /* R_RenderBSPNode: the walls, and the segs and columns behind them */
    DOOM_PROF_PLANES,   /* R_DrawPlanes: floors and ceilings */
    DOOM_PROF_MASKED,   /* R_DrawMasked: sprites and the player's weapon */
    DOOM_PROF_STBAR,    /* ST_Drawer: the status bar */
    DOOM_PROF_THINK,    /* P_Ticker: monsters, doors and the rest of the world, every tic */
    DOOM_PROF_SIGHT,    /* P_CheckSight, within the thinkers: whether a monster sees its target */
    DOOM_PROF_MOVE,     /* P_TryMove, within the thinkers: a thing moving, and what it hits */
    DOOM_PROF_INPUT,    /* I_StartTic: the messages and the sound */
    DOOM_PROF_CONVERT,  /* I_FinishUpdate: DOOM's picture through the palette */
    DOOM_PROF_DOUBLE,   /* I_FinishUpdate: the rows repeated to fill the window */
    DOOM_PROF_COUNT
};

extern int doom_show_times;
extern long doom_prof[DOOM_PROF_COUNT];
extern long doom_prof_instructions[DOOM_PROF_COUNT];

#define DOOM_PROF(part, call)                                                       \
    do {                                                                            \
        if (!doom_show_times) {                                                     \
            call;                                                                   \
        } else {                                                                    \
            long doom_prof_ms = os_now_ms(), doom_prof_count = os_instructions();   \
            call;                                                                   \
            doom_prof[part] += os_now_ms() - doom_prof_ms;                          \
            doom_prof_instructions[part] += os_instructions() - doom_prof_count;    \
        }                                                                           \
    } while (0)

#endif
