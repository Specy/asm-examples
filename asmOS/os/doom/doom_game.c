/*
 * DOOM on asmOS, one of its four parts: the game: its loop, menus, configuration and the IWAD.
 * DOOM is too large for one compilation, so os/doom/doom_*.c each include some of the engine's
 * Files, in os/doom/doomgeneric/, and of asmOS's for it, beside them, and are compiled apart, as
 * os/main.c and os/apps/apps.c are; the Build links them by name.
 */
#include "doomgeneric/d_main.c"
#include "doomgeneric/d_loop.c"
#include "doomgeneric/d_net.c"
#include "doomgeneric/d_event.c"
#include "doomgeneric/d_items.c"
#include "doomgeneric/d_iwad.c"
#include "doomgeneric/d_mode.c"
#include "doomgeneric/g_game.c"
#include "doomgeneric/m_menu.c"
#include "doomgeneric/m_config.c"
#include "doomgeneric/m_controls.c"
#include "doomgeneric/m_misc.c"
#include "doomgeneric/m_argv.c"
#include "doomgeneric/m_bbox.c"
#include "doomgeneric/m_cheat.c"
#include "doomgeneric/m_fixed.c"
#include "doomgeneric/m_random.c"
#include "doomgeneric/dummy.c"
#include "doomgeneric/doomdef.c"
#include "doomgeneric/doomstat.c"
#include "doomgeneric/dstrings.c"
