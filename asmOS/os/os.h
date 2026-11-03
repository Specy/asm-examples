/*
 * asmOS's system interface: what the kernel (os/main.c and the Files it includes) offers the
 * applications (os/apps/apps.c and the Files it includes, and DOOM, os/doom/). They are compiled
 * apart, each into its own Generated assembly, and linked together by name, so everything here is
 * an ordinary function or object of one side that the other calls.
 *
 * An application is a process: its `..._main` function runs on a stack of its own, and the kernel
 * switches between processes, when one waits for a message or when the timer says its turn is
 * over. A process must therefore keep to what can be interrupted at any instruction: its own
 * variables, the drawing calls, the pure parts of the C library (string.h, ctype.h, math.h,
 * snprintf, sscanf, strtol, atoi, qsort, abs), and the os_ calls below for everything else. In
 * particular it never calls malloc, free, printf, fopen, opendir, time or gmtime itself: os_alloc,
 * os_read_file, os_list_dir and os_time do those safely.
 */
#ifndef ASMOS_OS_H
#define ASMOS_OS_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ geometry */

typedef struct {
    int x, y, w, h;
} Rect;

static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline int max_int(int a, int b) { return a > b ? a : b; }
static inline int clamp_int(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static inline Rect rect(int x, int y, int w, int h) {
    Rect r = { x, y, w, h };
    return r;
}
static inline int rect_empty(Rect r) { return r.w <= 0 || r.h <= 0; }
static inline int rect_contains(Rect r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
static inline Rect rect_intersect(Rect a, Rect b) {
    int x0 = max_int(a.x, b.x), y0 = max_int(a.y, b.y);
    int x1 = min_int(a.x + a.w, b.x + b.w), y1 = min_int(a.y + a.h, b.y + b.h);
    return rect(x0, y0, x1 - x0, y1 - y0);
}
static inline Rect rect_union(Rect a, Rect b) {
    if (rect_empty(a)) return b;
    if (rect_empty(b)) return a;
    int x0 = min_int(a.x, b.x), y0 = min_int(a.y, b.y);
    int x1 = max_int(a.x + a.w, b.x + b.w), y1 = max_int(a.y + a.h, b.y + b.h);
    return rect(x0, y0, x1 - x0, y1 - y0);
}
static inline int rect_overlaps(Rect a, Rect b) { return !rect_empty(rect_intersect(a, b)); }

/* ------------------------------------------------------------------ numbers in bytes */

/* Numbers in a file's bytes, read a byte at a time so that the processor's own byte order never
 * matters: BMP files and the videos keep theirs little-endian, MIDI files big-endian. */
static inline uint32_t read_le16(const void *at) {
    const uint8_t *b = at;
    return (uint32_t)b[0] | (uint32_t)b[1] << 8;
}
static inline uint32_t read_le32(const void *at) {
    const uint8_t *b = at;
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}
static inline uint32_t read_be16(const void *at) {
    const uint8_t *b = at;
    return (uint32_t)b[0] << 8 | (uint32_t)b[1];
}
static inline uint32_t read_be32(const void *at) {
    const uint8_t *b = at;
    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | (uint32_t)b[3];
}
static inline void write_le16(void *at, uint32_t value) {
    uint8_t *b = at;
    b[0] = (uint8_t)value;
    b[1] = (uint8_t)(value >> 8);
}
static inline void write_le32(void *at, uint32_t value) {
    uint8_t *b = at;
    for (int i = 0; i < 4; i++) b[i] = (uint8_t)(value >> 8 * i);
}

/* ------------------------------------------------------------------ fast pixel loops */

/* The platform's own where it has faster ones, as x86's `rep` instructions are
 * (os/platform/x86/blit.asm); os/blit.c's C loops elsewhere. */
void fill_rect32(uint32_t *to, size_t stride, size_t width, size_t height, uint32_t colour);
void copy_rect32(uint32_t *to, size_t to_stride, const uint32_t *from, size_t from_stride,
                 size_t width, size_t height);
void move_rect32(uint32_t *to, const uint32_t *from, size_t stride, size_t width, size_t height);

/* ------------------------------------------------------------------ colours */

enum {
    BLACK = 0x000000,
    WHITE = 0xffffff,
    FACE = 0xc0c0c0,      /* buttons, window frames, the taskbar */
    LIGHT = 0xdfdfdf,
    SHADOW = 0x808080,
    DESKTOP = 0x008080,
    NAVY = 0x000080,      /* selections */
    TOOLTIP = 0xffffe1,
    RED = 0xff0000,
    GREEN = 0x008000,
    BLUE = 0x0000ff,
    YELLOW = 0xffff00
};

/* ------------------------------------------------------------------ drawing (gfx.c) */

enum { FONT_WIDTH = 8, FONT_HEIGHT = 13 };
/* The symbols the font has past ASCII, as char codes. */
enum {
    CH_LEFT = 127, CH_UP, CH_RIGHT, CH_DOWN, CH_DEGREE, CH_MIDDOT, CH_TIMES, CH_DIVIDE, CH_BULLET
};

/* A 16 by 16 picture, one character per pixel: see icons.c for which colour each is. */
typedef struct {
    const char *rows[16];
} Icon;

/*
 * Where the drawing calls draw: a picture, the part of it they may touch (in its own pixels) and
 * what x 0, y 0 means. Every process has one of its own, which the kernel switches with the
 * process, so two processes never draw through each other's.
 */
typedef struct {
    uint32_t *pixels;
    int w, h, stride;     /* the stride in pixels */
    Rect clip;
    int origin_x, origin_y;
} GfxContext;

extern int screen_w, screen_h;

void gfx_target(uint32_t *pixels, int width, int height, int stride);
void gfx_clip(Rect clip);          /* in the picture's pixels */
void gfx_origin(int x, int y);     /* what x 0, y 0 means for the calls below */
Rect gfx_get_clip(void);
void gfx_get_origin(int *x, int *y);
GfxContext gfx_save(void);
void gfx_restore(GfxContext state);

void fill_rect(int x, int y, int w, int h, uint32_t colour);
void put_pixel(int x, int y, uint32_t colour);
uint32_t get_pixel(int x, int y);  /* 0 outside the clip */
void hline(int x, int y, int w, uint32_t colour);
void vline(int x, int y, int h, uint32_t colour);
void frame_rect(int x, int y, int w, int h, uint32_t colour);
void draw_line(int x0, int y0, int x1, int y1, uint32_t colour);
void thick_line(int x0, int y0, int x1, int y1, int width, uint32_t colour);
void fill_circle(int cx, int cy, int radius, uint32_t colour);
void draw_circle(int cx, int cy, int radius, uint32_t colour);
void blit(const uint32_t *pixels, int stride, int x, int y, int w, int h);
/* A w by h picture drawn scaled into a dw by dh rectangle, nearest pixel. */
void blit_scaled(const uint32_t *pixels, int stride, int w, int h, int x, int y, int dw, int dh);
/* A picture whose pixels of colour `key` are transparent. */
void blit_keyed(const uint32_t *pixels, int stride, int x, int y, int w, int h, uint32_t key);
void gradient_rect(int x, int y, int w, int h, uint32_t left, uint32_t right);

void bevel_raised(int x, int y, int w, int h);   /* a button */
void bevel_sunken(int x, int y, int w, int h);   /* a text field */
void bevel_frame(int x, int y, int w, int h);    /* a window */
void bevel_thin_sunken(int x, int y, int w, int h);
void bevel_thin_raised(int x, int y, int w, int h);

void draw_char(int x, int y, int ch, uint32_t colour);
void draw_text(int x, int y, const char *text, uint32_t colour);
void draw_text_bold(int x, int y, const char *text, uint32_t colour);
void draw_text_n(int x, int y, const char *text, int count, uint32_t colour);
void draw_text_up(int x, int y, const char *text, uint32_t colour);  /* rotated, bottom to top */
int text_width(const char *text);
void draw_icon(const Icon *icon, int x, int y, int scale);
/* A button: raised, or pushed in while pressed, with its label centred. */
void draw_button(int x, int y, int w, int h, const char *label, int pressed);
/* A white field sunk into the face, as text boxes and lists are. */
void draw_field(int x, int y, int w, int h, uint32_t background);
/* A check box with its label; a radio button with its label. */
void draw_check(int x, int y, const char *label, int checked);
void draw_radio(int x, int y, const char *label, int checked);

/* The icons (icons.c). */
extern const Icon ICON_NOTEPAD, ICON_CALCULATOR, ICON_PAINT, ICON_MINES, ICON_CLOCK,
    ICON_TERMINAL, ICON_FOLDER, ICON_COMPUTER, ICON_CHIP, ICON_PICTURE, ICON_SHUTDOWN,
    ICON_PACMAN, ICON_BLOCKS, ICON_DOOM, ICON_VIEWER, ICON_MUSIC, ICON_VIDEO, ICON_HEX, ICON_IDE, ICON_TASKS, ICON_SETTINGS,
    ICON_FILE, ICON_WARNING, ICON_INFO, ICON_QUESTION;

/* ------------------------------------------------------------------ processes (kernel.c) */

/* Starts application `app` (APP_ below, or one found by name with os_app_named) with an
 * argument, such as a file to open, which may be NULL. Answers its pid, or -1. */
int os_spawn(int app, const char *argument);
/* Ends the calling process: its windows close and its memory is freed. */
void os_exit(int code) __attribute__((noreturn));
/* Ends process `pid` the same way, as Task Manager does; 0 when done, -1 for no such process. */
int os_kill(int pid);
int os_getpid(void);
/* Gives the processor to the other processes until the next turn. */
void os_yield(void);
/*
 * A point in a long loop where the process lets the others run if its turn is over. A machine
 * whose timer preempts processes takes the processor from them anyway, and this does nothing; on
 * one without (a cooperative machine), a process that never gets to a yield point, a wait for a
 * message or a sleep holds the desktop up, the pointer included. os_peek_message and
 * os_get_message are yield points too.
 */
void os_yield_point(void);
/* Waits `ms` milliseconds; messages keep arriving meanwhile and wait in the queue. */
void os_sleep(int ms);
/* Milliseconds since asmOS started, and the calendar time (UTC). */
long os_now_ms(void);
typedef struct {
    int year, month, day, weekday, hour, minute, second;   /* month 1-12, weekday 0 = Sunday */
} DateTime;
void os_time(DateTime *now);

/* What Task Manager shows of a process. */
enum { PROC_READY, PROC_RUNNING, PROC_WAITING, PROC_SLEEPING, PROC_ENDED };
typedef struct {
    int pid;
    char name[32];
    int state;
    long memory;           /* bytes allocated with os_alloc */
    long cpu;              /* instructions it ran in the last second */
    int windows;
} ProcessInfo;
/* Fills `list` with up to `max` processes; answers how many there are. */
int os_processes(ProcessInfo *list, int max);
/* How busy the processor was in the last second, in percent of the time it was not waiting for
 * input, and how many instructions all processes ran in it (each ProcessInfo has its own share). */
void os_cpu_load(int *busy_percent, long *instructions);
/* The instructions the calling process has run since it started, to the moment: the emulator's
 * own count, so the difference of two is what the code between them cost. */
uint64_t os_instructions(void);
/* A profiler: while on, every tick of the timer that finds the calling process running notes the
 * address of the instruction it was at; os_profile_take hands over the addresses noted since it
 * was last called, up to `max`, and answers how many. */
void os_profile(int on);
int os_profile_take(uint64_t *addresses, int max);

/* ------------------------------------------------------------------ memory */

/* Zeroed memory that belongs to the calling process and is freed when it ends. */
void *os_alloc(size_t size);
void *os_realloc(void *memory, size_t size);
void os_free(void *memory);

/* ------------------------------------------------------------------ files */

/* The Project is the working directory: a path is relative to it, "notes.txt" or
 * "pictures/sunset.bmp". */
typedef struct {
    char name[64];
    int is_dir;
    long size;
} DirEntry;
/* Reads a whole file into os_alloc'd memory, with a 0 after it; NULL if it cannot. */
char *os_read_file(const char *path, long *size);
/* Writes a whole file, replacing it; 0 when done, -1 when not. */
int os_write_file(const char *path, const void *data, long size);
/* Up to `max` entries of the directory (folders first, then by name); answers how many, or -1. */
int os_list_dir(const char *path, DirEntry *entries, int max);
/* 1 for a file, 2 for a folder, 0 for nothing there; `size` gets the file's size. */
int os_stat(const char *path, long *size);
int os_delete(const char *path);              /* a file, or an empty folder */
int os_rename(const char *from, const char *to);
int os_make_dir(const char *path);
int os_copy_file(const char *from, const char *to);
/* What this machine's file system can do besides reading and writing whole files: not every one
 * can list a folder, make one, delete or rename. The calls it cannot answer -1. */
enum { FS_LIST = 1, FS_MAKE_DIR = 2, FS_DELETE = 4, FS_RENAME = 8 };
int os_file_system(void);
/* The name after the last slash. */
const char *os_base_name(const char *path);
/* An uncompressed 24- or 32-bit BMP file's picture, top row first, in os_alloc'd memory; NULL
 * for anything else. And a picture as a 24-bit BMP file, in os_alloc'd memory. */
uint32_t *os_bmp_decode(const char *data, long size, int *w, int *h);
char *os_bmp_encode(const uint32_t *pixels, int w, int h, long *size);
/* The application that opens a file of this name, from its extension, or -1. */
int os_app_for_file(const char *path);

/* ------------------------------------------------------------------ windows and messages */

typedef struct Window Window;

enum {
    WIN_RESIZABLE = 1,     /* the borders resize it and it can be maximized */
    WIN_NO_MINIMIZE = 2,
    WIN_CENTER = 4,        /* placed in the middle of the screen rather than cascaded */
};

/*
 * Opens a window whose client area, the part the application draws, is `w` by `h` pixels. The
 * kernel draws the frame, the title bar and the menu bar; the application draws the client area
 * into the window's own picture and calls os_window_update to show what it drew.
 */
Window *os_window_create(const char *title, const Icon *icon, int w, int h, int flags);
void os_window_close(Window *win);
void os_window_set_title(Window *win, const char *title);
/* Moves the window's frame, title bar included, so that its top left corner is at x, y on the
 * screen, as far as the screen allows. */
void os_window_move(Window *win, int x, int y);
/* The window's client-area picture and its size, which a resize changes (see MSG_RESIZE). */
uint32_t *os_window_pixels(Window *win, int *w, int *h);
/* Makes the drawing calls draw into the window's client area, clipped to it, from 0, 0. */
void os_window_draw(Window *win);
/* Shows what was drawn in a rectangle of the client area, or in all of it. */
void os_window_update(Window *win, int x, int y, int w, int h);
void os_window_update_all(Window *win);
int os_window_focused(Window *win);
void os_window_focus(Window *win);
/* The smallest size a resize may give the client area. */
void os_window_min_size(Window *win, int w, int h);
/* Where the window's client area is on the screen, for a popup menu placed beside something. */
Rect os_window_client_rect(Window *win);

/*
 * A menu: an array of items ended by one with a NULL label. An item with label "-" is a
 * separator; one with `submenu` opens it to the side. `id` comes back in MSG_COMMAND, or from
 * os_popup_menu. A menu bar is an array of menus ended by one with a NULL title.
 */
enum { MENU_DISABLED = 1, MENU_CHECKED = 2 };
typedef struct MenuItem MenuItem;
struct MenuItem {
    const char *label;
    int id;
    int flags;
    const char *shortcut;     /* shown on the right, such as "Ctrl+S"; the keys are the app's */
    const MenuItem *submenu;
};
typedef struct {
    const char *title;
    MenuItem *items;          /* not const: an application may change flags, as checks */
} Menu;
void os_window_set_menu(Window *win, const Menu *menus);
/* Opens a menu at a point of the window's client area and waits for a choice: its id, or 0. */
int os_popup_menu(Window *win, int x, int y, const MenuItem *items);

enum {
    MSG_NONE,
    MSG_MOUSE_DOWN, MSG_MOUSE_UP, MSG_MOUSE_MOVE, MSG_MOUSE_WHEEL,
    MSG_KEY,              /* a key pressed or repeating */
    MSG_KEY_UP,           /* a key released, for games */
    MSG_COMMAND,          /* a menu item chosen: `id` */
    MSG_TIMER,            /* a timer of os_set_timer came due: `id` */
    MSG_RESIZE,           /* the client area is now `w` by `h`: draw it again */
    MSG_CLOSE,            /* the close button: os_window_close, or ask first */
    MSG_DROP,             /* a file dropped on the window: `text` is its path, x and y where */
    MSG_FOCUS,            /* the window gained the keyboard */
    MSG_BLUR,             /* or lost it */
    MSG_SETTINGS,         /* the Settings changed: read os_settings() again */
    MSG_QUIT              /* asmOS is shutting down */
};
enum { BUTTON_LEFT = 1, BUTTON_RIGHT = 2, BUTTON_MIDDLE = 4 };
enum { MOD_SHIFT = 1, MOD_CTRL = 2, MOD_ALT = 4 };
typedef struct {
    int type;
    Window *win;
    int x, y;             /* in the window's client area */
    int button;           /* the button that went down or up */
    int buttons;          /* the buttons held */
    int clicks;           /* 2 for the second press of a double click */
    int wheel;            /* +1 a notch away from the user, -1 towards */
    int key;              /* asmOS's KEY_ code, keys.h */
    int ch;               /* the character the key types, or 0 */
    int mods;             /* MOD_ held */
    int id;               /* of the command or the timer */
    int w, h;             /* of MSG_RESIZE */
    char text[128];       /* of MSG_DROP */
} Message;

/* Waits for the next message for any of the process's windows. */
void os_get_message(Message *message);
/* The next message if one is waiting: 1, or 0 at once. */
int os_peek_message(Message *message);
/* What most windows do with a message they do not handle: MSG_CLOSE closes the window, and the
 * process ends with its last window; MSG_QUIT ends it. */
void os_default_message(Message *message);
/* A MSG_TIMER with this id every `ms` milliseconds for the window; 0 stops it. */
void os_set_timer(Window *win, int id, int ms);
/* Whether a key (KEY_ code, keys.h) is held now, for games. */
int os_key_down(int key);

/* ------------------------------------------------------------------ dialogs */

enum { MB_OK, MB_OK_CANCEL, MB_YES_NO, MB_YES_NO_CANCEL };
enum { MB_INFO, MB_WARNING, MB_QUESTION };
enum { ID_OK = 1, ID_CANCEL, ID_YES, ID_NO };
/* A message box over `owner` (or none), waiting for an answer: ID_OK and the rest. */
int os_message_box(Window *owner, const char *title, const char *text, int buttons, int kind);
/*
 * The Open or Save As dialog: a folder's files, a name to type, OK and Cancel. `extension`, such
 * as ".txt", limits the files shown and is added to a typed name without one; NULL shows all.
 * `path` holds the starting file or folder and gets the choice. Answers 1 when chosen.
 */
int os_file_dialog(Window *owner, int save, const char *title, const char *extension, char *path,
                   int size);
/* A box asking for a line of text, such as a new name: 1 when OK. */
int os_input_box(Window *owner, const char *title, const char *prompt, char *text, int size);

/* ------------------------------------------------------------------ the clipboard */

enum { CLIP_NONE, CLIP_TEXT, CLIP_PICTURE };
int os_clipboard_kind(void);
void os_clipboard_set_text(const char *text, int length);
/* A copy of the text in os_alloc'd memory, with a 0 after it, or NULL. */
char *os_clipboard_text(int *length);
void os_clipboard_set_picture(const uint32_t *pixels, int w, int h);
/* A copy of the picture in os_alloc'd memory, or NULL. */
uint32_t *os_clipboard_picture(int *w, int *h);

/* ------------------------------------------------------------------ drag and drop */

/* Starts dragging a file while the left button is held: dropped on a window, the window's
 * process gets MSG_DROP; dropped on the desktop, the file is copied into the Desktop folder. */
void os_drag_file(const char *path);

/* ------------------------------------------------------------------ settings */

enum { WALLPAPER_TEAL, WALLPAPER_BLUE, WALLPAPER_GREEN, WALLPAPER_PLUM, WALLPAPER_GRADIENT,
       WALLPAPER_PATTERN, WALLPAPER_PICTURE, WALLPAPER_COUNT };
enum { THEME_CLASSIC, THEME_OLIVE, THEME_PLUM, THEME_SLATE, THEME_COUNT };
typedef struct {
    int wallpaper;
    char picture[64];      /* the .bmp of WALLPAPER_PICTURE */
    int theme;
    int double_click_ms;
    int clock_24h;
    int sounds;
} Settings;
const Settings *os_settings(void);
/* Applies and saves Settings, in asmos.ini; every window gets MSG_SETTINGS. */
void os_apply_settings(const Settings *settings);
/* The theme's colours: the active title bar's two ends, and the selection's. */
uint32_t os_theme_colour(int which);
enum { THEME_TITLE, THEME_TITLE_END, THEME_INACTIVE, THEME_INACTIVE_END, THEME_SELECTION };

/* ------------------------------------------------------------------ sound */

/* Notes on the General MIDI synthesizer, through the kernel's MIDI port: a channel's instrument,
 * a note on and off, and a note played later for a time. Channel 9 is the drums. */
void os_midi_program(int channel, int program);
void os_midi_note(int channel, int note, int velocity, int start_ms, int length_ms);
void os_midi_send(const unsigned char *bytes, int length);
void os_beep(void);
/*
 * Sampled sound through /dev/dsp, 16-bit mono at `rate` samples a second, one process at a time:
 * os_pcm_open answers 0, or -1 while another process has it. os_pcm_space says how many samples
 * fit without waiting and os_pcm_write writes at most that many, so a game never stalls on it.
 * The kernel closes it when the process ends.
 */
int os_pcm_open(int rate);
int os_pcm_space(void);
int os_pcm_write(const int16_t *samples, int count);
void os_pcm_close(void);

/* ------------------------------------------------------------------ the applications */

enum {
    APP_NOTEPAD, APP_CALCULATOR, APP_PAINT, APP_CLOCK, APP_VIEWER, APP_MUSIC, APP_VIDEO, APP_HEX,
    APP_IDE, APP_PACMAN, APP_BLOCKS, APP_MINES, APP_DOOM,
    APP_TERMINAL, APP_FILES, APP_TASKS, APP_SETTINGS, APP_ABOUT,
    APP_COUNT
};
enum { GROUP_ACCESSORIES, GROUP_GAMES, GROUP_SYSTEM };
typedef struct {
    const char *name;
    const Icon *icon;
    void (*main)(const char *argument);
    int group;
    int on_desktop;
    const char *command;      /* what the Terminal's `open` calls it */
} App;
extern const App APPS[APP_COUNT];
int os_app_named(const char *name);

/* Each application's process, in os/apps/. */
void notepad_main(const char *argument);
void calculator_main(const char *argument);
void paint_main(const char *argument);
void clock_main(const char *argument);
void viewer_main(const char *argument);
void music_main(const char *argument);
void video_main(const char *argument);
void hex_main(const char *argument);
void ide_main(const char *argument);      /* os/ide/ */
void pacman_main(const char *argument);
void blocks_main(const char *argument);
void mines_main(const char *argument);
void doom_main(const char *argument);     /* os/doom/ */
void terminal_main(const char *argument);
void files_main(const char *argument);
void tasks_main(const char *argument);
void settings_main(const char *argument);
void about_main(const char *argument);

/* Shuts asmOS down, after asking every process to end. */
void os_shutdown(void);

/* The machine asmOS runs on, in words, as its platform puts them: About asmOS's Processor line,
 * the Terminal's uname -a, and About's lines on how asmOS runs there. */
enum { MACHINE_PROCESSOR, MACHINE_UNAME, MACHINE_CODE, MACHINE_RUNS, MACHINE_FILES, MACHINE_LINES };
const char *os_machine(int what);

#endif
