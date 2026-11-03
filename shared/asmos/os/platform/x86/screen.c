/*
 * The screen is Linux's framebuffer device, /dev/fb0. Its driver says how large the screen is and
 * how its pixels are laid out (FBIOGET_VSCREENINFO, FBIOGET_FSCREENINFO), and mmap puts its
 * memory in the program's: a pixel written there is on the screen, so nothing is left to do to
 * show it. The emulated machine's framebuffer holds 32-bit pixels, 0x00RRGGBB, as asmOS draws.
 */
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

static int x86_screen_w, x86_screen_h;   /* for the tablet, input.c */

int platform_screen_open(uint32_t **pixels, int *width, int *height, int *stride) {
    int fb = open("/dev/fb0", O_RDWR);
    if (fb < 0) {
        printf("asmOS needs the framebuffer, /dev/fb0\n");
        return -1;
    }
    struct fb_var_screeninfo var;
    struct fb_fix_screeninfo fix;
    ioctl(fb, FBIOGET_VSCREENINFO, &var);
    ioctl(fb, FBIOGET_FSCREENINFO, &fix);
    void *memory = mmap(NULL, fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    if (memory == MAP_FAILED) return -1;
    *pixels = memory;
    *width = x86_screen_w = var.xres;
    *height = x86_screen_h = var.yres;
    *stride = fix.line_length / 4;
    return 0;
}

void platform_screen_present(int x, int y, int w, int h) {
    (void)x, (void)y, (void)w, (void)h;
}

void platform_print(const char *text) { fputs(text, stdout); }
