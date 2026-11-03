/*
 * DOOM on asmOS, one of its four parts: the rest: the things' table, sound, status bar, intermission, WAD files and asmOS's platform.
 * DOOM is too large for one compilation, so os/doom/doom_*.c each include some of the engine's
 * Files, in os/doom/doomgeneric/, and of asmOS's for it, beside them, and are compiled apart, as
 * os/main.c and os/apps/apps.c are; the Build links them by name.
 */
#include "doomgeneric/info.c"
#include "doomgeneric/sounds.c"
#include "doomgeneric/s_sound.c"
#include "doomgeneric/hu_lib.c"
#include "doomgeneric/hu_stuff.c"
#include "doomgeneric/st_lib.c"
#include "doomgeneric/st_stuff.c"
#include "doomgeneric/wi_stuff.c"
#include "doomgeneric/f_finale.c"
#include "doomgeneric/w_wad.c"
#include "doomgeneric/w_main.c"
#include "doomgeneric/w_file.c"
#include "doomgeneric/w_checksum.c"
#include "doomgeneric/sha1.c"
#include "doomgeneric/z_zone.c"
#include "doomgeneric/statdump.c"
#include "doomgeneric/i_system.c"
#include "doomgeneric/i_sound.c"
#include "doomgeneric/i_cdmus.c"
#include "doomgeneric/i_endoom.c"
#include "doomgeneric/i_joystick.c"
#include "doomgeneric/memio.c"
#include "doomgeneric/mus2mid.c"
#include "doomlibc.c"
#include "i_asmos.c"
#include "i_asmos_sound.c"
