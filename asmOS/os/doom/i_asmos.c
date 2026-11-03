/*
 * DOOM's platform on asmOS, in place of doomgeneric's: the window DOOM draws in, the keys, the
 * clock, the IWAD and the end of the process. doomgeneric's own Files are DOOM; this one is what
 * i_video.c, i_input.c, i_timer.c and w_file_stdc.c were, made of os_ calls.
 *
 * DOOM draws its 320 by 200 picture in 256 colours into I_VideoBuffer. I_FinishUpdate turns it
 * into the window's pixels through the palette, as large as the window allows: the window can be
 * resized and maximized.
 */
#include "doomgeneric/config.h"
#include "doomgeneric/d_event.h"
#include "doomgeneric/d_loop.h"
#include "doomgeneric/d_main.h"
#include "doomgeneric/doomkeys.h"
#include "doomgeneric/doomstat.h"
#include "doomgeneric/i_system.h"
#include "doomgeneric/i_timer.h"
#include "doomgeneric/i_video.h"
#include "doomgeneric/m_argv.h"
#include "doomgeneric/m_menu.h"
#include "doomgeneric/s_sound.h"
#include "doomgeneric/v_video.h"
#include "doomgeneric/w_file.h"
#include "doomgeneric/z_zone.h"
#include "asmos_prof.h"

/* r_draw.c's: where the 3D view is in DOOM's picture */
extern int viewwindowy, viewheight;

/* Linux's KEY_ codes of the keys DOOM uses: <linux/input-event-codes.h> would clash with
 * doomkeys.h, which names DOOM's own codes KEY_ too. */
enum {
    LK_ESC = 1, LK_1 = 2, LK_0 = 11, LK_MINUS = 12, LK_EQUAL = 13, LK_BACKSPACE = 14, LK_TAB = 15,
    LK_ENTER = 28, LK_LEFTCTRL = 29, LK_LEFTSHIFT = 42, LK_COMMA = 51, LK_DOT = 52,
    LK_RIGHTSHIFT = 54, LK_KPASTERISK = 55, LK_LEFTALT = 56, LK_SPACE = 57, LK_CAPSLOCK = 58,
    LK_F1 = 59, LK_F10 = 68, LK_NUMLOCK = 69, LK_SCROLLLOCK = 70, LK_KP7 = 71, LK_KP8, LK_KP9,
    LK_KPMINUS, LK_KP4, LK_KP5, LK_KP6, LK_KPPLUS, LK_KP1, LK_KP2, LK_KP3, LK_KP0, LK_KPDOT,
    LK_F11 = 87, LK_F12 = 88, LK_KPENTER = 96, LK_RIGHTCTRL = 97, LK_KPSLASH = 98,
    LK_RIGHTALT = 100, LK_HOME = 102, LK_UP, LK_PAGEUP, LK_LEFT, LK_RIGHT, LK_END, LK_DOWN,
    LK_PAGEDOWN, LK_INSERT, LK_DELETE, LK_PAUSE = 119
};

/* i_asmos_sound.c: tops up the sound, which a frame lasts too long to do only once */
void I_SoundPump(void);

/* doom_simd.asm: rows of DOOM's picture through the palette, eight pixels at a time */
void doom_palette_row(uint32_t *to, const byte *from, int count, const uint32_t *palette);
void doom_palette_row2(uint64_t *to, const byte *from, int count, const uint64_t *palette2);
int doom_row_changed(const byte *row, byte *previous, int count);

/* Game > Fast Drawing: DOOM's pixel loops in AVX2, doom_simd.asm's, rather than in C. r_draw.c
 * reads it too. */
int doom_simd = 1;

enum { DOOM_MIN_W = 320, DOOM_MIN_H = 240 };
enum {
    DOOM_MENU_MENU = 1, DOOM_MENU_FPS, DOOM_MENU_TIMES, DOOM_MENU_SIMD, DOOM_MENU_DETAIL,
    DOOM_MENU_PROFILE, DOOM_MENU_BENCH, DOOM_MENU_EXIT, DOOM_MENU_KEYS, DOOM_MENU_ABOUT
};

/* m_menu.c's: DOOM's detail, 1 when low, and what F5 and its Options menu change it with */
extern int detailLevel;
void M_ChangeDetail(int choice);

/* The checks of FPS Counter, Frame Times, Fast Drawing, Low Detail and Profile change, in this
 * array itself: there is only one DOOM. */
static MenuItem doom_game_menu[] = {
    { "DOOM's Menu", DOOM_MENU_MENU, 0, "Esc", NULL },
    { "FPS Counter", DOOM_MENU_FPS, MENU_CHECKED, NULL, NULL },
    { "Frame Times", DOOM_MENU_TIMES, 0, NULL, NULL },
    { "Fast Drawing (AVX2)", DOOM_MENU_SIMD, MENU_CHECKED, NULL, NULL },
    { "Low Detail", DOOM_MENU_DETAIL, 0, "F5", NULL },
    { "Profile", DOOM_MENU_PROFILE, 0, NULL, NULL },
    { "Benchmark CPU", DOOM_MENU_BENCH, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", DOOM_MENU_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem doom_help_menu[] = {
    { "Keys...", DOOM_MENU_KEYS, 0, NULL, NULL },
    { "About DOOM...", DOOM_MENU_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const Menu doom_menus[] = {
    { "Game", doom_game_menu }, { "Help", doom_help_menu }, { NULL, NULL }
};

static Window *doom_window;

/* ------------------------------------------------------------------ the end */

void doom_exit(int code) {
    os_exit(code);
}

void doom_fatal(const char *message) {
    os_message_box(doom_window, "DOOM", message, MB_OK, MB_WARNING);
    os_exit(1);
}

/* ------------------------------------------------------------------ the clock (i_timer.c) */

static long doom_base_ms;
long doom_now_ms;   /* os_now_ms as DOOM last read it, which the sound goes by rather than ask again */

int I_GetTimeMS(void) {
    doom_now_ms = os_now_ms();
    return (int)(doom_now_ms - doom_base_ms);
}

int I_GetTime(void) {
    doom_now_ms = os_now_ms();
    return (int)((doom_now_ms - doom_base_ms) * TICRATE / 1000);
}

void I_Sleep(int ms) {
    os_sleep(ms);
}

void I_WaitVBL(int count) {
    (void)count;
}

void I_InitTimer(void) {
    doom_base_ms = os_now_ms();
}

/* ------------------------------------------------------------------ the IWAD (w_file_stdc.c) */

/* The whole file in memory, which DOOM then reads its lumps from as from a mapped file. */
static wad_file_t *doom_wad_open(char *path) {
    long size;
    char *data = os_read_file(path, &size);
    if (!data) return NULL;
    wad_file_t *wad = Z_Malloc(sizeof *wad, PU_STATIC, 0);
    wad->file_class = &stdc_wad_file;
    wad->mapped = (byte *)data;
    wad->length = (unsigned int)size;
    return wad;
}

static void doom_wad_close(wad_file_t *wad) {
    os_free(wad->mapped);
    Z_Free(wad);
}

static size_t doom_wad_read(wad_file_t *wad, unsigned int offset, void *buffer, size_t length) {
    if (offset >= wad->length) return 0;
    if (length > wad->length - offset) length = wad->length - offset;
    memcpy(buffer, wad->mapped + offset, length);
    return length;
}

wad_file_class_t stdc_wad_file = { doom_wad_open, doom_wad_close, doom_wad_read };

/* ------------------------------------------------------------------ the picture (i_video.c) */

byte *I_VideoBuffer = NULL;
boolean screenvisible = true;
boolean screensaver_mode = false;
int usegamma = 0;
int usemouse = 0;
float mouse_acceleration = 2.0;
int mouse_threshold = 10;
int screen_width = SCREENWIDTH, screen_height = SCREENHEIGHT, screen_bpp = 32;
int fullscreen = 0, aspect_ratio_correct = 1;
int show_diskicon = 0, diskicon_readbytes = 0;
char *video_driver = "";

static uint32_t doom_palette[256];
static uint64_t doom_palette2[256];     /* each colour twice, for a picture twice as wide */
static uint32_t *doom_pixels;           /* the window's client area */
static int doom_client_w, doom_client_h;
static Rect doom_view;                  /* where the picture goes in it */
static int doom_scale;                  /* 1, or 2: each of DOOM's pixels two wide */
static int doom_crop;                   /* columns of DOOM's picture left out at either side */
static int doom_show_fps = 1;           /* the frame rate over the picture's top right corner */
int doom_show_times;                    /* where a frame's time goes, over its top left corner */
static char doom_fps_text[24] = "-- fps";
static char doom_times_text[4][160];    /* the lines of that */
static int doom_frames;                 /* frames shown since doom_fps_since */
static long doom_fps_since;
static long doom_tics_since;            /* gametic at doom_fps_since */
static long doom_ms_logic, doom_ms_draw, doom_ms_show;   /* where the time went since then */
static long doom_in_logic, doom_in_draw, doom_in_show;   /* and the instructions, while shown */
long doom_prof[DOOM_PROF_COUNT];        /* and inside those, asmos_prof.h's parts */
long doom_prof_instructions[DOOM_PROF_COUNT];

/* The picture as the window last showed it, so that the rows that have not changed since, the
 * status bar's most of the time, are neither turned into colours nor copied again. */
static byte doom_previous[SCREENWIDTH * SCREENHEIGHT];
static int doom_previous_valid;         /* 0: every row is shown anew, after a resize, say */
static int doom_overlay_rows;           /* DOOM's rows under the overlays, shown anew each frame */

/* A row of the window's picture showing one of DOOM's, and the copies of it below. */
typedef struct {
    short y, copies, changed;
    const byte *from;
} DoomBand;

/*
 * The window's picture after a resize, and where DOOM's goes in it. The emulator counts
 * instructions, not pixels, so the picture is DOOM's own width or twice it, a 64-bit store making
 * two pixels: a window a few pixels narrower than 640 leaves out DOOM's edge columns rather than
 * scale by another factor, pixel by pixel. The rows are as many as fit at 4:3, each a copy of the
 * one above it when it shows the same row of DOOM's, which one rep movsb makes (copy_rect32).
 */
static void doom_layout(void) {
    doom_pixels = os_window_pixels(doom_window, &doom_client_w, &doom_client_h);
    doom_scale = doom_client_w >= 2 * SCREENWIDTH - 16 ? 2 : 1;
    doom_crop = doom_scale == 2 && doom_client_w < 2 * SCREENWIDTH
                    ? (2 * SCREENWIDTH - doom_client_w + 3) / 4 : 0;
    int w = (SCREENWIDTH - 2 * doom_crop) * doom_scale;
    int h = min_int(doom_client_h, SCREENWIDTH * doom_scale * 3 / 4);
    doom_view = rect((doom_client_w - w) / 2, (doom_client_h - h) / 2, w, h);
    doom_previous_valid = 0;
    os_window_draw(doom_window);
    fill_rect(0, 0, doom_client_w, doom_client_h, BLACK);
    os_window_update_all(doom_window);
}

void I_InitGraphics(void) {
    I_VideoBuffer = Z_Malloc(SCREENWIDTH * SCREENHEIGHT, PU_STATIC, NULL);
    screenvisible = true;
    doom_fps_since = os_now_ms();
}

void I_ShutdownGraphics(void) {
}

void I_SetPalette(byte *palette) {
    for (int i = 0; i < 256; i++, palette += 3) {
        doom_palette[i] = (uint32_t)gammatable[usegamma][palette[0]] << 16
                          | (uint32_t)gammatable[usegamma][palette[1]] << 8
                          | gammatable[usegamma][palette[2]];
        doom_palette2[i] = (uint64_t)doom_palette[i] << 32 | doom_palette[i];
    }
    doom_previous_valid = 0;    /* the same picture in other colours: a flash of red, say */
}

int I_GetPaletteIndex(int r, int g, int b) {
    int best = 0, best_difference = INT_MAX;
    for (int i = 0; i < 256; i++) {
        int dr = (int)(doom_palette[i] >> 16 & 0xff) - r;
        int dg = (int)(doom_palette[i] >> 8 & 0xff) - g;
        int db = (int)(doom_palette[i] & 0xff) - b;
        int difference = dr * dr + dg * dg + db * db;
        if (difference < best_difference) {
            best = i;
            best_difference = difference;
        }
        if (!difference) break;
    }
    return best;
}

/*
 * The overlays' lines: each drawn, text on black, into its own strip of doom_overlay_pixels only
 * when its text changes, about once a second, and copied over the picture every frame, a rep movsb
 * a row; drawing the text again every frame cost more than some of DOOM's own work.
 */
enum { DOOM_OVERLAY_LINES = 18, DOOM_OVERLAY_W = 2 * SCREENWIDTH };
static uint32_t doom_overlay_pixels[DOOM_OVERLAY_LINES * FONT_HEIGHT * DOOM_OVERLAY_W];
static char doom_overlay_drawn[DOOM_OVERLAY_LINES][160];   /* what each strip shows */
static uint32_t doom_overlay_colours[DOOM_OVERLAY_LINES];

/* Line `line` of the overlays at x, y of the window's picture, or right-aligned to x when x is
 * negative; answers how far down the picture it reaches. */
static int doom_overlay_line(int line, int x, int y, const char *text, uint32_t colour) {
    uint32_t *strip = doom_overlay_pixels + (size_t)line * FONT_HEIGHT * DOOM_OVERLAY_W;
    int w = min_int(text_width(text) + 6, DOOM_OVERLAY_W);
    if (strcmp(doom_overlay_drawn[line], text) || doom_overlay_colours[line] != colour) {
        GfxContext saved = gfx_save();
        gfx_target(strip, DOOM_OVERLAY_W, FONT_HEIGHT, DOOM_OVERLAY_W);
        fill_rect(0, 0, w, FONT_HEIGHT, BLACK);
        draw_text(3, 0, text, colour);
        gfx_restore(saved);
        snprintf(doom_overlay_drawn[line], sizeof doom_overlay_drawn[line], "%s", text);
        doom_overlay_colours[line] = colour;
    }
    if (x < 0) x = -x - w;
    w = min_int(w, doom_client_w - x);
    int h = min_int(FONT_HEIGHT, doom_client_h - y);
    if (w > 0 && h > 0)
        copy_rect32(doom_pixels + (size_t)y * doom_client_w + x, (size_t)doom_client_w * 4, strip,
                    DOOM_OVERLAY_W * 4, w, h);
    return y + FONT_HEIGHT + 1 - doom_view.y;
}

/* The frames a second over the picture's top right corner, white on black. */
static int doom_draw_fps(void) {
    return doom_overlay_line(0, -(doom_view.x + doom_view.w - 2), doom_view.y + 2, doom_fps_text,
                             0xffffff);
}

/* Frame Times' lines over the top left corner, green. */
static int doom_draw_times(void) {
    int covered = 0;
    for (int i = 0; i < 4; i++)
        covered = doom_overlay_line(1 + i, doom_view.x + 2, doom_view.y + 2 + i * FONT_HEIGHT,
                                    doom_times_text[i], 0x40ff40);
    return covered;
}

/*
 * The attract demo must play as it always has: a sum of where the player is, tic by tic, which
 * any change to the game's logic would change. G_Ticker calls it every tic of a level (g_game.c);
 * Frame Times shows the last whole demo's. A demo is also the same work every time it plays, so
 * the frames shown while it played, a second, measure the speed better than a moment of it does.
 */
static unsigned doom_demo_sum, doom_demo_tics, doom_demo_number, doom_demo_frames;
static unsigned doom_last_sum, doom_last_tics, doom_last_number, doom_last_tenths;
static long doom_demo_since;

void doom_demo_tic(void) {
    mobj_t *mo = players[consoleplayer].mo;
    if (!demoplayback || !mo) return;
    if (!doom_demo_tics) {
        doom_demo_number++;
        doom_demo_frames = 0;
        doom_demo_since = doom_now_ms;
    }
    doom_demo_sum = doom_demo_sum * 31 + ((unsigned)mo->x ^ (unsigned)mo->y * 7 ^ mo->angle
                                          ^ (unsigned)mo->z * 3 ^ (unsigned)mo->health);
    doom_demo_tics++;
}

static void doom_demo_check(void) {
    if (demoplayback && doom_demo_tics) doom_demo_frames++;
    if (demoplayback || !doom_demo_tics) return;
    long span = doom_now_ms - doom_demo_since;
    doom_last_tenths = span > 0 ? (unsigned)(doom_demo_frames * 10000L / span) : 0;
    doom_last_sum = doom_demo_sum;
    doom_last_tics = doom_demo_tics;
    doom_last_number = doom_demo_number;
    doom_demo_sum = doom_demo_tics = 0;
}

/*
 * Game > Profile: where DOOM spends its time, function by function. The kernel notes where the
 * timer finds DOOM, a hundred times a second (os_profile), and the program's own File, the Build
 * output os/main, says where each function starts: its ELF symbol table, read when Profile first
 * goes on. A sample counts for the last function starting at or before it. The overlay shows the
 * functions with the most samples since Profile went on, asmOS's and the C library's among them.
 */
enum { DOOM_PROFILE_LINES = 12 };
#define DOOM_PROGRAM "os/main"          /* the Build output of the Entry, os/main.c.asm */

typedef struct {
    uint64_t address;
    const char *name;                   /* in doom_elf's string table */
} DoomSymbol;

/* An ELF file's section header and symbol, Elf64_Shdr and Elf64_Sym */
typedef struct {
    uint32_t name, type;
    uint64_t flags, address, offset, size;
    uint32_t link, info;
    uint64_t align, entry_size;
} DoomElfSection;
typedef struct {
    uint32_t name;
    uint8_t info, other;
    uint16_t section;
    uint64_t value, size;
} DoomElfSymbol;
enum { DOOM_SHT_SYMTAB = 2, DOOM_SHF_EXECINSTR = 4 };

static char *doom_elf;                  /* os/main, read whole: the names are in it */
static DoomSymbol *doom_symbols;        /* the functions, by address */
static int doom_symbol_count;
static int *doom_profile_hits;          /* samples in each */
static long doom_profile_total, doom_profile_before;   /* all samples; those before every one */
static int doom_show_profile;
static char doom_profile_text[DOOM_PROFILE_LINES][64];

static int doom_symbol_order(const void *a, const void *b) {
    uint64_t x = ((const DoomSymbol *)a)->address, y = ((const DoomSymbol *)b)->address;
    return x < y ? -1 : x > y;
}

/* Whether a name in the code is a function's: not a label inside one, such as the L2 the
 * Generated assembly makes of GCC's .L2 or the .row of fill_rect32.row in os/blit.asm. A name
 * with a dot is a function only when what follows it is GCC's, as in R_Foo.part.0 or .cold. */
static int doom_function_name(const char *name) {
    if (!name[0]) return 0;
    if (name[0] == 'L' && name[1]) {
        const char *digit = name + 1;
        while (*digit >= '0' && *digit <= '9') digit++;
        if (!*digit) return 0;
    }
    for (const char *dot = strchr(name, '.'); dot; dot = strchr(dot + 1, '.')) {
        static const char *const gcc[] = { "part", "constprop", "isra", "cold", "lto_priv",
                                           "localalias" };
        const char *part = dot + 1;
        size_t length = strcspn(part, ".");
        int known = length > 0 && strspn(part, "0123456789") == length;
        for (size_t i = 0; i < sizeof gcc / sizeof *gcc && !known; i++)
            known = strlen(gcc[i]) == length && !strncmp(part, gcc[i], length);
        if (!known) return 0;
    }
    return 1;
}

/* The functions of the program, from its symbol table: 0, keeping none, when os/main cannot be
 * read as an ELF file that has one. */
static int doom_read_symbols(void) {
    long size;
    char *elf = os_read_file(DOOM_PROGRAM, &size);
    if (!elf) return 0;
    uint64_t at = 0;
    uint16_t entry_size = 0, count = 0;
    if (size >= 64 && !memcmp(elf, "\177ELF", 4)) {
        memcpy(&at, elf + 0x28, 8);
        memcpy(&entry_size, elf + 0x3a, 2);
        memcpy(&count, elf + 0x3c, 2);
    }
    const DoomElfSection *sections = (const DoomElfSection *)(elf + at), *table = NULL;
    if (entry_size == sizeof *sections && at + (uint64_t)count * entry_size <= (uint64_t)size)
        for (int i = 0; i < count && !table; i++)
            if (sections[i].type == DOOM_SHT_SYMTAB && sections[i].link < count)
                table = &sections[i];
    const DoomElfSection *strings = table ? &sections[table->link] : NULL;
    int total = table ? (int)(table->size / sizeof(DoomElfSymbol)) : 0;
    if (table && table->offset + table->size <= (uint64_t)size &&
        strings->offset + strings->size <= (uint64_t)size) {
        doom_symbols = malloc(total * sizeof *doom_symbols);
        doom_profile_hits = malloc(total * sizeof *doom_profile_hits);
    }
    if (!doom_symbols || !doom_profile_hits) {
        free(doom_symbols);
        free(doom_profile_hits);
        doom_symbols = NULL;
        doom_profile_hits = NULL;
        os_free(elf);
        return 0;
    }
    const DoomElfSymbol *symbols = (const DoomElfSymbol *)(elf + table->offset);
    doom_symbol_count = 0;
    for (int k = 0; k < total; k++) {
        const DoomElfSymbol *symbol = &symbols[k];
        if (symbol->section == 0 || symbol->section >= count ||
            !(sections[symbol->section].flags & DOOM_SHF_EXECINSTR) ||
            symbol->name >= strings->size)
            continue;
        const char *name = elf + strings->offset + symbol->name;
        if (doom_function_name(name))
            doom_symbols[doom_symbol_count++] = (DoomSymbol){ symbol->value, name };
    }
    qsort(doom_symbols, doom_symbol_count, sizeof *doom_symbols, doom_symbol_order);
    int kept = 0;               /* two names at one address, a function and its alias: once */
    for (int k = 0; k < doom_symbol_count; k++)
        if (!kept || doom_symbols[k].address != doom_symbols[kept - 1].address)
            doom_symbols[kept++] = doom_symbols[k];
    doom_symbol_count = kept;
    doom_elf = elf;             /* the names are in it */
    return 1;
}

static void doom_profile_start(void) {
    for (int i = 0; i < DOOM_PROFILE_LINES; i++) doom_profile_text[i][0] = 0;
    if (!doom_profile_hits && !doom_read_symbols()) {
        snprintf(doom_profile_text[0], sizeof doom_profile_text[0], "profile: no symbols in %s",
                 DOOM_PROGRAM);
        return;
    }
    memset(doom_profile_hits, 0, doom_symbol_count * sizeof *doom_profile_hits);
    doom_profile_total = doom_profile_before = 0;
    snprintf(doom_profile_text[0], sizeof doom_profile_text[0], "profiling...");
    os_profile(1);
}

/* The samples the kernel noted since the last time, counted to their functions. */
static void doom_profile_collect(void) {
    static uint64_t samples[4096];
    if (!doom_profile_hits) return;     /* no symbols: doom_profile_start said so */
    int count = os_profile_take(samples, 4096);
    for (int i = 0; i < count; i++) {
        int low = 0, high = doom_symbol_count - 1, found = -1;
        while (low <= high) {   /* the last function starting at or before the sample */
            int middle = (low + high) / 2;
            if (doom_symbols[middle].address <= samples[i]) {
                found = middle;
                low = middle + 1;
            } else {
                high = middle - 1;
            }
        }
        if (found < 0) doom_profile_before++;
        else doom_profile_hits[found]++;
        doom_profile_total++;
    }
    if (!doom_profile_total) return;
    snprintf(doom_profile_text[0], sizeof doom_profile_text[0], "profile: %ld samples%s",
             doom_profile_total, doom_profile_before ? ", some before any function" : "");
    /* the functions with the most samples, most first, in one pass */
    enum { TOP = DOOM_PROFILE_LINES - 1 };
    int top[TOP], count_top = 0;
    for (int i = 0; i < doom_symbol_count; i++) {
        int hits = doom_profile_hits[i];
        if (!hits || (count_top == TOP && hits <= doom_profile_hits[top[TOP - 1]])) continue;
        int at = count_top < TOP ? count_top++ : TOP - 1;
        while (at > 0 && doom_profile_hits[top[at - 1]] < hits) {
            top[at] = top[at - 1];
            at--;
        }
        top[at] = i;
    }
    for (int line = 1; line < DOOM_PROFILE_LINES; line++) {
        if (line > count_top) {
            doom_profile_text[line][0] = 0;
            continue;
        }
        int best = top[line - 1];
        long tenths = doom_profile_hits[best] * 1000L / doom_profile_total;
        snprintf(doom_profile_text[line], sizeof doom_profile_text[line], "%3ld.%ld%% %s",
                 tenths / 10, tenths % 10, doom_symbols[best].name);
    }
}

/* The profile's lines over the top left corner, below Frame Times' when those show, yellow. */
static int doom_draw_profile(int first_line) {
    int covered = 0;
    for (int i = 0; i < DOOM_PROFILE_LINES && doom_profile_text[i][0]; i++)
        covered = doom_overlay_line(5 + i, doom_view.x + 2,
                                    doom_view.y + 2 + (first_line + i) * FONT_HEIGHT,
                                    doom_profile_text[i], 0xffff40);
    return covered;
}

/* "12.34": millions of instructions in a second. */
static void doom_mips(char *text, size_t size, long instructions) {
    snprintf(text, size, "%ld.%02ld", instructions / 1000000, instructions / 10000 % 100);
}

/*
 * Frame Times' lines, of the second gone: the frame rate and how many instructions DOOM, the
 * system (the window manager) and all ran in it; then where a frame's milliseconds went, and its
 * thousands of instructions: L the game's logic, of which T the thinkers, and of those sight and
 * move; W walls, F floors, M sprites; O DOOM's other drawing, of which bar the status bar; S the
 * copy into the window, of which pal the palette and dbl the repeated rows; I messages and sound.
 */
static void doom_times_lines(long span) {
    long f = doom_frames, *p = doom_prof, *n = doom_prof_instructions;
    long view = p[DOOM_PROF_BSP] + p[DOOM_PROF_PLANES] + p[DOOM_PROF_MASKED];
    long nview = n[DOOM_PROF_BSP] + n[DOOM_PROF_PLANES] + n[DOOM_PROF_MASKED];
    ProcessInfo list[32];
    int count = min_int(os_processes(list, 32), 32), self = os_getpid(), busy;
    long mine = 0, system = 0, all;
    for (int i = 0; i < count; i++) {
        if (list[i].pid == self) mine = list[i].cpu;
        if (list[i].pid == 0) system = list[i].cpu;
    }
    os_cpu_load(&busy, &all);
    char a[48], b[48], c[48];
    doom_mips(a, sizeof a, all);
    doom_mips(b, sizeof b, mine);
    doom_mips(c, sizeof c, system);
    snprintf(doom_times_text[0], sizeof doom_times_text[0],
             "%s %ld tic/s  MIPS %s: DOOM %s system %s", doom_fps_text,
             (gametic - doom_tics_since) * 1000L / span, a, b, c);
    snprintf(doom_times_text[1], sizeof doom_times_text[1],
             "ms L%ld T%ld sight%ld move%ld W%ld F%ld M%ld O%ld bar%ld S%ld pal%ld dbl%ld I%ld",
             doom_ms_logic / f, p[DOOM_PROF_THINK] / f, p[DOOM_PROF_SIGHT] / f,
             p[DOOM_PROF_MOVE] / f, p[DOOM_PROF_BSP] / f, p[DOOM_PROF_PLANES] / f,
             p[DOOM_PROF_MASKED] / f, (doom_ms_draw - doom_ms_show - view) / f,
             p[DOOM_PROF_STBAR] / f, doom_ms_show / f, p[DOOM_PROF_CONVERT] / f,
             p[DOOM_PROF_DOUBLE] / f, p[DOOM_PROF_INPUT] / f);
    f *= 1000;      /* thousands of instructions */
    snprintf(doom_times_text[2], sizeof doom_times_text[2],
             "kI L%ld T%ld sight%ld move%ld W%ld F%ld M%ld O%ld bar%ld S%ld pal%ld dbl%ld I%ld",
             doom_in_logic / f, n[DOOM_PROF_THINK] / f, n[DOOM_PROF_SIGHT] / f,
             n[DOOM_PROF_MOVE] / f, n[DOOM_PROF_BSP] / f, n[DOOM_PROF_PLANES] / f,
             n[DOOM_PROF_MASKED] / f, (doom_in_draw - doom_in_show - nview) / f,
             n[DOOM_PROF_STBAR] / f, doom_in_show / f, n[DOOM_PROF_CONVERT] / f,
             n[DOOM_PROF_DOUBLE] / f, n[DOOM_PROF_INPUT] / f);
    if (doom_last_tics)
        snprintf(doom_times_text[3], sizeof doom_times_text[3],
                 "demo %u: %u tics, sum %08x, %u.%u fps", doom_last_number, doom_last_tics,
                 doom_last_sum, doom_last_tenths / 10, doom_last_tenths % 10);
    else
        snprintf(doom_times_text[3], sizeof doom_times_text[3], "demo: none ended yet");
}

static void doom_reset_counts(void) {
    doom_frames = 0;
    doom_fps_since = os_now_ms();
    doom_tics_since = gametic;
    doom_ms_logic = doom_ms_draw = doom_ms_show = 0;
    doom_in_logic = doom_in_draw = doom_in_show = 0;
    memset(doom_prof, 0, sizeof doom_prof);
    memset(doom_prof_instructions, 0, sizeof doom_prof_instructions);
}

/* The frame rate, and the frame times if they are shown, of the second since doom_fps_since. */
static void doom_count_frame(void) {
    long now = os_now_ms(), span = now - doom_fps_since;
    doom_frames++;
    doom_demo_check();
    if (span < 1000) return;
    long tenths = doom_frames * 10000L / span;
    snprintf(doom_fps_text, sizeof doom_fps_text, "%ld.%ld fps", tenths / 10, tenths % 10);
    if (doom_show_times) doom_times_lines(span);
    if (doom_show_profile) doom_profile_collect();
    doom_reset_counts();
}

/* Turns a row of DOOM's picture into the window's pixels, once, or twice as wide. */
static void doom_convert_row(uint32_t *to, const byte *from, int columns) {
    if (doom_scale == 2) {
        uint64_t *pair = (uint64_t *)to;
        if (doom_simd) doom_palette_row2(pair, from, columns, doom_palette2);
        else for (int x = 0; x < columns; x++) pair[x] = doom_palette2[from[x]];
    } else {
        if (doom_simd) doom_palette_row(to, from, columns, doom_palette);
        else for (int x = 0; x < columns; x++) to[x] = doom_palette[from[x]];
    }
}

/* Whether a row of DOOM's picture changed since the window last showed it; it is kept now. */
static int doom_row_differs(int row) {
    const byte *now = I_VideoBuffer + row * SCREENWIDTH;
    byte *before = doom_previous + row * SCREENWIDTH;
    if (doom_simd) return doom_row_changed(now, before, SCREENWIDTH);
    int changed = memcmp(now, before, SCREENWIDTH) != 0;
    if (changed) memcpy(before, now, SCREENWIDTH);
    return changed;
}

/*
 * DOOM's picture into the window: the rows that changed since the last frame, each turned into
 * colours once and then repeated below by rep movsb, and the window told of them run by run. The
 * rows under the overlays are shown anew every frame, since the overlays are drawn over them.
 */
void I_FinishUpdate(void) {
    static DoomBand bands[SCREENHEIGHT];
    long start = doom_now_ms = os_now_ms(), counted = doom_show_times ? os_instructions() : 0;
    int w = doom_view.w, h = doom_view.h, columns = SCREENWIDTH - 2 * doom_crop, count = 0;
    /* the 3D view's rows change nearly every frame of a level: comparing them would only cost, so
     * they are shown anyway, and the status bar's and the border's are compared */
    static int view_top_before = -1, view_end_before = -1;
    int view_top = viewwindowy, view_end = gamestate == GS_LEVEL ? viewwindowy + viewheight : 0;
    if (view_top != view_top_before || view_end != view_end_before) {
        doom_previous_valid = 0;    /* rows in and out of the view: compare against nothing old */
        view_top_before = view_top;
        view_end_before = view_end;
    }
    int always = doom_previous_valid ? doom_overlay_rows : SCREENHEIGHT;
    size_t stride = (size_t)doom_client_w * sizeof(uint32_t);
    uint32_t *top = doom_pixels + (size_t)doom_view.y * doom_client_w + doom_view.x;
    for (int y = 0; y < h; count++) {
        int row = y * SCREENHEIGHT / h, copies = 0;
        while (y + 1 + copies < h && (y + 1 + copies) * SCREENHEIGHT / h == row) copies++;
        int changed = row >= view_top && row < view_end ? 1 : doom_row_differs(row);
        bands[count] = (DoomBand){ (short)y, (short)copies, (short)(changed || row < always),
                                   I_VideoBuffer + row * SCREENWIDTH + doom_crop };
        y += 1 + copies;
    }
    doom_previous_valid = 1;
    doom_game_menu[4].flags = detailLevel ? MENU_CHECKED : 0;   /* F5 changes it too */
    DOOM_PROF(DOOM_PROF_CONVERT, {
        for (int i = 0; i < count; i++)
            if (bands[i].changed)
                doom_convert_row(top + (size_t)bands[i].y * doom_client_w, bands[i].from, columns);
    });
    DOOM_PROF(DOOM_PROF_DOUBLE, {
        for (int i = 0; i < count; i++) {
            uint32_t *to = top + (size_t)bands[i].y * doom_client_w;
            if (bands[i].changed && bands[i].copies)
                copy_rect32(to + doom_client_w, stride, to, 0, w, bands[i].copies);
        }
    });
    /* the overlays before the window hears of the rows, which it may show at once */
    int covered = 0;
    if (doom_show_fps) covered = doom_draw_fps();
    if (doom_show_times) covered = max_int(covered, doom_draw_times());
    if (doom_show_profile) covered = max_int(covered, doom_draw_profile(doom_show_times ? 4 : 0));
    doom_overlay_rows = covered ? covered * SCREENHEIGHT / h + 2 : 0;
    for (int i = 0; i < count;) {
        if (!bands[i].changed) {
            i++;
            continue;
        }
        int first = bands[i].y;
        while (i < count && bands[i].changed) i++;
        int end = i < count ? bands[i].y : h;
        os_window_update(doom_window, doom_view.x, doom_view.y + first, w, end - first);
    }
    doom_ms_show += os_now_ms() - start;
    if (doom_show_times) doom_in_show += os_instructions() - counted;
    doom_count_frame();
    I_SoundPump();
}

void I_UpdateNoBlit(void) {
}

void I_ReadScreen(byte *screen) {
    memcpy(screen, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT);
}

void I_BeginRead(void) {
}

void I_EndRead(void) {
}

void I_SetWindowTitle(char *title) {
    (void)title;
}

void I_GraphicsCheckCommandLine(void) {
}

void I_SetGrabMouseCallback(grabmouse_callback_t callback) {
    (void)callback;
}

void I_EnableLoadingDisk(void) {
}

void I_BindVideoVariables(void) {
}

void I_DisplayFPSDots(boolean dots_on) {
    (void)dots_on;
}

void I_CheckIsScreensaver(void) {
}

void I_InitWindowTitle(void) {
}

void I_InitWindowIcon(void) {
}

/* ------------------------------------------------------------------ the keys (i_input.c) */

int vanilla_keyboard_mapping = 1;

static boolean doom_held[256];     /* DOOM's keys down, to let go of when the window loses them */
static byte doom_pressed[256];     /* DOOM's key each Linux key went down as, to let go of the same */

/*
 * W, A, S and D move and strafe, E opens and F fires, but only while playing: in DOOM's menu they
 * are letters, a saved game's name among them, and on the map F follows the player. The cheats
 * read the character typed, which goes with the key all the same.
 */
static int doom_play_key(int ch) {
    if (gamestate != GS_LEVEL || menuactive) return 0;
    switch (ch | 0x20) {
    case 'w': return KEY_UPARROW;
    case 's': return KEY_DOWNARROW;
    case 'a': return KEY_STRAFE_L;
    case 'd': return KEY_STRAFE_R;
    case 'e': return KEY_USE;
    case 'f': return automapactive ? 0 : KEY_FIRE;
    }
    return 0;
}

/* DOOM's code for a Linux key, or 0 for one DOOM has no use for. */
static int doom_key(int key, int ch) {
    int play = ch > 0 && ch < 127 ? doom_play_key(ch) : 0;
    if (play) return play;
    switch (key) {
    case LK_ESC: return KEY_ESCAPE;
    case LK_ENTER: case LK_KPENTER: return KEY_ENTER;
    case LK_TAB: return KEY_TAB;
    case LK_BACKSPACE: return KEY_BACKSPACE;
    case LK_UP: case LK_KP8: return KEY_UPARROW;
    case LK_DOWN: case LK_KP2: return KEY_DOWNARROW;
    case LK_LEFT: case LK_KP4: return KEY_LEFTARROW;
    case LK_RIGHT: case LK_KP6: return KEY_RIGHTARROW;
    case LK_LEFTCTRL: case LK_RIGHTCTRL: return KEY_FIRE;
    case LK_SPACE: return KEY_USE;
    case LK_LEFTSHIFT: case LK_RIGHTSHIFT: return KEY_RSHIFT;
    case LK_LEFTALT: case LK_RIGHTALT: return KEY_RALT;
    case LK_COMMA: return KEY_STRAFE_L;
    case LK_DOT: return KEY_STRAFE_R;
    case LK_F11: return KEY_F11;
    case LK_F12: return KEY_F12;
    case LK_PAUSE: return KEY_PAUSE;
    case LK_CAPSLOCK: return KEY_CAPSLOCK;
    case LK_NUMLOCK: return KEY_NUMLOCK;
    case LK_SCROLLLOCK: return KEY_SCRLCK;
    case LK_HOME: case LK_KP7: return KEY_HOME;
    case LK_END: case LK_KP1: return KEY_END;
    case LK_PAGEUP: case LK_KP9: return KEY_PGUP;
    case LK_PAGEDOWN: case LK_KP3: return KEY_PGDN;
    case LK_INSERT: case LK_KP0: return KEY_INS;
    case LK_DELETE: case LK_KPDOT: return KEY_DEL;
    case LK_KPMINUS: case LK_MINUS: return KEY_MINUS;
    case LK_KPPLUS: case LK_EQUAL: return KEY_EQUALS;
    case LK_KPASTERISK: return '*';
    case LK_KPSLASH: return '/';
    case LK_KP5: return '5';
    }
    if (key >= LK_F1 && key <= LK_F10) return KEY_F1 + (key - LK_F1);
    if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 'a';
    if (ch > ' ' && ch < 127) return ch;
    return 0;
}

static void doom_post_key(int down, int key, int ch) {
    if (!key) return;
    event_t event = { 0 };
    event.type = down ? ev_keydown : ev_keyup;
    event.data1 = key;
    /* the character typed, for a saved game's name and the cheats, which read it */
    event.data2 = down ? (ch > 0 && ch < 127 ? ch : key < 128 ? key : 0) : 0;
    doom_held[key & 0xff] = down;
    D_PostEvent(&event);
}

/* Every key DOOM thinks is held, let go: the window lost the keyboard and will not see them up. */
static void doom_release_keys(void) {
    for (int key = 0; key < 256; key++)
        if (doom_held[key]) doom_post_key(0, key, 0);
    memset(doom_pressed, 0, sizeof doom_pressed);
}

/*
 * Game > Benchmark CPU: what each kind of instruction costs in the emulator, which charges
 * neither as a real processor does nor all kinds alike. asmos_bench.asm has a loop for each kind,
 * ten instructions a turn, eight of them of that kind, and for the ways of copying memory; each
 * runs about a fifth of a second by the clock, and the answer is shown in a box. It found that
 * the kernel's interval timer, while set, made every instruction cost about ten times as much:
 * the emulator looked at the host's clock before each one, and since November 2026 it looks
 * once in a thousand.
 */
void bench_int(long n, void *buf);
void bench_mul(long n, void *buf);
void bench_div(long n, void *buf);
void bench_load(long n, void *buf);
void bench_store(long n, void *buf);
void bench_scatter(long n, void *buf);
void bench_branch(long n, void *buf);
void bench_call(long n, void *buf);
void bench_sse(long n, void *buf);
void bench_avx(long n, void *buf);
void bench_pextrb(long n, void *buf);
void bench_gather(long n, void *buf);
void bench_rep(long n, void *buf);
void bench_movsb(long n, void *buf);
void bench_movsq(long n, void *buf);
void bench_stosd(long n, void *buf);
void bench_avxcopy(long n, void *buf);

static void bench_clock(long n, void *buf) {    /* a system call: what os_now_ms costs */
    (void)buf;
    for (long i = 0; i < n; i++) os_now_ms();
}

/* One kind, run for about a fifth of a second: the nanoseconds a turn took. */
static long bench_measure(void (*run)(long, void *), void *buf) {
    long n = 100, ms = 0;
    for (;;) {                  /* longer until it runs a fifth of a second */
        long start = os_now_ms();
        run(n, buf);
        ms = os_now_ms() - start;
        if (ms >= 200 || n > (1L << 40)) break;
        n = ms < 10 ? n * 16 : n * 220 / ms;
    }
    return ms * 1000000L / n;
}

static void doom_benchmark(void) {
    /* a turn: ten instructions, eight of the kind; or two copies or fills of 2560 bytes */
    static const struct { const char *name; void (*run)(long, void *); int copies; } kinds[] = {
        { "add/xor", bench_int, 0 },     { "imul", bench_mul, 0 },      { "div", bench_div, 0 },
        { "load", bench_load, 0 },       { "store", bench_store, 0 },   { "column", bench_scatter, 0 },
        { "call/ret", bench_call, 0 },   { "AVX add", bench_avx, 0 },   { "vpextrb", bench_pextrb, 0 },
        { "gather", bench_gather, 0 },   { "os_now_ms", bench_clock, 0 },
        { "rep movsd", bench_rep, 1 },   { "rep movsb", bench_movsb, 1 }, { "rep movsq", bench_movsq, 1 },
        { "rep stosd", bench_stosd, 1 }, { "ymm copy", bench_avxcopy, 1 },
    };
    static uint8_t buf[65536];
    char text[1600];
    int used = snprintf(text, sizeof text,
                        "ns a turn of 10 instructions, 8 of the kind;\n"
                        "copies and fills in MB/s\n");
    for (unsigned k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
        long ns = bench_measure(kinds[k].run, buf);
        if (ns < 1) ns = 1;
        if (kinds[k].copies)
            used += snprintf(text + used, sizeof text - used, "%-9s %5ld MB/s%s", kinds[k].name,
                             5120L * 1000 / ns, k & 1 ? "\n" : "  ");
        else
            used += snprintf(text + used, sizeof text - used, "%-9s %5ld ns  %s", kinds[k].name, ns,
                             k & 1 ? "\n" : "  ");
        if (used >= (int)sizeof text) break;
    }
    doom_previous_valid = 0;
    os_message_box(doom_window, "Benchmark CPU", text, MB_OK, MB_INFO);
}

static void doom_command(int id) {
    switch (id) {
    case DOOM_MENU_MENU:
        if (!menuactive) M_StartControlPanel();
        break;
    case DOOM_MENU_FPS:
        doom_show_fps = !doom_show_fps;
        doom_game_menu[1].flags = doom_show_fps ? MENU_CHECKED : 0;
        doom_previous_valid = 0;    /* every row again, to take the counter away */
        break;
    case DOOM_MENU_TIMES:
        doom_show_times = !doom_show_times;
        doom_game_menu[2].flags = doom_show_times ? MENU_CHECKED : 0;
        snprintf(doom_times_text[0], sizeof doom_times_text[0], "measuring...");
        for (int i = 1; i < 4; i++) doom_times_text[i][0] = 0;
        doom_reset_counts();
        doom_previous_valid = 0;
        break;
    case DOOM_MENU_SIMD:
        doom_simd = !doom_simd;
        doom_game_menu[3].flags = doom_simd ? MENU_CHECKED : 0;
        break;
    case DOOM_MENU_DETAIL:          /* as F5 does: its check follows in I_FinishUpdate */
        M_ChangeDetail(0);
        break;
    case DOOM_MENU_PROFILE:
        doom_show_profile = !doom_show_profile;
        doom_game_menu[5].flags = doom_show_profile ? MENU_CHECKED : 0;
        if (doom_show_profile) doom_profile_start();
        else os_profile(0);
        doom_previous_valid = 0;
        break;
    case DOOM_MENU_BENCH:
        doom_release_keys();
        doom_benchmark();
        break;
    case DOOM_MENU_EXIT: I_Quit(); break;
    case DOOM_MENU_KEYS:
        doom_release_keys();
        os_message_box(doom_window, "DOOM keys",
                       "Move: W and S or the up and down arrows.  Turn: left and right.\n"
                       "Strafe: A and D, , and . or Alt with the arrows.  Run: Shift.\n"
                       "Fire: F, the left mouse button or Ctrl.  Open, use: E or Space.\n"
                       "Weapons: 1 to 7.  Map: Tab.  Menu: Esc.\n"
                       "Save and load: F2 and F3.  Screen size: - and =.  Detail: F5.\n"
                       "In the browser Ctrl+W closes the tab: fire with F or the mouse.",
                       MB_OK, MB_INFO);
        break;
    case DOOM_MENU_ABOUT:
        doom_release_keys();
        os_message_box(doom_window, "About DOOM",
                       "DOOM, by id Software (1993), through doomgeneric, the portable\n"
                       "Chocolate Doom by Simon Howard and others, under the GNU GPL 2.\n"
                       "The game's levels, pictures and sounds are its IWAD's.",
                       MB_OK, MB_INFO);
        break;
    }
}

/* Every message waiting for DOOM's window, as DOOM's events. */
static void doom_messages(void) {
    Message m;
    while (os_peek_message(&m)) {
        switch (m.type) {
        case MSG_KEY: {
            int key = doom_key(m.key, m.ch);
            /* a key held down repeats: one that went down as another of DOOM's lets that go */
            if (doom_pressed[m.key & 0xff] && doom_pressed[m.key & 0xff] != key)
                doom_post_key(0, doom_pressed[m.key & 0xff], 0);
            doom_pressed[m.key & 0xff] = (byte)key;
            doom_post_key(1, key, m.ch);
            break;
        }
        case MSG_KEY_UP:
            doom_post_key(0, doom_pressed[m.key & 0xff], 0);
            doom_pressed[m.key & 0xff] = 0;
            break;
        case MSG_MOUSE_DOWN:
            /* only in a level: a click that gives the window the keyboard is no key for the menu */
            if (m.button == BUTTON_LEFT && gamestate == GS_LEVEL && !menuactive && !demoplayback)
                doom_post_key(1, KEY_FIRE, 0);
            break;
        case MSG_MOUSE_UP:
            if (m.button == BUTTON_LEFT && doom_held[KEY_FIRE]) doom_post_key(0, KEY_FIRE, 0);
            break;
        case MSG_BLUR: doom_release_keys(); break;
        case MSG_RESIZE: doom_layout(); break;
        case MSG_COMMAND: doom_command(m.id); break;
        case MSG_CLOSE: case MSG_QUIT: I_Quit(); break;
        default: os_default_message(&m);
        }
    }
}

void I_StartFrame(void) {
    doom_messages();
}

/* DOOM calls it between the renderer's phases too, through NetUpdate */
void I_StartTic(void) {
    if (!doom_show_times) {
        doom_messages();
        I_SoundPump();
        return;
    }
    DOOM_PROF(DOOM_PROF_INPUT, doom_messages(); I_SoundPump());
}

void I_InitInput(void) {
}

/* ------------------------------------------------------------------ the process */

/* The IWAD to play: the argument, a .wad file opened with DOOM, or the first of games/doom/'s. */
static int doom_find_iwad(const char *argument, char *path, int size) {
    static const char *const names[] = {
        "doom1.wad", "doom.wad", "doomu.wad", "doom2.wad", "plutonia.wad", "tnt.wad",
        "freedoom1.wad", "freedoom2.wad", "freedm.wad",
    };
    if (argument && *argument && os_stat(argument, NULL) == 1) {
        snprintf(path, size, "%s", argument);
        return 1;
    }
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        snprintf(path, size, "games/doom/%s", names[i]);
        if (os_stat(path, NULL) == 1) return 1;
    }
    return 0;
}

/* Whether a DOOM that started before this one is still running. */
static int doom_running_already(void) {
    ProcessInfo list[32];
    int count = min_int(os_processes(list, 32), 32), self = os_getpid();
    for (int i = 0; i < count; i++)
        if (list[i].pid < self && strcmp(list[i].name, "DOOM") == 0) return 1;
    return 0;
}

static void doom_loading(const char *text) {
    os_window_draw(doom_window);
    fill_rect(0, 0, doom_client_w, doom_client_h, BLACK);
    draw_text((doom_client_w - text_width(text)) / 2, doom_client_h / 2 - FONT_HEIGHT / 2, text,
              0xc0c0c0);
    os_window_update_all(doom_window);
}

/* os/doom/asmos_fresh.asm: DOOM's variables as they were when asmOS started, so that a second
 * game in the same asmOS does not begin with what the first left in them */
void doom_fresh(void);

/* d_main.c and m_argv.c, which declare them nowhere */
void D_DoomMain(void);
void D_Display(void);
void M_FindResponseFile(void);

/* One frame: doomgeneric_Tick's, with the time each part takes for the Frame Rate. */
static void doom_frame(void) {
    I_StartFrame();
    int timed = doom_show_times;
    long start = os_now_ms(), counted = timed ? os_instructions() : 0;
    TryRunTics();
    long ran = os_now_ms(), ran_count = timed ? os_instructions() : 0;
    S_UpdateSounds(players[consoleplayer].mo);
    if (screenvisible) D_Display();
    doom_ms_logic += ran - start;
    doom_ms_draw += os_now_ms() - ran;
    if (timed && doom_show_times) {     /* not if Frame Times went on in the middle */
        doom_in_logic += ran_count - counted;
        doom_in_draw += os_instructions() - ran_count;
    }
}

void doom_main(const char *argument) {
    static char iwad[128];
    static char *args[] = { "doom", "-iwad", iwad, NULL };
    if (doom_running_already()) {
        os_message_box(NULL, "DOOM", "DOOM is running already: there can be only one game at a time.",
                       MB_OK, MB_INFO);
        os_exit(0);
    }
    /* only now, with no other DOOM using them: its variables as they were at the start */
    doom_fresh();
    if (!doom_find_iwad(argument, iwad, sizeof iwad)) {
        os_message_box(NULL, "DOOM",
                       "DOOM needs the game's data, an IWAD file.\n"
                       "Put doom1.wad (the shareware episode) or another IWAD,\n"
                       "such as freedoom1.wad, in the Project's games/doom folder.",
                       MB_OK, MB_WARNING);
        os_exit(1);
    }
    doom_window = os_window_create("DOOM", &ICON_DOOM, DOOM_MIN_W, DOOM_MIN_H,
                                   WIN_RESIZABLE | WIN_CENTER);
    if (!doom_window) os_exit(1);
    os_window_set_menu(doom_window, doom_menus);
    os_window_min_size(doom_window, DOOM_MIN_W, DOOM_MIN_H);
    doom_layout();
    doom_loading("Loading DOOM...");

    myargc = 3;
    myargv = args;
    M_FindResponseFile();
    D_DoomMain();
    for (;;) doom_frame();
}
