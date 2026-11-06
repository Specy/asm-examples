/*
 * Relay Maze, in C for RISC-V, at 224 by 288 pixels: the same game as the assembly
 * version in ../assembly, reading the same pictures and maze from ../assets.
 *
 * Compile this File, then Build and Run the assembly it generates. Click the Screen to give it the
 * keyboard. W A S D move the player (Z Q S D and 8 4 5 6 too), P pauses, and any key starts a game.
 *
 * Compile sends Compiler Explorer this File and every File its quoted includes reach, so the parts
 * below are pasted in here and compiled as one program: each is an ordinary .c File, kept apart
 * only to be read apart. relay.h declares what they share.
 *
 *   state.c    variables and difficulty settings    sound.c    the music and the sound effects
 *   assets.c   reading the Files                    player.c   the player
 *   video.c    drawing                              enemies.c   the enemies
 *   maze.c     the maze                             game.c     states, score, input, frame timing
 *
 * sound.c comes before the parts that play its sounds, which name its tables.
 */
#include <stdio.h>
#include <stdlib.h>
#include <sim.h>

#include "relay.h"

/* The Screen: one word a pixel, configured by this line at every Build. */
SIM_SCREEN(display, GAME_W, GAME_H, 1);

#include "state.c"
#include "assets.c"
#include "video.c"
#include "maze.c"
#include "sound.c"
#include "player.c"
#include "enemies.c"
#include "game.c"

int main(void) {
    printf("Relay Maze for RISC-V, in C\n"
           "Click the Screen so it gets the keyboard, then press space.\n"
           "Move with W A S D (Z Q S D and 8 4 5 6 work too), P pauses.\n"
           "The arrow keys do not type characters, so the game cannot see them.\n");
    load_assets();
    init_sounds();
    rng_state ^= (uint32_t)sim_time(); /* a different game every time */
    if (!rng_state) rng_state = 1;
    enter_title();
    for (;;) {
        read_input();
        update_game();
        render_sprites();
        update_sounds();
        wait_frame();
    }
}
