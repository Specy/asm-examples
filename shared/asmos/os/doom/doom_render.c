/*
 * DOOM on asmOS, one of its four parts: the renderer: walls, floors, sprites, the automap and the screen wipe.
 * DOOM is too large for one compilation, so os/doom/doom_*.c each include some of the engine's
 * Files, in os/doom/doomgeneric/, and of asmOS's for it, beside them, and are compiled apart, as
 * os/main.c and os/apps/apps.c are; the Build links them by name.
 */
#include "doomgeneric/r_bsp.c"
#include "doomgeneric/r_data.c"
#include "doomgeneric/r_draw.c"
#include "doomgeneric/r_main.c"
#include "doomgeneric/r_plane.c"
#include "doomgeneric/r_segs.c"
#include "doomgeneric/r_sky.c"
#include "doomgeneric/r_things.c"
#include "doomgeneric/v_video.c"
#include "doomgeneric/f_wipe.c"
#include "doomgeneric/am_map.c"
#include "doomgeneric/tables.c"
