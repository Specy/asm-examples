/*
 * asmOS's applications, each a process the kernel runs (see os.h). This File and the Files it
 * includes are compiled apart from the kernel's half, os/main.c, into their own Generated
 * assembly; the Build links the two by the names os.h declares.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../keys.h"

#include "../os.h"
#include "widgets.c"
#include "notepad.c"
#include "calculator.c"
#include "paint.c"
#include "clock.c"
#include "viewer.c"
#include "music.c"
#include "video.c"
#include "hex.c"
#include "pacman.c"
#include "blocks.c"
#include "mines.c"
#include "terminal.c"
#include "files.c"
#include "tasks.c"
#include "settings.c"
#include "about.c"

const App APPS[APP_COUNT] = {
    [APP_NOTEPAD] = { "Notepad", &ICON_NOTEPAD, notepad_main, GROUP_ACCESSORIES, 1, "notepad" },
    [APP_CALCULATOR] = { "Calculator", &ICON_CALCULATOR, calculator_main, GROUP_ACCESSORIES, 1, "calc" },
    [APP_PAINT] = { "Paint", &ICON_PAINT, paint_main, GROUP_ACCESSORIES, 1, "paint" },
    [APP_CLOCK] = { "Clock", &ICON_CLOCK, clock_main, GROUP_ACCESSORIES, 0, "clock" },
    [APP_VIEWER] = { "Image Viewer", &ICON_VIEWER, viewer_main, GROUP_ACCESSORIES, 0, "viewer" },
    [APP_MUSIC] = { "Music Player", &ICON_MUSIC, music_main, GROUP_ACCESSORIES, 1, "music" },
    [APP_VIDEO] = { "Video Player", &ICON_VIDEO, video_main, GROUP_ACCESSORIES, 1, "video" },
    [APP_HEX] = { "Hex Viewer", &ICON_HEX, hex_main, GROUP_ACCESSORIES, 0, "hex" },
    /* the Assembly Editor is compiled apart too, in os/ide/ */
    [APP_IDE] = { "Assembly Editor", &ICON_IDE, ide_main, GROUP_ACCESSORIES, 1, "asm" },
    [APP_PACMAN] = { "Pac-Man", &ICON_PACMAN, pacman_main, GROUP_GAMES, 1, "pacman" },
    [APP_BLOCKS] = { "Blocks", &ICON_BLOCKS, blocks_main, GROUP_GAMES, 1, "blocks" },
    [APP_MINES] = { "Minesweeper", &ICON_MINES, mines_main, GROUP_GAMES, 1, "mines" },
    /* DOOM is too large to be one of these Files: os/doom/ is compiled apart */
    [APP_DOOM] = { "DOOM", &ICON_DOOM, doom_main, GROUP_GAMES, 1, "doom" },
    [APP_TERMINAL] = { "Terminal", &ICON_TERMINAL, terminal_main, GROUP_SYSTEM, 1, "terminal" },
    [APP_FILES] = { "Files", &ICON_FOLDER, files_main, GROUP_SYSTEM, 1, "files" },
    [APP_TASKS] = { "Task Manager", &ICON_TASKS, tasks_main, GROUP_SYSTEM, 1, "tasks" },
    [APP_SETTINGS] = { "Settings", &ICON_SETTINGS, settings_main, GROUP_SYSTEM, 0, "settings" },
    [APP_ABOUT] = { "About asmOS", &ICON_COMPUTER, about_main, GROUP_SYSTEM, 0, "about" },
};
