/*
 * DOOM on asmOS, one of its four parts: the world in play: things, monsters, doors, lifts, switches and saved games.
 * DOOM is too large for one compilation, so os/doom/doom_*.c each include some of the engine's
 * Files, in os/doom/doomgeneric/, and of asmOS's for it, beside them, and are compiled apart, as
 * os/main.c and os/apps/apps.c are; the Build links them by name.
 */
#include "doomgeneric/p_ceilng.c"
#include "doomgeneric/p_doors.c"
#include "doomgeneric/p_enemy.c"
#include "doomgeneric/p_floor.c"
#include "doomgeneric/p_inter.c"
#include "doomgeneric/p_lights.c"
#include "doomgeneric/p_map.c"
#include "doomgeneric/p_maputl.c"
#include "doomgeneric/p_mobj.c"
#include "doomgeneric/p_plats.c"
#include "doomgeneric/p_pspr.c"
#include "doomgeneric/p_saveg.c"
#include "doomgeneric/p_setup.c"
#include "doomgeneric/p_sight.c"
#include "doomgeneric/p_spec.c"
#include "doomgeneric/p_switch.c"
#include "doomgeneric/p_telept.c"
#include "doomgeneric/p_tick.c"
#include "doomgeneric/p_user.c"
