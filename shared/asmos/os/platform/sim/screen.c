/*
 * The screen is MARS's and RARS's bitmap display: a grid of words in memory, each one pixel's
 * colour as 0x00RRGGBB, which the display shows as it is. SIM_SCREEN (<sim.h>) makes the grid a
 * global array and tells the Build to show it, 640 by 480 pixels of one word each: 1.2 MB of the
 * data segment. A word written there is on the screen, so nothing is left to do to show it.
 * The console is the Terminal, through the print-string syscall.
 */
#include <sim.h>

SIM_SCREEN(sim_screen, 640, 480, 1);

int platform_screen_open(uint32_t **pixels, int *width, int *height, int *stride) {
    *pixels = (uint32_t *)sim_screen;
    *width = 640;
    *height = 480;
    *stride = 640;
    return 0;
}

void platform_screen_present(int x, int y, int w, int h) {
    (void)x, (void)y, (void)w, (void)h;
}

void platform_print(const char *text) { sim_print_string(text); }
