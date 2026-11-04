// @screen width=640 height=480
/*
 * asmOS, a small operating system: a kernel that runs applications as processes and switches
 * between them, a compositing window manager with menus and dialogs, a desktop with a Start menu,
 * and eighteen applications.
 *
 * It runs as one ordinary program of the machine. What it needs of the machine, a screen, the
 * keyboard and the pointer, clocks, a way to switch processes, sound and files, the platform
 * layer gives it: os/platform/platform.h says what, and os/platform/<machine>/ how. On x86 that is
 * a Linux program drawing on /dev/fb0 and keeping its files in the Project, the program's working
 * directory; on MIPS and RISC-V one program of MARS or RARS, drawing on their bitmap display.
 *
 * This File and the Files it includes are the kernel's half; os/apps/apps.c and its Files are the
 * applications'. Each half is compiled on its own into its own Generated assembly, and the Build
 * links the two by the names os.h declares.
 */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os.h"
#include "keys.h"
#include "platform/platform.h"
#include "font.c"
#include "gfx.c"
#include "blit.c"
#include "icons.c"
#include "kernel.c"
#include "bmp.c"
#include "input.c"
#include "sound.c"
#include "wm.c"
#include "dialogs.c"
#include "platform/platform.c"

static uint32_t *fb_pixels;
static int fb_stride;
static long shutdown_deadline;

/* Shows a rectangle of the back buffer: nothing to copy where the back buffer is the screen. */
static void show_rect(Rect r) {
    if (back_buffer != fb_pixels)
        copy_rect32(fb_pixels + r.y * fb_stride + r.x, fb_stride * 4, back_buffer + r.y * screen_w + r.x,
                    screen_w * 4, r.w, r.h);
    platform_screen_present(r.x, r.y, r.w, r.h);
}

/* Shows the whole back buffer at once. */
static void show_all(void) { show_rect(rect(0, 0, screen_w, screen_h)); }

/* Large text, each pixel of the font drawn as a square of `scale` pixels. */
static void draw_text_scaled(int x, int y, const char *text, int scale, uint32_t colour) {
    for (int i = 0; text[i]; i++) {
        if (text[i] == ' ') continue;
        const unsigned char *glyph = (const unsigned char *)font_bits + (text[i] - FONT_FIRST) * FONT_H;
        for (int line = 0; line < FONT_H; line++)
            for (int column = 0; column < FONT_W; column++)
                if (glyph[line] & (0x80 >> column))
                    fill_rect(x + (i * FONT_W + column) * scale, y + line * scale, scale, scale, colour);
    }
}

/* The chip and the name in the middle of a black screen, and a bar of blocks running under them
 * while the start-up chime plays. */
static void boot_splash(void) {
    gfx_clip(rect(0, 0, screen_w, screen_h));
    fill_rect(0, 0, screen_w, screen_h, BLACK);
    int cx = screen_w / 2, cy = screen_h / 2 - 40;
    draw_icon(&ICON_CHIP, cx - 64, cy - 72, 8);
    draw_text_scaled(cx - 60, cy + 64, "asm", 3, 0x8080ff);
    draw_text_scaled(cx - 60 + 3 * FONT_W * 3, cy + 64, "OS", 3, WHITE);
    const char *line = "A small operating system in " PLATFORM_LANGUAGES;
    draw_text(cx - text_width(line) / 2, cy + 112, line, SHADOW);
    show_all();
    sound_chime(0);
    Rect bar = rect(cx - 80, cy + 150, 160, 14);
    frame_rect(bar.x - 2, bar.y - 2, bar.w + 4, bar.h + 4, SHADOW);
    for (int frame = 0; frame < 48; frame++) {
        gfx_clip(bar);
        fill_rect(bar.x, bar.y, bar.w, bar.h, BLACK);
        for (int block = 0; block < 3; block++) {
            int x = (frame * 6 + block * 14) % (bar.w + 40) - 30;
            fill_rect(bar.x + x, bar.y + 2, 10, bar.h - 4, block == 1 ? 0x4040ff : 0x2020a0);
        }
        gfx_clip(rect(0, 0, screen_w, screen_h));
        show_rect(rect(bar.x - 2, bar.y - 2, bar.w + 4, bar.h + 4));
        sound_run();
        platform_sleep_ms(30);
    }
}

/* The last screen: orange words on black, once the closing chime has played. */
static void shutdown_screen(void) {
    sound_all_off();
    sound_chime(1);
    gfx_target(back_buffer, screen_w, screen_h, screen_w);
    fill_rect(0, 0, screen_w, screen_h, NAVY);
    const char *saving = "asmOS is shutting down...";
    draw_text_bold((screen_w - text_width(saving)) / 2, screen_h / 2 - 6, saving, WHITE);
    show_all();
    for (int i = 0; i < 50; i++) {
        sound_run();
        platform_sleep_ms(30);
    }
    fill_rect(0, 0, screen_w, screen_h, BLACK);
    const char *lines[] = { "It is now safe to turn off", "your computer." };
    for (int l = 0; l < 2; l++)
        draw_text_scaled((screen_w - text_width(lines[l]) * 2) / 2, screen_h / 2 - 30 + l * 32, lines[l], 2,
                         0xf0a020);
    show_all();
    while (sound_run() >= 0) platform_sleep_ms(30);
}

void os_shutdown(void) {
    lock();
    if (!shutting_down) {
        shutting_down = 1;
        shutdown_deadline = os_now_ms() + 1500;
        popup_close_all();
        Message message = { 0 };
        message.type = MSG_QUIT;
        for (int i = 1; i < MAX_PROCESSES; i++)
            if (processes[i].used && processes[i].state != PROC_ENDED) post_message(processes[i].pid, &message);
    }
    unlock();
}

/* The applications autostart.txt lists, one a line: the name the Terminal's `open` takes, then
 * an argument for it, such as a file to open. */
static void autostart(void) {
    char *text = read_file_with("autostart.txt", NULL, malloc, free);
    if (!text) return;
    for (char *line = text, *next; *line; line = next) {
        next = line + strcspn(line, "\n");
        if (*next) *next++ = 0;
        line[strcspn(line, "\r")] = 0;
        char *argument = strchr(line, ' ');
        if (argument) *argument++ = 0;
        int app = os_app_named(line);
        if (app >= 0) os_spawn(app, argument && *argument ? argument : NULL);
    }
    free(text);
}

/* The shortest of the waits, ignoring the -1 of "nothing due". */
static long sooner(long a, long b) { return a < 0 ? b : b < 0 ? a : a < b ? a : b; }

int main(void) {
    if (platform_screen_open(&fb_pixels, &screen_w, &screen_h, &fb_stride) < 0) return 1;
    /* the window manager composes in a back buffer, or, where there is no memory for one, straight
     * on the screen, whose rows must then be as long as the screen is wide */
    size_t back_bytes = PLATFORM_BACK_BUFFER ? (size_t)screen_w * screen_h * 4 : 0;
    if (!kernel_memory_check(back_bytes)) return 1;
    uint32_t *back = PLATFORM_BACK_BUFFER ? malloc(back_bytes) : fb_stride == screen_w ? fb_pixels : NULL;
    if (!back) return 1;
    back_buffer = back;
    gfx_target(back, screen_w, screen_h, screen_w);
    kernel_start();
    settings_load();
    sound_init();
    platform_input_open();
    char line[80];
    snprintf(line, sizeof line, "asmOS: %d x %d. Click the Screen to use the keyboard and the mouse.\n",
             screen_w, screen_h);
    platform_print(line);
    boot_splash();
    wm_start(fb_pixels, fb_stride, back);
    autostart();

    long second = os_now_ms() + 1000, last_second = os_now_ms();
    for (;;) {
        long wake = schedule_pass();
        long due = sooner(wake, sooner(run_timers(), sound_run()));
        long now = os_now_ms();
        if (now >= second) {
            measure_load(now - last_second);
            wm_tick_second();
            last_second = now;
            second = now + 1000;
        }
        wm_render();
        if (shutting_down && (!applications_running() || now >= shutdown_deadline)) break;
        int ready = applications_ready();
        long timeout = ready ? 0 : sooner(due, second - now);
        long before = os_now_ms();
        input_wait((int)timeout);
        if (!ready) idle_ms += os_now_ms() - before;
        /* the timer runs while an application is ready, one the input just woke included, so that
         * its turn ends even if it never gives the processor up */
        ready = applications_ready();
        timer_set(ready);
        if (ready) yield();
    }
    timer_set(0);
    for (int i = 1; i < MAX_PROCESSES; i++)
        if (processes[i].used && processes[i].state != PROC_ENDED) process_cleanup(&processes[i]);
    shutdown_screen();
    return 0;
}
