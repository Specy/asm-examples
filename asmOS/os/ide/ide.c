/*
 * Assembly Editor: writes, builds, runs and debugs RISC-V programs inside asmOS.
 *
 * The programs are RARS's: RV32IM assembly with RARS's directives, pseudo-instructions, ecalls,
 * memory map and devices, so that a program runs the same here and in the web editor's RISC-V
 * Target. The assembler and the interpreter, rv_asm.c and rv_cpu.c (see rv.h), are plain C with
 * nothing of asmOS in them; this File is the window around them: an editor with RISC-V colours,
 * line numbers and a breakpoint gutter; Build, Run, Pause, Step, Step back and Stop; the registers,
 * the memory, a console that also takes the program's input, the Build's errors; and a second
 * window for the bitmap display that a `# @screen` comment asks for.
 *
 * A program runs in slices of a few milliseconds, between which the window takes its messages and
 * shows what changed, so the desktop and the editor stay responsive while it runs. Every step is
 * written to a journal, so Step back takes instructions back one at a time.
 *
 * Programs are `.riscv` Files, the web editor's name for RISC-V assembly: a Build of asmOS
 * assembles every `.s` and `.asm` File of the Project with NASM, as x86.
 *
 * Like DOOM, this is compiled apart from os/apps/apps.c into its own Generated assembly, and the
 * Build links it by ide_main's name.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input-event-codes.h>

#include "../os.h"

#define RV_ALLOC(size) os_alloc(size)
#define RV_REALLOC(pointer, size) os_realloc((pointer), (size))
#define RV_FREE(pointer) os_free(pointer)
#include "rv_asm.c"
#include "rv_cpu.c"
#include "../apps/widgets.c"

enum {
    IDE_CAPACITY = 65536,      /* bytes of program text */
    IDE_JOURNAL = 65536,       /* Step back's journal entries: 1 MiB, tens of thousands of steps */
    IDE_MAX_BREAKS = 128,
    IDE_LINE_MAX = 512,        /* the part of a line that is coloured */
    IDE_CONSOLE = 32768,       /* bytes of output the console keeps */
    IDE_INPUT = 200,           /* a line of input */
    IDE_EXAMPLES = 24,
    IDE_SLICE_MS = 12,         /* how long one slice of a running program should take */
};
/* The layout, in pixels. */
enum {
    TOOL_H = 26, SIDE_W = 216, BOTTOM_H = 134, GAP = 2, GUTTER_W = 40, SCROLL_W = 16,
    LINE_H = 14, ROW_H = 13, HEAD_H = 17, TAB_H = 18, BUTTON_W = 50, BUTTONS = 6
};
enum { TB_BUILD, TB_RUN, TB_PAUSE, TB_STEP, TB_BACK, TB_STOP };
enum { FOCUS_EDITOR, FOCUS_CONSOLE };
enum { TAB_CONSOLE, TAB_PROBLEMS };
enum { MEM_DATA, MEM_STACK, MEM_ADDRESS };
enum { SIDE_REGISTERS, SIDE_BITMAP };
enum { IDE_BLINK = 1 };
enum { NOTE_INFO, NOTE_ERROR };
enum {
    IC_NEW = 1, IC_OPEN, IC_SAVE, IC_SAVE_AS, IC_EXIT,
    IC_CUT, IC_COPY, IC_PASTE, IC_SELECT_ALL, IC_GOTO,
    IC_BUILD, IC_RUN, IC_PAUSE, IC_STEP, IC_BACK, IC_STOP, IC_RESET, IC_BREAK, IC_CLEAR_BREAKS, IC_HISTORY,
    IC_BITMAP, IC_MEM_DATA, IC_MEM_STACK, IC_MEM_ADDRESS,
    IC_ECALLS, IC_DEVICES, IC_ABOUT,
    IC_EXAMPLE = 100
};

/* The colours of the code. */
enum { K_TEXT, K_COMMENT, K_LABEL, K_MNEMONIC, K_DIRECTIVE, K_REGISTER, K_NUMBER, K_STRING };
static const uint32_t IDE_KIND_COLOUR[] = {
    [K_TEXT] = 0x000000, [K_COMMENT] = 0x008000, [K_LABEL] = 0x900000, [K_MNEMONIC] = 0x0000c0,
    [K_DIRECTIVE] = 0x800080, [K_REGISTER] = 0x006f6f, [K_NUMBER] = 0xa04800, [K_STRING] = 0xa00060,
};
enum {
    C_PC_LINE = 0xfff1a0, C_ERROR_LINE = 0xffd6d6, C_GUTTER = 0xe8e8e8, C_CHANGED = 0xc00000,
    C_CHANGED_BG = 0xfff1a0, C_INPUT = 0x0000c0, C_NOTE = 0x000080, C_FAULT = 0xc00000, C_DIM = 0x808080
};

static const char IDE_TEMPLATE[] =
    "# A RISC-V program, as RARS runs it.\n"
    "# Build (F3), Run (F5), Step (F7).\n"
    "# File > Examples has more programs.\n"
    "\n"
    "    .data\n"
    "message: .asciz \"Hello from RISC-V!\\n\"\n"
    "\n"
    "    .text\n"
    "main:\n"
    "    la      a0, message\n"
    "    li      a7, 4         # print string\n"
    "    ecall\n"
    "    li      a7, 10        # exit\n"
    "    ecall\n";

static const char *const IDE_BUTTON_LABEL[BUTTONS] = { "Build", "Run", "Pause", "Step", "Back", "Stop" };

static const MenuItem IDE_FILE_MENU[] = {
    { "New", IC_NEW, 0, NULL, NULL },
    { "Open...", IC_OPEN, 0, "Ctrl+O", NULL },
    { "Examples", 0, 0, NULL, NULL },
    { "Save", IC_SAVE, 0, "Ctrl+S", NULL },
    { "Save As...", IC_SAVE_AS, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", IC_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem IDE_EDIT_MENU[] = {
    { "Cut", IC_CUT, 0, "Ctrl+X", NULL },
    { "Copy", IC_COPY, 0, "Ctrl+C", NULL },
    { "Paste", IC_PASTE, 0, "Ctrl+V", NULL },
    { "Select All", IC_SELECT_ALL, 0, "Ctrl+A", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Go to Line...", IC_GOTO, 0, "Ctrl+G", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem IDE_RUN_MENU[] = {
    { "Build", IC_BUILD, 0, "F3", NULL },
    { "Run", IC_RUN, 0, "F5", NULL },
    { "Pause", IC_PAUSE, 0, "F9", NULL },
    { "Step", IC_STEP, 0, "F7", NULL },
    { "Step Back", IC_BACK, 0, "F8", NULL },
    { "Stop", IC_STOP, 0, "F11", NULL },
    { "Back to the Start", IC_RESET, 0, "F12", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Toggle Breakpoint", IC_BREAK, 0, "F2", NULL },
    { "Clear Breakpoints", IC_CLEAR_BREAKS, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Keep History for Step Back", IC_HISTORY, MENU_CHECKED, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem IDE_VIEW_MENU[] = {
    { "Bitmap Display Window", IC_BITMAP, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Memory: Data", IC_MEM_DATA, 0, NULL, NULL },
    { "Memory: Stack", IC_MEM_STACK, 0, NULL, NULL },
    { "Memory: Go to Address...", IC_MEM_ADDRESS, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem IDE_HELP_MENU[] = {
    { "Ecalls", IC_ECALLS, 0, NULL, NULL },
    { "Devices", IC_DEVICES, 0, NULL, NULL },
    { "About Assembly Editor", IC_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem IDE_CONTEXT_MENU[] = {
    { "Cut", IC_CUT, 0, NULL, NULL },
    { "Copy", IC_COPY, 0, NULL, NULL },
    { "Paste", IC_PASTE, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Toggle Breakpoint", IC_BREAK, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

typedef struct {
    Window *win, *bitmap_win;
    int w, h;

    /* the text: a buffer, and where each line starts */
    char *text;
    int length;
    int *starts;
    int lines;
    int cursor, anchor, goal, top, left;
    int blink, modified, selecting, focus;
    char path[160];
    int breaks[IDE_MAX_BREAKS];   /* lines, 0 based */
    int break_count;

    /* the Build and the run */
    RvProgram program;
    int built;                    /* program holds the last Build, errors or not */
    int edited;                   /* the text changed since */
    RvMachine machine;
    int session;                  /* machine holds the program, ready, running or ended */
    int running;
    int history;                  /* the journal is on */
    long wake_at;                 /* a sleep ecall waits until then */
    uint32_t slice;               /* instructions a slice runs */
    uint32_t prev[32];            /* the registers before the last step, to show what changed */
    int selected;                 /* the register whose number shows in decimal */
    char status[120];
    char note[200];               /* the end of the run, under the console's output */
    int note_kind;
    uint64_t speed_steps;
    long speed_time, regs_drawn, console_drawn, bitmap_drawn;
    long speed;                   /* instructions a second, while running */
    int64_t epoch_ms;             /* the calendar time when os_now_ms was epoch_at */
    long epoch_at;

    /* the console: the program's output, the line being typed for a read ecall */
    char *console;
    int console_length;
    uint64_t console_base;        /* the output byte console[0] is */
    int console_scroll;           /* rows scrolled up from the bottom */
    int console_dirty;
    char input[IDE_INPUT];
    int input_length;
    int tab;
    int problems_top;

    /* the memory view */
    int mem_mode, mem_scroll;
    uint32_t mem_address;
    uint32_t mem_shown_at, mem_shown[32];

    /* the parts of the window */
    Rect tool_r, edit_r, gutter_r, text_r, bottom_r, console_r, regs_r, mem_r, side_r;
    Rect buttons[BUTTONS], tabs[2], mem_buttons[3], side_tabs[2];
    int side_tab;                 /* the side shows the registers and memory, or the bitmap */
    ButtonTracker tracker;
    ScrollBar scroll;

    uint32_t bitmap_row[1024];
    MenuItem file_menu[sizeof IDE_FILE_MENU / sizeof IDE_FILE_MENU[0]];
    MenuItem edit_menu[sizeof IDE_EDIT_MENU / sizeof IDE_EDIT_MENU[0]];
    MenuItem run_menu[sizeof IDE_RUN_MENU / sizeof IDE_RUN_MENU[0]];
    MenuItem view_menu[sizeof IDE_VIEW_MENU / sizeof IDE_VIEW_MENU[0]];
    MenuItem help_menu[sizeof IDE_HELP_MENU / sizeof IDE_HELP_MENU[0]];
    MenuItem example_items[IDE_EXAMPLES + 1];
    char example_names[IDE_EXAMPLES][64];
    Menu menus[6];
} Ide;

static void ide_draw_all(Ide *I);
static void ide_draw_editor(Ide *I);
static void ide_draw_toolbar(Ide *I);
static void ide_draw_bottom(Ide *I);
static void ide_draw_side(Ide *I);
static void ide_bitmap_draw(Ide *I);
static void ide_bitmap_draw_rows(Ide *I, int full);

/* ------------------------------------------------------------------ the text */

static void ed_reindex(Ide *I) {
    int n = 0;
    I->starts[n++] = 0;
    for (int i = 0; i < I->length; i++)
        if (I->text[i] == '\n') I->starts[n++] = i + 1;
    I->lines = n;
}

static int ed_line_of(const Ide *I, int index) {
    int lo = 0, hi = I->lines - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (I->starts[mid] <= index) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}
static int ed_line_end(const Ide *I, int line) {
    return line + 1 < I->lines ? I->starts[line + 1] - 1 : I->length;
}
static int ed_index_at(const Ide *I, int line, int column) {
    line = clamp_int(line, 0, I->lines - 1);
    int start = I->starts[line];
    return start + min_int(max_int(column, 0), ed_line_end(I, line) - start);
}
static int ed_column(const Ide *I, int index) { return index - I->starts[ed_line_of(I, index)]; }
static int sel_from(const Ide *I) { return min_int(I->cursor, I->anchor); }
static int sel_to(const Ide *I) { return max_int(I->cursor, I->anchor); }

static int ed_rows(const Ide *I) { return max_int(1, I->text_r.h / LINE_H); }
static int ed_cols(const Ide *I) { return max_int(1, (I->text_r.w - 4) / FONT_WIDTH); }

/* Breakpoints follow their lines when lines are added or taken out above them: `delta` lines
 * appeared (or, negative, went) just after `line`. */
static void ed_shift_breaks(Ide *I, int line, int delta) {
    int kept = 0;
    for (int i = 0; i < I->break_count; i++) {
        int b = I->breaks[i];
        if (b > line && delta < 0 && b <= line - delta) continue;   /* its line was joined to `line` */
        I->breaks[kept++] = b > line ? b + delta : b;
    }
    I->break_count = kept;
}

static void ed_delete(Ide *I, int from, int to) {
    if (from >= to) return;
    int line = ed_line_of(I, from), joined = 0;
    for (int i = from; i < to; i++) joined += I->text[i] == '\n';
    memmove(I->text + from, I->text + to, (size_t)(I->length - to));
    I->length -= to - from;
    I->text[I->length] = 0;
    I->cursor = I->anchor = from;
    I->modified = I->edited = 1;
    if (joined) ed_shift_breaks(I, line, -joined);
    ed_reindex(I);
}

/* Inserts text in place of the selection, without tabs or carriage returns. */
static void ed_insert(Ide *I, const char *text, int count) {
    ed_delete(I, sel_from(I), sel_to(I));
    int column = ed_column(I, I->cursor), added = 0, size = 0;
    for (int i = 0; i < count; i++) {
        char c = text[i];
        if (c == '\r') continue;
        if (c == '\t') {
            int spaces = 8 - (column + size) % 8;
            size += spaces;
            continue;
        }
        size++;
        if (c == '\n') column = -size;
    }
    if (I->length + size >= IDE_CAPACITY) {
        os_beep();
        snprintf(I->status, sizeof I->status, "The program is too long: %d bytes at most", IDE_CAPACITY - 1);
        return;
    }
    memmove(I->text + I->cursor + size, I->text + I->cursor, (size_t)(I->length - I->cursor));
    int at = I->cursor, line = ed_line_of(I, at), col = ed_column(I, at);
    for (int i = 0; i < count; i++) {
        char c = text[i];
        if (c == '\r') continue;
        if (c == '\t') {
            do I->text[at++] = ' ', col++;
            while (col % 8);
            continue;
        }
        I->text[at++] = c;
        col++;
        if (c == '\n') added++, col = 0;
    }
    I->length += size;
    I->text[I->length] = 0;
    I->cursor = I->anchor = at;
    I->modified = I->edited = 1;
    if (added) ed_shift_breaks(I, line, added);
    ed_reindex(I);
}

/* Scrolls so that the cursor shows; answers whether anything scrolled. */
static int ed_follow(Ide *I) {
    int line = ed_line_of(I, I->cursor), column = I->cursor - I->starts[line];
    int top = I->top, left = I->left, rows = ed_rows(I), cols = ed_cols(I);
    if (line < I->top) I->top = line;
    if (line >= I->top + rows) I->top = line - rows + 1;
    if (column < I->left) I->left = max_int(0, column - 8);
    if (column >= I->left + cols) I->left = column - cols + 8;
    I->top = clamp_int(I->top, 0, max_int(0, I->lines - 1));
    return top != I->top || left != I->left;
}

/* Puts a line in the middle of the editor when it is out of sight. */
static void ed_reveal(Ide *I, int line) {
    int rows = ed_rows(I);
    if (line < I->top || line >= I->top + rows) I->top = max_int(0, line - rows / 2);
}

static void ed_set_text(Ide *I, const char *text, long size) {
    I->length = 0;
    I->cursor = I->anchor = 0;
    int kept = 0, column = 0;
    for (long i = 0; i < size && kept < IDE_CAPACITY - 9; i++) {
        char c = text[i];
        if (c == '\r') continue;
        if (c == '\t') {
            do I->text[kept++] = ' ';
            while (++column % 8);
            continue;
        }
        I->text[kept++] = c;
        column = c == '\n' ? 0 : column + 1;
    }
    I->text[kept] = 0;
    I->length = kept;
    I->cursor = I->anchor = I->top = I->left = I->goal = 0;
    I->modified = 0;
    I->edited = 1;
    I->break_count = 0;
    ed_reindex(I);
}

/* ------------------------------------------------------------------ colours */

static int ide_word_char(char c) { return isalnum((unsigned char)c) || c == '_' || c == '.' || c == '$'; }

static int ide_is_register(const char *s, int n) {
    if (n < 2 || n > 4) return 0;
    char w[5];
    memcpy(w, s, (size_t)n);
    w[n] = 0;
    if (w[0] == 'x') {
        for (int i = 1; i < n; i++)
            if (!isdigit((unsigned char)w[i])) return 0;
        return atoi(w + 1) <= 31;
    }
    static const char *const names[] = { "zero", "ra", "sp", "gp", "tp", "fp" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
        if (strcmp(w, names[i]) == 0) return 1;
    if (n == 2 && w[0] == 't') return w[1] >= '0' && w[1] <= '6';
    if (n == 2 && w[0] == 'a') return w[1] >= '0' && w[1] <= '7';
    if (w[0] == 's') return (n == 2 && isdigit((unsigned char)w[1])) || (n == 3 && w[1] == '1' && (w[2] == '0' || w[2] == '1'));
    return 0;
}

/* The kind of each character of a line: comments, labels, the mnemonic, directives, registers,
 * numbers, strings and characters. */
static void ide_classify(const char *s, int n, uint8_t *kind) {
    int i = 0, operation = 0;
    while (i < n) {
        char c = s[i];
        int j = i + 1, k = K_TEXT;
        if (c == '#') {
            memset(kind + i, K_COMMENT, (size_t)(n - i));
            return;
        }
        if (c == '"' || c == '\'') {
            while (j < n && s[j] != c) j += s[j] == '\\' && j + 1 < n ? 2 : 1;
            j = min_int(n, j + 1);
            k = K_STRING;
        } else if (isalpha((unsigned char)c) || c == '_' || c == '.' || c == '$' || c == '%') {
            while (j < n && ide_word_char(s[j])) j++;
            int after = j;
            while (after < n && s[after] == ' ') after++;
            if (!operation && after < n && s[after] == ':') {
                k = K_LABEL;
                j = after + 1;
            } else if (c == '.' || c == '%') {
                k = K_DIRECTIVE;
                operation |= c == '.';
            } else if (!operation) {
                k = K_MNEMONIC;
                operation = 1;
            } else if (ide_is_register(s + i, j - i)) {
                k = K_REGISTER;
            }
        } else if (isdigit((unsigned char)c) ||
                   (c == '-' && j < n && isdigit((unsigned char)s[j]) && !(i > 0 && ide_word_char(s[i - 1])))) {
            while (j < n && isalnum((unsigned char)s[j])) j++;
            k = K_NUMBER;
        }
        memset(kind + i, k, (size_t)(j - i));
        i = j;
    }
}

/* ------------------------------------------------------------------ what the interpreter asks */

static void con_clear(Ide *I) {
    I->console_length = 0;
    I->console_base = 0;
    I->console_scroll = 0;
    I->input_length = 0;
    I->note[0] = 0;
    I->console_dirty = 1;
}

static void ide_host_write(void *context, const char *text, int length) {
    Ide *I = context;
    for (int i = 0; i < length; i++) {
        if (text[i] == '\f') {
            /* a form feed clears the console, as the web editor's Terminal does */
            I->console_base += (uint64_t)I->console_length + 1;
            I->console_length = 0;
            continue;
        }
        if (I->console_length == IDE_CONSOLE) {
            /* the oldest half goes */
            int drop = IDE_CONSOLE / 2;
            memmove(I->console, I->console + drop, (size_t)(IDE_CONSOLE - drop));
            I->console_length -= drop;
            I->console_base += (uint64_t)drop;
        }
        I->console[I->console_length++] = text[i];
    }
    I->console_scroll = 0;
    I->console_dirty = 1;
}

static void ide_host_rewind(void *context, uint64_t length) {
    Ide *I = context;
    if (length <= I->console_base) {
        I->console_length = 0;
        I->console_base = length;
    } else if (length - I->console_base < (uint64_t)I->console_length) {
        I->console_length = (int)(length - I->console_base);
    }
    I->console_dirty = 1;
}

static int64_t ide_host_time(void *context) {
    Ide *I = context;
    return I->epoch_ms + (os_now_ms() - I->epoch_at);
}

static void ide_host_midi(void *context, int pitch, int duration_ms, int instrument, int volume) {
    (void)context;
    os_midi_program(14, instrument);
    os_midi_note(14, pitch, volume, 0, duration_ms);
}

static int64_t ide_host_seed(void *context) {
    Ide *I = context;
    uint64_t seed = (uint64_t)ide_host_time(I) * 6364136223846793005ull + (uint64_t)os_instructions();
    return (int64_t)(seed >> 16);
}

/* Days from 1970-01-01 to a date, by Howard Hinnant's method. */
static int64_t ide_days(int year, int month, int day) {
    year -= month <= 2;
    int64_t era = (year >= 0 ? year : year - 399) / 400;
    int yoe = (int)(year - era * 400);
    int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

/* ------------------------------------------------------------------ the run */

static int ide_alive(const Ide *I) { return I->session && !I->machine.exited && !I->machine.faulted; }

/* The line pc is on, 0 based, while the program is stopped somewhere it can go on from; or -1. */
static int ide_pc_line(const Ide *I) {
    if (!I->session || I->running || I->edited) return -1;
    if (I->machine.exited && !I->machine.faulted) return -1;
    return rv_line_of(&I->program, I->machine.pc) - 1;
}

static int ide_has_break(const Ide *I, int line) {
    for (int i = 0; i < I->break_count; i++)
        if (I->breaks[i] == line) return 1;
    return 0;
}

static int ide_line_has_error(const Ide *I, int line) {
    if (!I->built || I->edited) return 0;
    int count = min_int(I->program.error_count, RV_MAX_ERRORS);
    for (int i = 0; i < count; i++)
        if (I->program.errors[i].line == line + 1) return 1;
    return 0;
}

/* The first line from `line` on that has an instruction, 0 based, or -1. */
static int ide_code_line(const Ide *I, int line) {
    for (int l = line; l < I->program.line_count; l++)
        if (rv_address_of_line(&I->program, l + 1)) return l;
    return -1;
}

/* Gives the machine the breakpoints, each moved to the first line with an instruction. */
static void ide_apply_breaks(Ide *I) {
    rv_clear_breakpoints(&I->machine);
    int kept = 0;
    for (int i = 0; i < I->break_count; i++) {
        int line = ide_code_line(I, I->breaks[i]);
        if (line < 0) continue;
        int twice = 0;
        for (int j = 0; j < kept; j++) twice |= I->breaks[j] == line;
        if (twice) continue;
        I->breaks[kept++] = line;
        rv_set_breakpoint(&I->machine, rv_address_of_line(&I->program, line + 1), 1);
    }
    I->break_count = kept;
}

static void ide_toggle_break(Ide *I, int line) {
    if (line < 0 || line >= I->lines) return;
    if (I->session && !I->edited) {
        int code = ide_code_line(I, line);
        if (code < 0) {
            snprintf(I->status, sizeof I->status, "No instruction on or after line %d", line + 1);
            return;
        }
        line = code;
    }
    int at = -1;
    for (int i = 0; i < I->break_count; i++)
        if (I->breaks[i] == line) at = i;
    if (at >= 0) I->breaks[at] = I->breaks[--I->break_count];
    else if (I->break_count < IDE_MAX_BREAKS) I->breaks[I->break_count++] = line;
    if (I->session && !I->edited)
        rv_set_breakpoint(&I->machine, rv_address_of_line(&I->program, line + 1), at < 0);
    snprintf(I->status, sizeof I->status, "Breakpoint %s line %d", at < 0 ? "on" : "off", line + 1);
}

/* The registers as they are, to compare with after the next step. */
static void ide_mark(Ide *I) { memcpy(I->prev, I->machine.x, sizeof I->prev); }

static void ide_end_session(Ide *I) {
    if (I->session) rv_machine_free(&I->machine);
    memset(&I->machine, 0, sizeof I->machine);
    I->session = 0;
    I->running = 0;
    I->wake_at = 0;
}

/* The bitmap display the program asks for, or before a Build the web editor's default. */
static RvScreen ide_screen(const Ide *I) {
    if (I->built && I->program.screen.width > 0) return I->program.screen;
    RvScreen s = { 0, 0, 512, 256, 1, 1, RV_DATA_BASE };
    return s;
}

/* The bitmap display's grid: words across and down. */
static void ide_grid(const Ide *I, int *cols, int *rows) {
    RvScreen s = ide_screen(I);
    *cols = max_int(1, s.width / max_int(1, s.unit_w));
    *rows = max_int(1, s.height / max_int(1, s.unit_h));
}

static void ide_bitmap_open(Ide *I) {
    int cols, rows;
    ide_grid(I, &cols, &rows);
    RvScreen s = ide_screen(I);
    int w = clamp_int(cols * s.unit_w, 64, min_int(1024, screen_w - 8));
    int h = clamp_int(rows * s.unit_h, 32, screen_h - 60);
    if (I->bitmap_win) {
        int pw, ph;
        os_window_pixels(I->bitmap_win, &pw, &ph);
        if (pw == w && ph == h) {
            ide_bitmap_draw(I);
            return;
        }
        os_window_close(I->bitmap_win);
    }
    I->bitmap_win = os_window_create("Bitmap Display", &ICON_IDE, w, h, 0);
    I->view_menu[0].flags = I->bitmap_win ? MENU_CHECKED : 0;
    /* at the right, over the registers and the memory rather than the code and the toolbar */
    if (I->bitmap_win) os_window_move(I->bitmap_win, screen_w - w - 8, 40);
    ide_bitmap_draw(I);
}

static void ide_bitmap_close(Ide *I) {
    if (I->bitmap_win) os_window_close(I->bitmap_win);
    I->bitmap_win = NULL;
    I->view_menu[0].flags = 0;
}

/* Assembles the text; answers 1 when the program is ready to run. */
static int ide_build(Ide *I) {
    ide_end_session(I);
    rv_program_free(&I->program);
    memset(&I->program, 0, sizeof I->program);
    long started = os_now_ms();
    rv_assemble(&I->program, I->text, (size_t)I->length);
    long took = os_now_ms() - started;
    I->built = 1;
    I->edited = 0;
    I->problems_top = 0;
    con_clear(I);
    if (I->program.error_count) {
        I->tab = TAB_PROBLEMS;
        I->focus = FOCUS_EDITOR;
        snprintf(I->status, sizeof I->status, "Build failed: %d error%s", I->program.error_count,
                 I->program.error_count == 1 ? "" : "s");
        return 0;
    }
    RvHost host = { I, ide_host_write, ide_host_rewind, ide_host_time, ide_host_midi, ide_host_seed };
    if (rv_machine_init(&I->machine, &I->program, &host, IDE_JOURNAL, 0) != 0) {
        memset(&I->machine, 0, sizeof I->machine);
        snprintf(I->status, sizeof I->status, "Not enough memory to run the program");
        return 0;
    }
    I->session = 1;
    if (!I->history) rv_journal(&I->machine, 0);
    int cols, rows;
    ide_grid(I, &cols, &rows);
    uint64_t end = (uint64_t)I->program.screen.base + (uint64_t)cols * (uint64_t)rows * 4;
    I->machine.watch_lo = I->program.screen.base;
    I->machine.watch_hi = end > 0xffffffffu ? 0xffffffffu : (uint32_t)end;
    /* the reset in rv_machine_init marked a range of no size: all of the picture is new */
    I->machine.watch_from = I->machine.watch_lo;
    I->machine.watch_to = I->machine.watch_hi - 1;
    ide_apply_breaks(I);
    ide_mark(I);
    I->tab = I->program.warning_count ? TAB_PROBLEMS : TAB_CONSOLE;
    I->mem_scroll = 0;
    I->mem_shown_at = 0;
    snprintf(I->status, sizeof I->status, "Built in %ld ms: %d instructions%s", took, I->program.text_words,
             I->program.warning_count ? ", with warnings" : "");
    if (I->program.screen.present) I->side_tab = SIDE_BITMAP;
    if (I->bitmap_win) ide_bitmap_open(I);
    ed_reveal(I, ide_pc_line(I));
    return 1;
}

/* A program to run: the one built, or a new Build when there is none or the text changed. */
static int ide_ready(Ide *I) {
    if (I->session && !I->edited) return 1;
    return ide_build(I);
}

static void ide_reset(Ide *I) {
    rv_reset(&I->machine);
    if (!I->history) rv_journal(&I->machine, 0);
    con_clear(I);
    ide_mark(I);
    I->wake_at = 0;
    I->mem_shown_at = 0;
    ide_bitmap_draw(I);
}

static void ide_paused_status(Ide *I, const char *what) {
    snprintf(I->status, sizeof I->status, "%s at line %d", what, rv_line_of(&I->program, I->machine.pc));
}

/* What a stop of rv_run means for the window. */
static void ide_after_stop(Ide *I, int stop) {
    RvMachine *m = &I->machine;
    switch (stop) {
    case RV_STOP_EXIT:
        I->running = 0;
        I->note_kind = NOTE_INFO;
        if (m->cliff)
            snprintf(I->note, sizeof I->note, "The program ran off the end of its code.");
        else
            snprintf(I->note, sizeof I->note, "The program ended with exit code %d.", m->exit_code);
        snprintf(I->status, sizeof I->status, "Ended, exit code %d, %llu steps", m->exit_code,
                 (unsigned long long)m->steps);
        I->console_dirty = 1;
        break;
    case RV_STOP_FAULT: {
        I->running = 0;
        int line = rv_line_of(&I->program, m->pc);
        I->note_kind = NOTE_ERROR;
        if (line) snprintf(I->note, sizeof I->note, "Error on line %d: %s", line, m->fault);
        else snprintf(I->note, sizeof I->note, "Error: %s", m->fault);
        snprintf(I->status, sizeof I->status, "Error on line %d", line);
        I->tab = TAB_CONSOLE;
        I->console_dirty = 1;
        if (line) ed_reveal(I, line - 1);
        break;
    }
    case RV_STOP_BREAKPOINT:
        I->running = 0;
        ide_paused_status(I, "Breakpoint");
        ed_reveal(I, rv_line_of(&I->program, m->pc) - 1);
        break;
    case RV_STOP_INPUT:
        I->focus = FOCUS_CONSOLE;
        I->tab = TAB_CONSOLE;
        snprintf(I->status, sizeof I->status, m->want == RV_WANT_CHAR ? "Waiting for a key: type it in the console"
                                              : "Waiting for input: type, then Enter");
        I->console_dirty = 1;
        break;
    case RV_STOP_SLEEP:
        if (I->running) I->wake_at = os_now_ms() + m->sleep_ms;
        break;
    }
}

static void ide_run(Ide *I) {
    if (I->running) return;
    if (!ide_ready(I)) {
        ide_draw_all(I);
        return;
    }
    if (I->machine.exited || I->machine.faulted) ide_reset(I);
    ide_mark(I);
    I->note[0] = 0;
    I->running = 1;
    I->wake_at = 0;
    I->slice = 20000;
    I->speed = 0;
    I->speed_steps = I->machine.steps;
    I->speed_time = I->regs_drawn = os_now_ms();
    I->focus = FOCUS_CONSOLE;
    I->tab = TAB_CONSOLE;
    snprintf(I->status, sizeof I->status, I->machine.want ? "Waiting for input: type, then Enter" : "Running");
    ide_draw_all(I);
}

static void ide_pause(Ide *I) {
    if (!I->running) return;
    I->running = 0;
    I->wake_at = 0;
    ide_paused_status(I, "Paused");
    ed_reveal(I, ide_pc_line(I));
    ide_draw_all(I);
    ide_bitmap_draw(I);
}

static void ide_step(Ide *I) {
    if (I->running) {
        ide_pause(I);
        return;
    }
    if (!ide_ready(I)) {
        ide_draw_all(I);
        return;
    }
    RvMachine *m = &I->machine;
    if (m->exited || m->faulted) {
        snprintf(I->status, sizeof I->status, "The program has ended: Run starts it again");
    } else if (m->want) {
        I->focus = FOCUS_CONSOLE;
        snprintf(I->status, sizeof I->status, "The program waits for input: type it in the console");
    } else {
        ide_mark(I);
        I->note[0] = 0;
        int stop = rv_run(m, 1, 0);
        if (stop == RV_STOP_LIMIT || stop == RV_STOP_SLEEP) ide_paused_status(I, "Stepped");
        else ide_after_stop(I, stop);
        int line = ide_pc_line(I);
        if (line >= 0) ed_reveal(I, line);
    }
    ide_draw_all(I);
    if (m->watch_dirty) ide_bitmap_draw_rows(I, 0);
}

static void ide_back(Ide *I) {
    if (I->running) ide_pause(I);
    if (!I->session || I->edited) return;
    ide_mark(I);
    if (!rv_step_back(&I->machine)) {
        snprintf(I->status, sizeof I->status, I->history ? "Nothing more to step back"
                                                         : "Step back needs Run > Keep History for Step Back");
    } else {
        I->note[0] = 0;
        I->input_length = 0;
        I->wake_at = 0;
        ide_paused_status(I, "Stepped back");
        ed_reveal(I, ide_pc_line(I));
    }
    I->console_dirty = 1;
    ide_draw_all(I);
    ide_bitmap_draw(I);
}

static void ide_stop(Ide *I) {
    if (!I->session) return;
    ide_end_session(I);
    I->note[0] = 0;
    I->focus = FOCUS_EDITOR;
    snprintf(I->status, sizeof I->status, "Stopped");
    ide_draw_all(I);
    ide_bitmap_draw(I);
}

/* The line typed in the console goes to the read ecall that waits for it. */
static void ide_submit(Ide *I) {
    char line[IDE_INPUT];
    memcpy(line, I->input, (size_t)I->input_length);
    line[I->input_length] = 0;
    I->input_length = 0;
    rv_supply_line(&I->machine, line);
    if (I->machine.faulted) ide_after_stop(I, RV_STOP_FAULT);
    else if (I->running) snprintf(I->status, sizeof I->status, "Running");
    else ide_paused_status(I, "Paused");
    ide_draw_all(I);
}

/* A slice of the running program, then what changed on the screen. */
static void ide_slice(Ide *I) {
    RvMachine *m = &I->machine;
    long started = os_now_ms();
    int stop = rv_run(m, I->slice, 1);
    long took = os_now_ms() - started;
    if (stop == RV_STOP_LIMIT) {
        if (took < IDE_SLICE_MS / 2 && I->slice < (1u << 21)) I->slice *= 2;
        else if (took > IDE_SLICE_MS * 2 && I->slice > 2000) I->slice /= 2;
    }
    if (stop != RV_STOP_LIMIT) ide_after_stop(I, stop);
    if (!I->running || stop == RV_STOP_INPUT) {
        ide_draw_all(I);
        ide_bitmap_draw(I);
    }
}

/* While it runs: the console, the bitmap and the speed now and then, the registers less often. */
static void ide_refresh(Ide *I) {
    long now = os_now_ms();
    if ((I->bitmap_win || I->side_tab == SIDE_BITMAP) && I->machine.watch_dirty && now - I->bitmap_drawn >= 30) {
        ide_bitmap_draw_rows(I, 0);
        I->bitmap_drawn = now;
    }
    if (I->console_dirty && now - I->console_drawn >= 60) {
        ide_draw_bottom(I);
        I->console_drawn = now;
    }
    if (now - I->speed_time >= 500) {
        uint64_t steps = I->machine.steps - I->speed_steps;
        I->speed = (long)(steps * 1000 / (uint64_t)(now - I->speed_time));
        I->speed_steps = I->machine.steps;
        I->speed_time = now;
        if (!I->machine.want) {
            char rate[24], count[24];
            snprintf(rate, sizeof rate, "%ld", I->speed);
            snprintf(count, sizeof count, "%llu", (unsigned long long)I->machine.steps);
            snprintf(I->status, sizeof I->status, "Running: %s steps, %s a second", count, rate);
        }
        ide_draw_toolbar(I);
    }
    if (now - I->regs_drawn >= 250 && I->side_tab == SIDE_REGISTERS) {
        ide_draw_side(I);
        ide_mark(I);
        I->regs_drawn = now;
    }
}

/* ------------------------------------------------------------------ drawing */

static void ide_layout(Ide *I) {
    os_window_pixels(I->win, &I->w, &I->h);
    int left = I->w - SIDE_W - GAP;
    int bottom = clamp_int(I->h / 3, 90, 200);
    I->tool_r = rect(0, 0, I->w, TOOL_H);
    I->edit_r = rect(0, TOOL_H, left, I->h - TOOL_H - bottom - GAP);
    I->gutter_r = rect(2, TOOL_H + 2, GUTTER_W, I->edit_r.h - 4);
    I->text_r = rect(2 + GUTTER_W, TOOL_H + 2, left - 4 - GUTTER_W - SCROLL_W, I->edit_r.h - 4);
    I->scroll.r = rect(left - 2 - SCROLL_W, TOOL_H + 2, SCROLL_W, I->edit_r.h - 4);
    I->bottom_r = rect(0, I->h - bottom, left, bottom);
    I->console_r = rect(0, I->h - bottom + TAB_H, left, bottom - TAB_H);
    for (int i = 0; i < 2; i++) I->tabs[i] = rect(2 + i * 104, I->h - bottom + 1, 102, TAB_H - 1);
    int regs_h = HEAD_H + 18 * ROW_H + 6;
    I->regs_r = rect(left + GAP, TOOL_H, SIDE_W, regs_h);
    I->mem_r = rect(left + GAP, TOOL_H + regs_h + GAP, SIDE_W, I->h - TOOL_H - regs_h - GAP);
    for (int i = 0; i < 3; i++) I->mem_buttons[i] = rect(I->mem_r.x + 58 + i * 52, I->mem_r.y + 1, 50, HEAD_H - 2);
    for (int i = 0; i < BUTTONS; i++) I->buttons[i] = rect(4 + i * (BUTTON_W + 2), 3, BUTTON_W, TOOL_H - 6);
    I->side_r = rect(left + GAP, TOOL_H, SIDE_W, I->h - TOOL_H);
    for (int i = 0; i < 2; i++) I->side_tabs[i] = rect(I->side_r.x + 2 + i * 90, TOOL_H + 1, 88, HEAD_H - 1);
}

static int ide_button_enabled(const Ide *I, int button) {
    switch (button) {
    case TB_RUN: return !I->running;
    case TB_PAUSE: return I->running;
    case TB_STEP: return !I->running && (!I->session || I->edited || ide_alive(I));
    case TB_BACK: return I->session && !I->edited && rv_can_step_back(&I->machine);
    case TB_STOP: return I->session;
    }
    return 1;
}

static void ide_draw_toolbar(Ide *I) {
    os_window_draw(I->win);
    Rect t = I->tool_r;
    fill_rect(t.x, t.y, t.w, t.h, FACE);
    hline(0, t.h - 1, t.w, SHADOW);
    for (int i = 0; i < BUTTONS; i++) {
        Rect b = I->buttons[i];
        int enabled = ide_button_enabled(I, i);
        draw_button(b.x, b.y, b.w, b.h, "", enabled && button_down(&I->tracker, i));
        const char *label = IDE_BUTTON_LABEL[i];
        int x = b.x + (b.w - text_width(label)) / 2, y = b.y + (b.h - FONT_HEIGHT) / 2;
        if (enabled) {
            draw_text(x, y, label, BLACK);
        } else {
            draw_text(x + 1, y + 1, label, WHITE);
            draw_text(x, y, label, SHADOW);
        }
    }
    int x = I->buttons[BUTTONS - 1].x + BUTTON_W + 8;
    bevel_thin_sunken(x, 3, t.w - x - 4, t.h - 7);
    gfx_clip(rect(x + 2, 3, t.w - x - 8, t.h - 7));
    uint32_t colour = I->status[0] == 'E' && strncmp(I->status, "Error", 5) == 0 ? C_FAULT : BLACK;
    draw_text(x + 4, 6, I->status, colour);
    gfx_clip(rect(0, 0, I->w, I->h));
    os_window_update(I->win, t.x, t.y, t.w, t.h);
}

static void ide_draw_gutter_row(Ide *I, int line, int y, int pc_line) {
    Rect g = I->gutter_r;
    fill_rect(g.x, y, g.w, LINE_H, C_GUTTER);
    if (line >= I->lines) return;
    char number[12];
    snprintf(number, sizeof number, "%d", line + 1);
    draw_text(g.x + g.w - 3 - text_width(number), y + 1, number, C_DIM);
    int cx = g.x + 7, cy = y + LINE_H / 2;
    if (ide_has_break(I, line)) {
        int live = !I->session || I->edited || rv_address_of_line(&I->program, line + 1);
        fill_circle(cx, cy, 5, live ? 0xe00000 : C_GUTTER);
        draw_circle(cx, cy, 5, 0x800000);
    }
    if (line == pc_line) {
        /* an arrow at the line the program stopped on, over the breakpoint if there is one */
        uint32_t colour = I->machine.faulted ? 0xff4040 : 0xffd000;
        int x = cx - 4;
        fill_rect(x, cy - 1, 4, 3, colour);
        for (int i = 0; i < 4; i++) vline(x + 4 + i, cy - 3 + i, 7 - 2 * i, colour);
        hline(x, cy - 2, 4, BLACK);
        hline(x, cy + 2, 4, BLACK);
        vline(x - 1, cy - 2, 5, BLACK);
        for (int i = 0; i < 5; i++) {
            put_pixel(x + 4 + i, cy - 4 + i, BLACK);
            put_pixel(x + 4 + i, cy + 4 - i, BLACK);
        }
    }
}

static void ide_draw_text_row(Ide *I, int line, int y, int pc_line) {
    Rect t = I->text_r;
    gfx_clip(rect(t.x, y, t.w, LINE_H));
    uint32_t background = WHITE;
    if (line == pc_line) background = I->machine.faulted ? C_ERROR_LINE : C_PC_LINE;
    else if (line < I->lines && ide_line_has_error(I, line)) background = C_ERROR_LINE;
    fill_rect(t.x, y, t.w, LINE_H, background);
    if (line < I->lines) {
        int start = I->starts[line], end = ed_line_end(I, line), n = end - start;
        int cols = ed_cols(I) + 1, x0 = t.x + 2 - I->left * FONT_WIDTH;
        int a = max_int(sel_from(I), start), b = min_int(sel_to(I), end + (end < I->length));
        if (a < b)
            fill_rect(x0 + (a - start) * FONT_WIDTH, y, (b - a) * FONT_WIDTH, LINE_H, os_theme_colour(THEME_SELECTION));
        uint8_t kinds[IDE_LINE_MAX];
        int coloured = min_int(n, IDE_LINE_MAX);
        ide_classify(I->text + start, coloured, kinds);
        int from = I->left, to = min_int(n, I->left + cols);
        while (from < to) {
            int selected = start + from >= a && start + from < b;
            uint32_t colour = selected ? WHITE : IDE_KIND_COLOUR[from < coloured ? kinds[from] : K_TEXT];
            int run = from + 1;
            while (run < to) {
                int s = start + run >= a && start + run < b;
                uint32_t c = s ? WHITE : IDE_KIND_COLOUR[run < coloured ? kinds[run] : K_TEXT];
                if (c != colour) break;
                run++;
            }
            draw_text_n(x0 + from * FONT_WIDTH, y + 1, I->text + start + from, run - from, colour);
            from = run;
        }
        if (I->focus == FOCUS_EDITOR && I->blink && I->anchor == I->cursor && os_window_focused(I->win) &&
            I->cursor >= start && I->cursor <= end)
            fill_rect(x0 + (I->cursor - start) * FONT_WIDTH - 1, y, 2, LINE_H, BLACK);
    }
    gfx_clip(rect(0, 0, I->w, I->h));
}

static void ide_draw_scroll(Ide *I) {
    I->scroll.total = I->lines + (I->lines > ed_rows(I) ? ed_rows(I) / 2 : 0);
    I->scroll.visible = ed_rows(I);
    I->scroll.top = I->top;
    draw_scroll_bar(&I->scroll);
}

static void ide_draw_editor(Ide *I) {
    os_window_draw(I->win);
    Rect e = I->edit_r, t = I->text_r, g = I->gutter_r;
    bevel_sunken(e.x, e.y, e.w, e.h);
    int rows = ed_rows(I), pc_line = ide_pc_line(I);
    for (int row = 0; row < rows; row++) {
        int y = t.y + row * LINE_H;
        ide_draw_gutter_row(I, I->top + row, y, pc_line);
        ide_draw_text_row(I, I->top + row, y, pc_line);
    }
    int y = t.y + rows * LINE_H, rest = t.y + t.h - y;
    if (rest > 0) {
        fill_rect(g.x, y, g.w, rest, C_GUTTER);
        fill_rect(t.x, y, t.w, rest, WHITE);
    }
    ide_draw_scroll(I);
    os_window_update(I->win, e.x, e.y, e.w, e.h);
}

/* Redraws one line of the editor, for the cursor's blink and for typing within a line. */
static void ide_draw_line(Ide *I, int line) {
    int row = line - I->top;
    if (row < 0 || row >= ed_rows(I)) return;
    os_window_draw(I->win);
    int y = I->text_r.y + row * LINE_H, pc_line = ide_pc_line(I);
    ide_draw_gutter_row(I, line, y, pc_line);
    ide_draw_text_row(I, line, y, pc_line);
    os_window_update(I->win, I->gutter_r.x, y, I->gutter_r.w + I->text_r.w, LINE_H);
}

static void ide_panel_head(int x, int y, int w, const char *title) {
    fill_rect(x, y, w, HEAD_H, FACE);
    draw_text_bold(x + 4, y + 2, title, BLACK);
}

static void ide_draw_registers(Ide *I) {
    Rect r = I->regs_r;
    draw_field(r.x, r.y + HEAD_H, r.w, r.h - HEAD_H, WHITE);
    int x0 = r.x + 4, y0 = r.y + HEAD_H + 3;
    if (!I->session) {
        draw_text(x0, y0, "Build to see them.", C_DIM);
        return;
    }
    RvMachine *m = &I->machine;
    char text[48];
    for (int i = 0; i < 32; i++) {
        int x = x0 + (i / 16) * 106, y = y0 + (i % 16) * ROW_H;
        int changed = m->x[i] != I->prev[i];
        if (i == I->selected) fill_rect(x - 2, y, 104, ROW_H, 0xdde6ff);
        if (changed) fill_rect(x + 38, y, 66, ROW_H, C_CHANGED_BG);
        draw_text(x, y, rv_register_name(i), C_DIM);
        snprintf(text, sizeof text, "%08x", (unsigned)m->x[i]);
        draw_text(x + 38, y, text, changed ? C_CHANGED : BLACK);
    }
    int y = y0 + 16 * ROW_H;
    draw_text(x0, y, "pc", C_DIM);
    snprintf(text, sizeof text, "%08x", (unsigned)m->pc);
    draw_text(x0 + 38, y, text, BLACK);
    int line = rv_line_of(&I->program, m->pc);
    if (m->exited && !m->faulted) snprintf(text, sizeof text, "ended");
    else if (line) snprintf(text, sizeof text, "line %d", line);
    else text[0] = 0;
    draw_text(x0 + 106, y, text, C_DIM);
    int32_t v = (int32_t)m->x[I->selected];
    if (v >= 32 && v < 127) snprintf(text, sizeof text, "%s = %d '%c'", rv_register_name(I->selected), v, (char)v);
    else snprintf(text, sizeof text, "%s = %d", rv_register_name(I->selected), v);
    draw_text(x0, y + ROW_H, text, 0x000080);
}

static uint32_t ide_mem_top(const Ide *I) {
    uint32_t base = I->mem_mode == MEM_STACK ? (I->session ? I->machine.x[2] & ~7u : RV_SP_VALUE & ~7u)
                    : I->mem_mode == MEM_ADDRESS ? I->mem_address
                                                 : (I->built ? I->program.data_base : RV_DATA_BASE);
    return base + (uint32_t)(I->mem_scroll * 8);
}

static void ide_draw_memory(Ide *I) {
    Rect r = I->mem_r;
    ide_panel_head(r.x, r.y, r.w, "Memory");
    static const char *const labels[3] = { "Data", "Stack", "Go to" };
    for (int i = 0; i < 3; i++) {
        Rect b = I->mem_buttons[i];
        draw_button(b.x, b.y, b.w, b.h, labels[i], i == I->mem_mode);
    }
    draw_field(r.x, r.y + HEAD_H, r.w, r.h - HEAD_H, WHITE);
    int rows = max_int(0, (r.h - HEAD_H - 6) / ROW_H), x0 = r.x + 4, y0 = r.y + HEAD_H + 3;
    if (!I->session) {
        draw_text(x0, y0, "Build to see it.", C_DIM);
        return;
    }
    uint32_t top = ide_mem_top(I), sp = I->machine.x[2];
    int same = top == I->mem_shown_at;
    gfx_clip(rect(r.x + 2, y0, r.w - 4, r.h - HEAD_H - 4));
    for (int row = 0; row < rows && row < 16; row++) {
        uint32_t address = top + (uint32_t)row * 8;
        int y = y0 + row * ROW_H;
        char text[16];
        snprintf(text, sizeof text, "%08x", (unsigned)address);
        draw_text(x0, y, text, address == (sp & ~7u) ? 0x0000c0 : C_DIM);
        for (int k = 0; k < 2; k++) {
            uint32_t word = 0, at = address + (uint32_t)k * 4;
            int mapped = rv_read(&I->machine, at, &word, 4);
            int index = row * 2 + k, changed = same && mapped && word != I->mem_shown[index];
            if (mapped) snprintf(text, sizeof text, "%08x", (unsigned)word);
            else snprintf(text, sizeof text, "--------");
            if (at == sp) fill_rect(x0 + 72 + k * 72 - 1, y, 66, ROW_H, 0xdde6ff);
            draw_text(x0 + 72 + k * 72, y, text, changed ? C_CHANGED : mapped ? BLACK : C_DIM);
            I->mem_shown[index] = word;
        }
    }
    gfx_clip(rect(0, 0, I->w, I->h));
    I->mem_shown_at = top;
}

static void ide_draw_side_tabs(Ide *I) {
    Rect r = I->side_r;
    fill_rect(r.x, r.y, r.w, HEAD_H, FACE);
    hline(r.x, r.y + HEAD_H - 1, r.w, WHITE);
    static const char *const labels[2] = { "Registers", "Bitmap" };
    for (int i = 0; i < 2; i++) {
        Rect t = I->side_tabs[i];
        if (i == I->side_tab) {
            hline(t.x, t.y, t.w, WHITE);
            vline(t.x, t.y, t.h, WHITE);
            vline(t.x + t.w - 1, t.y, t.h, BLACK);
            draw_text_bold(t.x + (t.w - text_width(labels[i])) / 2, t.y + 2, labels[i], BLACK);
        } else {
            hline(t.x + 2, t.y + 2, t.w - 4, WHITE);
            vline(t.x + t.w - 2, t.y + 3, t.h - 4, SHADOW);
            draw_text(t.x + (t.w - text_width(labels[i])) / 2, t.y + 3, labels[i], BLACK);
        }
    }
}

static void ide_paint_side_bitmap(Ide *I, int first, int last);

static void ide_draw_side(Ide *I) {
    os_window_draw(I->win);
    fill_rect(I->side_r.x - GAP, TOOL_H, GAP, I->h - TOOL_H, FACE);
    ide_draw_side_tabs(I);
    if (I->side_tab == SIDE_REGISTERS) {
        ide_draw_registers(I);
        fill_rect(I->mem_r.x, I->mem_r.y - GAP, I->mem_r.w, GAP, FACE);
        ide_draw_memory(I);
    } else {
        Rect r = I->side_r;
        draw_field(r.x, r.y + HEAD_H, r.w, r.h - HEAD_H, BLACK);
        ide_paint_side_bitmap(I, 0, 0x7fffffff);
        os_window_draw(I->win);
    }
    os_window_update(I->win, I->side_r.x - GAP, TOOL_H, SIDE_W + GAP, I->h - TOOL_H);
}

/* --- the console: the output, the line being typed and the note after them, as one text */

static int con_note_start(const Ide *I) { return I->console_length + I->input_length; }
static int con_total(const Ide *I) {
    int total = con_note_start(I);
    if (I->note[0]) total += 1 + (int)strlen(I->note);
    return total;
}
static char con_at(const Ide *I, int i) {
    if (i < I->console_length) return I->console[i];
    i -= I->console_length;
    if (i < I->input_length) return I->input[i];
    i -= I->input_length;
    return i == 0 ? '\n' : I->note[i - 1];
}
static uint32_t con_colour(const Ide *I, int i) {
    if (i < I->console_length) return BLACK;
    if (i < con_note_start(I)) return C_INPUT;
    return I->note_kind == NOTE_ERROR ? C_FAULT : C_NOTE;
}

/* Lays out the logical line [a, b) in rows of `cols`; draws its rows from `skip` on, from *y down
 * while they fit above `bottom`, when `draw`; answers how many rows it takes. */
static int con_line(Ide *I, int a, int b, int cols, int draw, int x0, int *y, int skip, int bottom) {
    int row = 0, column = 0;
    for (int i = a; i < b; i++) {
        char c = con_at(I, i);
        if (c == '\r') continue;
        int width = c == '\t' ? 8 - column % 8 : 1;
        if (column + width > cols && column > 0) {
            row++;
            column = 0;
            if (draw && row > skip) *y += ROW_H;
        }
        if (draw && row >= skip && *y + ROW_H <= bottom && c != '\t')
            draw_char(x0 + column * FONT_WIDTH, *y, (unsigned char)c < 32 ? '?' : c, con_colour(I, i));
        column += width;
    }
    if (draw && row >= skip) *y += ROW_H;
    return row + 1;
}

static void ide_draw_console(Ide *I, Rect area) {
    int cols = max_int(1, (area.w - 8) / FONT_WIDTH), rows = max_int(1, (area.h - 4) / ROW_H);
    int total = con_total(I), need = rows + I->console_scroll, typed = con_note_start(I);
    /* back from the end, a logical line at a time, until the lines fill the rows wanted */
    int start = 0, have = 0, end = total;
    for (;;) {
        int a = end;
        while (a > 0 && con_at(I, a - 1) != '\n') a--;
        have += con_line(I, a, end, cols, 0, 0, NULL, 0, 0);
        start = a;
        if (have >= need || a == 0) break;
        end = a - 1;
    }
    int skip = max_int(0, have - need), y = area.y + 2, bottom = area.y + area.h - 2;
    int caret_x = -1, caret_y = -1;
    for (int a = start; a <= total && y + ROW_H <= bottom;) {
        int b = a;
        while (b < total && con_at(I, b) != '\n') b++;
        int top_y = y, rows_of = con_line(I, a, b, cols, 1, area.x + 4, &y, skip, bottom);
        if (I->machine.want && a <= typed && typed <= b) {
            /* the caret after what is typed */
            int column = 0, row = 0;
            for (int i = a; i < typed; i++) {
                char c = con_at(I, i);
                int width = c == '\t' ? 8 - column % 8 : 1;
                if (column + width > cols && column > 0) row++, column = 0;
                column += width;
            }
            if (column >= cols) row++, column = 0;
            caret_x = area.x + 4 + column * FONT_WIDTH;
            caret_y = top_y + (row - skip) * ROW_H;
        }
        skip = max_int(0, skip - rows_of);
        a = b + 1;
    }
    if (caret_y >= area.y && caret_y + ROW_H <= bottom && I->focus == FOCUS_CONSOLE && I->blink)
        fill_rect(caret_x, caret_y + 1, FONT_WIDTH, ROW_H - 2, C_INPUT);
}

/* The problems of the last Build: errors, then warnings, and how many errors were not kept. */
static int ide_problem_count(const Ide *I) {
    int errors = min_int(I->program.error_count, RV_MAX_ERRORS);
    return errors + I->program.warning_count + (I->program.error_count > errors);
}

/* Problem `i`'s text and line (0 for none); answers whether it is an error. */
static int ide_problem(const Ide *I, int i, char *text, int size, int *line) {
    int errors = min_int(I->program.error_count, RV_MAX_ERRORS), warnings = I->program.warning_count;
    if (i < errors) {
        *line = I->program.errors[i].line;
        snprintf(text, (size_t)size, "Line %d: %s", *line, I->program.errors[i].text);
        return 1;
    }
    if (i < errors + warnings) {
        *line = I->program.warnings[i - errors].line;
        snprintf(text, (size_t)size, "Line %d, warning: %s", *line, I->program.warnings[i - errors].text);
        return 0;
    }
    *line = 0;
    snprintf(text, (size_t)size, "... and %d more errors", I->program.error_count - errors);
    return 1;
}

/* Draws text cut into rows of `cols` at its spaces, from y down, when `draw`; answers the rows. */
static int ide_wrap(const char *text, int cols, int draw, int x, int y, int bottom, uint32_t colour) {
    int rows = 0, length = (int)strlen(text);
    for (int at = 0; at < length; rows++) {
        int take = min_int(cols, length - at);
        if (at + take < length) {
            int space = take;
            while (space > cols / 2 && text[at + space] != ' ') space--;
            if (text[at + space] == ' ') take = space;
        }
        if (draw && y + (rows + 1) * ROW_H <= bottom) draw_text_n(x, y + rows * ROW_H, text + at, take, colour);
        at += take;
        while (at < length && text[at] == ' ') at++;
    }
    return max_int(rows, 1);
}

static void ide_draw_problems(Ide *I, Rect area) {
    int y = area.y + 2, bottom = area.y + area.h - 2, cols = max_int(8, (area.w - 20) / FONT_WIDTH);
    if (!I->built) {
        draw_text(area.x + 4, y, "Build to see the errors, if any.", C_DIM);
        return;
    }
    int total = ide_problem_count(I);
    if (total == 0) {
        draw_text(area.x + 4, y, "No errors.", C_DIM);
        return;
    }
    char text[200];
    for (int i = I->problems_top; i < total && y + ROW_H <= bottom; i++) {
        int line, error = ide_problem(I, i, text, sizeof text, &line);
        fill_rect(area.x + 4, y + 3, 6, 6, error ? C_FAULT : 0xe0a000);
        y += ide_wrap(text, cols, 1, area.x + 14, y, bottom, error ? BLACK : 0x604000) * ROW_H;
    }
}

/* The line of the problem drawn at y, or 0. */
static int ide_problem_line_at(const Ide *I, int y) {
    Rect area = rect(I->console_r.x + 2, I->console_r.y + 2, I->console_r.w - 4, I->console_r.h - 4);
    int top = area.y + 2, cols = max_int(8, (area.w - 20) / FONT_WIDTH), total = ide_problem_count(I);
    char text[200];
    for (int i = I->problems_top; i < total; i++) {
        int line;
        ide_problem(I, i, text, sizeof text, &line);
        int rows = ide_wrap(text, cols, 0, 0, 0, 0, 0);
        if (y >= top && y < top + rows * ROW_H) return line;
        top += rows * ROW_H;
    }
    return 0;
}

static void ide_draw_bottom(Ide *I) {
    os_window_draw(I->win);
    Rect b = I->bottom_r;
    fill_rect(b.x, b.y - GAP, b.w, GAP, FACE);
    fill_rect(b.x, b.y, b.w, TAB_H, FACE);
    hline(b.x, b.y + TAB_H - 1, b.w, WHITE);
    for (int i = 0; i < 2; i++) {
        Rect t = I->tabs[i];
        char label[32];
        if (i == TAB_CONSOLE) snprintf(label, sizeof label, "Console");
        else if (I->built && (I->program.error_count || I->program.warning_count))
            snprintf(label, sizeof label, "Problems (%d)", I->program.error_count + I->program.warning_count);
        else snprintf(label, sizeof label, "Problems");
        if (i == I->tab) {
            fill_rect(t.x, t.y, t.w, t.h + 1, FACE);
            hline(t.x, t.y, t.w, WHITE);
            vline(t.x, t.y, t.h, WHITE);
            vline(t.x + t.w - 1, t.y, t.h, BLACK);
        } else {
            hline(t.x + 2, t.y + 2, t.w - 4, WHITE);
            vline(t.x + t.w - 2, t.y + 3, t.h - 4, SHADOW);
        }
        draw_text(t.x + (t.w - text_width(label)) / 2, t.y + 3, label,
                  i == TAB_PROBLEMS && I->built && I->program.error_count ? C_FAULT : BLACK);
    }
    Rect c = I->console_r;
    draw_field(c.x, c.y, c.w, c.h, WHITE);
    if (I->tab == TAB_CONSOLE && I->focus == FOCUS_CONSOLE && os_window_focused(I->win))
        frame_rect(c.x + 2, c.y + 2, c.w - 4, c.h - 4, 0x8090c0);
    Rect inner = rect(c.x + 2, c.y + 2, c.w - 4, c.h - 4);
    gfx_clip(inner);
    if (I->tab == TAB_CONSOLE) ide_draw_console(I, inner);
    else ide_draw_problems(I, inner);
    gfx_clip(rect(0, 0, I->w, I->h));
    I->console_dirty = 0;
    os_window_update(I->win, b.x, b.y - GAP, b.w, b.h + GAP);
}

static void ide_title(Ide *I) {
    char title[200];
    snprintf(title, sizeof title, "%s%s - Assembly Editor", I->modified ? "*" : "",
             I->path[0] ? os_base_name(I->path) : "Untitled");
    os_window_set_title(I->win, title);
}

static void ide_update_menus(Ide *I) {
    I->run_menu[2].flags = I->running ? 0 : MENU_DISABLED;          /* Pause */
    I->run_menu[4].flags = ide_button_enabled(I, TB_BACK) ? 0 : MENU_DISABLED;
    I->run_menu[5].flags = I->session ? 0 : MENU_DISABLED;          /* Stop */
    I->run_menu[6].flags = I->session && !I->edited ? 0 : MENU_DISABLED;
    I->run_menu[11].flags = I->history ? MENU_CHECKED : 0;
    I->view_menu[0].flags = I->bitmap_win ? MENU_CHECKED : 0;
    for (int i = 0; i < 3; i++) I->view_menu[2 + i].flags = I->mem_mode == i ? MENU_CHECKED : 0;
}

static void ide_draw_all(Ide *I) {
    ide_update_menus(I);
    ide_draw_toolbar(I);
    ide_draw_editor(I);
    ide_draw_side(I);
    ide_draw_bottom(I);
}

/* --- the bitmap display: words of memory, each a square of its colour */

/* Paints the bitmap display into a picture (`stride` in pixels), at most w by h: each word a sw by
 * sh square or, with sw 0, the grid shrunk to fit, the word nearest each pixel; only the grid's
 * rows from `first` to `last`. Answers the size of the whole picture in *pw and *ph, and the
 * picture's rows painted in *y0 and *y1 (inclusive, *y1 < *y0 for none). */
static void ide_paint_grid(Ide *I, uint32_t *to, int stride, int w, int h, int sw, int sh, int first, int last,
                           int *pw, int *ph, int *y0, int *y1) {
    RvScreen s = ide_screen(I);
    int cols, rows;
    ide_grid(I, &cols, &rows);
    uint32_t step = 0;            /* words a pixel, 16.16, when shrinking */
    if (sw <= 0) {
        step = (uint32_t)max_int((cols << 16) / max_int(1, w), (rows << 16) / max_int(1, h)) + 1;
        sw = sh = 1;
    }
    int dw = step ? (int)(((uint64_t)cols << 16) / step) : cols * sw;
    int dh = step ? (int)(((uint64_t)rows << 16) / step) : rows * sh;
    dw = min_int(dw, min_int(w, 1024));
    dh = min_int(dh, h);
    *pw = dw;
    *ph = dh;
    *y0 = dh;
    *y1 = -1;
    if (!I->session) {
        for (int y = 0; y < dh; y++) memset(to + (size_t)y * stride, 0, (size_t)dw * 4);
        *y0 = 0;
        *y1 = dh - 1;
        return;
    }
    uint32_t *row = I->bitmap_row;
    for (int y = step ? 0 : min_int(dh, max_int(0, first) * sh); y < dh;) {
        int r = step ? (int)(((uint64_t)y * step) >> 16) : y / sh;
        int count = step ? 1 : min_int(sh - y % sh, dh - y);
        if (r > last) break;
        if (r < first) {
            y += count;
            continue;
        }
        *y0 = min_int(*y0, y);
        *y1 = y + count - 1;
        uint32_t address = s.base + (uint32_t)r * (uint32_t)cols * 4, tag = 0xffffffffu;
        const uint8_t *page = NULL;
        for (int x = 0, last = -1; x < dw;) {
            int c = step ? (int)(((uint64_t)x * step) >> 16) : x / sw;
            int repeat = step ? 1 : min_int(sw, dw - x);
            uint32_t colour = row[x - (x > 0)];
            if (c != last) {
                uint32_t at = address + (uint32_t)c * 4, word = 0;
                if ((at >> RV_PAGE_BITS) != tag) {
                    tag = at >> RV_PAGE_BITS;
                    page = rv_page(&I->machine, at);
                }
                if (page) memcpy(&word, page + (at & (RV_PAGE_SIZE - 1)), 4);
                colour = word & 0xffffff;
                last = c;
            }
            for (int k = 0; k < repeat; k++) row[x + k] = colour;
            x += repeat;
        }
        copy_rect32(to + (size_t)y * stride, (size_t)stride * 4, row, 0, (size_t)dw, (size_t)count);
        y += count;
    }
}

/* The bitmap in the side, as large as it fits in whole pixels a word, centred, with its size under
 * it: all of it, or the grid's rows from `first` to `last`, which it shows again. */
static void ide_paint_side_bitmap(Ide *I, int first, int last) {
    Rect r = I->side_r;
    int x0 = r.x + 2, y0 = r.y + HEAD_H + 2, w = r.w - 4, h = r.h - HEAD_H - 4 - ROW_H * 2;
    int cols, rows;
    ide_grid(I, &cols, &rows);
    RvScreen s = ide_screen(I);
    /* whole pixels a word, as many as fit up to the unit's, keeping the unit's shape */
    int uw = max_int(1, s.unit_w), uh = max_int(1, s.unit_h), scale_w = min_int(uw, w / cols), scale_h = 0;
    for (; scale_w >= 1; scale_w--) {
        scale_h = max_int(1, scale_w * uh / uw);
        if (rows * scale_h <= h) break;
    }
    int px;
    if (scale_w < 1) {
        /* larger than the side: shrunk, centred by the size it shrinks to */
        scale_w = scale_h = 0;
        uint32_t step = (uint32_t)max_int((cols << 16) / w, (rows << 16) / h) + 1;
        px = x0 + (w - (int)(((uint64_t)cols << 16) / step)) / 2;
    } else {
        px = x0 + (w - cols * scale_w) / 2;
    }
    int pw, ph, from, to, window_w, window_h;
    uint32_t *pixels = os_window_pixels(I->win, &window_w, &window_h);
    ide_paint_grid(I, pixels + (size_t)(y0 + 2) * window_w + px, window_w, w, h, scale_w, scale_h, first, last,
                   &pw, &ph, &from, &to);
    if (first <= 0 && last >= rows - 1) {
        os_window_draw(I->win);
        gfx_clip(rect(x0, y0, w, r.h - HEAD_H - 4));
        char text[64];
        snprintf(text, sizeof text, "%d x %d words", cols, rows);
        draw_text(x0 + (w - text_width(text)) / 2, y0 + 6 + ph, text, 0xc0c0c0);
        snprintf(text, sizeof text, "from %08x%s", (unsigned)s.base, s.present ? "" : " (no @screen)");
        draw_text(x0 + (w - text_width(text)) / 2, y0 + 6 + ph + ROW_H, text, 0xc0c0c0);
        gfx_clip(rect(0, 0, I->w, I->h));
        os_window_update(I->win, r.x, r.y + HEAD_H, r.w, r.h - HEAD_H);
    } else if (to >= from) {
        os_window_update(I->win, px, y0 + 2 + from, pw, to - from + 1);
    }
}

/* The bitmap display where it shows, the side and its own window: all of it, or with `full` 0
 * the rows that the program stored in since it last showed. */
static void ide_bitmap_draw_rows(Ide *I, int full) {
    int cols, rows, first = 0, last = 0x7fffffff;
    ide_grid(I, &cols, &rows);
    if (!full && I->session) {
        if (!I->machine.watch_dirty) return;
        uint32_t base = ide_screen(I).base, row_bytes = (uint32_t)cols * 4;
        uint32_t from = I->machine.watch_from, to = I->machine.watch_to;
        first = from < base ? 0 : (int)min_int(rows - 1, (int)((from - base) / row_bytes));
        last = to < base ? -1 : (int)min_int(rows - 1, (int)((to - base) / row_bytes));
    }
    if (I->side_tab == SIDE_BITMAP) {
        if (full) {
            os_window_draw(I->win);
            Rect r = I->side_r;
            draw_field(r.x, r.y + HEAD_H, r.w, r.h - HEAD_H, BLACK);
        }
        ide_paint_side_bitmap(I, first, last);
    }
    if (I->bitmap_win) {
        int w, h, pw, ph, from, to;
        uint32_t *pixels = os_window_pixels(I->bitmap_win, &w, &h);
        RvScreen s = ide_screen(I);
        if (full) {
            os_window_draw(I->bitmap_win);
            fill_rect(0, 0, w, h, BLACK);
        }
        ide_paint_grid(I, pixels, w, w, h, max_int(1, s.unit_w), max_int(1, s.unit_h), first, last, &pw, &ph, &from, &to);
        if (full) os_window_update_all(I->bitmap_win);
        else if (to >= from) os_window_update(I->bitmap_win, 0, from, pw, to - from + 1);
    }
    I->machine.watch_dirty = 0;
}

static void ide_bitmap_draw(Ide *I) { ide_bitmap_draw_rows(I, 1); }

/* ------------------------------------------------------------------ files */

static int ide_load(Ide *I, const char *path) {
    long size;
    char *data = os_read_file(path, &size);
    if (!data) {
        snprintf(I->status, sizeof I->status, "Cannot open %.60s", os_base_name(path));
        return 0;
    }
    ide_end_session(I);
    I->built = 0;
    ed_set_text(I, data, size);
    os_free(data);
    copy_text(I->path, sizeof I->path, path);
    con_clear(I);
    I->focus = FOCUS_EDITOR;
    snprintf(I->status, sizeof I->status, "Opened %.60s: Build, then Run", os_base_name(path));
    return 1;
}

/* Saves under the file's name, or asks for one; answers 1 when saved. */
static int ide_save(Ide *I, int ask) {
    if (ask || !I->path[0]) {
        char path[160];
        copy_text(path, sizeof path, I->path[0] ? I->path : "code/program.riscv");
        if (!os_file_dialog(I->win, 1, "Save As", ".riscv", path, sizeof path)) return 0;
        copy_text(I->path, sizeof I->path, path);
    }
    if (os_write_file(I->path, I->text, I->length) != 0) {
        os_message_box(I->win, "Assembly Editor", "The file could not be saved.", MB_OK, MB_WARNING);
        return 0;
    }
    I->modified = 0;
    snprintf(I->status, sizeof I->status, "Saved %.60s, %d bytes", os_base_name(I->path), I->length);
    ide_title(I);
    return 1;
}

/* Before the text goes: answers 1 to go on, after saving if asked to. */
static int ide_confirm(Ide *I) {
    if (!I->modified) return 1;
    char question[200];
    snprintf(question, sizeof question, "Do you want to save the changes to %.60s?",
             I->path[0] ? os_base_name(I->path) : "Untitled");
    int answer = os_message_box(I->win, "Assembly Editor", question, MB_YES_NO_CANCEL, MB_QUESTION);
    if (answer == ID_YES) return ide_save(I, 0);
    return answer == ID_NO;
}

static void ide_open_path(Ide *I, const char *path) {
    if (!ide_confirm(I)) return;
    ide_load(I, path);
    ide_title(I);
    ide_draw_all(I);
}

static void ide_examples(Ide *I) {
    DirEntry entries[IDE_EXAMPLES];
    int count = os_list_dir("code", entries, IDE_EXAMPLES), n = 0;
    for (int i = 0; i < count && n < IDE_EXAMPLES; i++) {
        if (entries[i].is_dir || !ends_with(entries[i].name, ".riscv")) continue;
        copy_text(I->example_names[n], sizeof I->example_names[n], entries[i].name);
        I->example_items[n] = (MenuItem){ I->example_names[n], IC_EXAMPLE + n, 0, NULL, NULL };
        n++;
    }
    if (n == 0) I->example_items[n++] = (MenuItem){ "(none in code/)", 0, MENU_DISABLED, NULL, NULL };
    I->example_items[n] = (MenuItem){ NULL, 0, 0, NULL, NULL };
}

/* ------------------------------------------------------------------ commands */

/* Editing the text ends the program built from it: Run builds the new text. */
static int ide_can_edit(Ide *I) {
    if (I->session) {
        ide_end_session(I);
        I->note[0] = 0;
        snprintf(I->status, sizeof I->status, "Edited: Run builds it again");
        ide_draw_side(I);
        ide_draw_bottom(I);
    }
    return 1;
}

static void ide_copy(Ide *I) {
    if (sel_from(I) < sel_to(I)) os_clipboard_set_text(I->text + sel_from(I), sel_to(I) - sel_from(I));
}

static void ide_paste(Ide *I) {
    int length;
    char *text = os_clipboard_text(&length);
    if (!text) {
        os_beep();
        return;
    }
    ed_insert(I, text, length);
    os_free(text);
}

static void ide_goto_address(Ide *I) {
    char text[64] = "";
    if (!os_input_box(I->win, "Memory", "Address (0x10010000) or label:", text, sizeof text)) return;
    char *end;
    unsigned long value = strtoul(text, &end, 0);
    int found = *text && !*end;
    for (int i = 0; !found && i < I->program.symbol_count; i++)
        if (strcmp(I->program.symbols[i].name, text) == 0) value = I->program.symbols[i].address, found = 1;
    if (!found) {
        snprintf(I->status, sizeof I->status, "No address or label '%.40s'", text);
        return;
    }
    I->mem_mode = MEM_ADDRESS;
    I->mem_address = (uint32_t)value & ~7u;
    I->mem_scroll = 0;
}

static void ide_ecalls(Ide *I) {
    os_message_box(I->win, "Ecalls",
                   "ecall runs the service in a7, with a0, a1:\n"
                   "1 int  4 string  11 char  34 hex\n"
                   "35 binary  36 unsigned  5 read int\n"
                   "8 read string (a0 buffer, a1 size)\n"
                   "12 read char  9 sbrk  10 exit\n"
                   "93 exit with code a0  32 sleep a0 ms\n"
                   "30 time (a0 low, a1 high)\n"
                   "31, 33 MIDI note  40 seed  41 random\n"
                   "42 random from 0 to a1 - 1",
                   MB_OK, MB_INFO);
}

static void ide_devices(Ide *I) {
    os_message_box(I->win, "Devices",
                   "Keyboard: the word at 0xffff0000 is 1\n"
                   "when a key waits in 0xffff0004, which\n"
                   "a load takes.\n"
                   "Display: a byte stored at 0xffff000c\n"
                   "prints in the console.\n"
                   "Bitmap: a comment such as\n"
                   "# @screen width=64 height=64 base=pic\n"
                   "shows the words from pic, 0x00RRGGBB.",
                   MB_OK, MB_INFO);
}

static void ide_command(Ide *I, int id) {
    if (id >= IC_EXAMPLE && id < IC_EXAMPLE + IDE_EXAMPLES) {
        char path[160];
        snprintf(path, sizeof path, "code/%s", I->example_names[id - IC_EXAMPLE]);
        ide_open_path(I, path);
        return;
    }
    switch (id) {
    case IC_NEW:
        if (!ide_confirm(I)) return;
        ide_end_session(I);
        I->built = 0;
        ed_set_text(I, IDE_TEMPLATE, (long)strlen(IDE_TEMPLATE));
        I->path[0] = 0;
        con_clear(I);
        snprintf(I->status, sizeof I->status, "New program");
        break;
    case IC_OPEN: {
        char path[160];
        copy_text(path, sizeof path, I->path[0] ? I->path : "code");
        if (os_file_dialog(I->win, 0, "Open", ".riscv", path, sizeof path)) ide_open_path(I, path);
        return;
    }
    case IC_SAVE: ide_save(I, 0); break;
    case IC_SAVE_AS: ide_save(I, 1); break;
    case IC_EXIT:
        if (ide_confirm(I)) os_exit(0);
        return;
    case IC_CUT:
        if (!ide_can_edit(I)) return;
        ide_copy(I);
        ed_delete(I, sel_from(I), sel_to(I));
        break;
    case IC_COPY: ide_copy(I); break;
    case IC_PASTE:
        if (!ide_can_edit(I)) return;
        ide_paste(I);
        break;
    case IC_SELECT_ALL:
        I->anchor = 0;
        I->cursor = I->length;
        break;
    case IC_GOTO: {
        char text[16] = "";
        if (os_input_box(I->win, "Go to Line", "Line number:", text, sizeof text) && atoi(text) > 0) {
            I->cursor = I->anchor = I->starts[clamp_int(atoi(text) - 1, 0, I->lines - 1)];
            ed_reveal(I, ed_line_of(I, I->cursor));
            I->focus = FOCUS_EDITOR;
        }
        break;
    }
    case IC_BUILD:
        ide_build(I);
        ide_draw_all(I);
        return;
    case IC_RUN: ide_run(I); return;
    case IC_PAUSE: ide_pause(I); return;
    case IC_STEP: ide_step(I); return;
    case IC_BACK: ide_back(I); return;
    case IC_STOP: ide_stop(I); return;
    case IC_RESET:
        if (I->session && !I->edited) {
            I->running = 0;
            ide_reset(I);
            snprintf(I->status, sizeof I->status, "Back at the start, line %d", rv_line_of(&I->program, I->machine.pc));
            ed_reveal(I, ide_pc_line(I));
        }
        break;
    case IC_BREAK: ide_toggle_break(I, ed_line_of(I, I->cursor)); break;
    case IC_CLEAR_BREAKS:
        I->break_count = 0;
        if (I->session) rv_clear_breakpoints(&I->machine);
        snprintf(I->status, sizeof I->status, "No breakpoints");
        break;
    case IC_HISTORY:
        I->history = !I->history;
        if (I->session) rv_journal(&I->machine, I->history);
        snprintf(I->status, sizeof I->status, I->history ? "Step back keeps a history of the steps"
                                                         : "No history: runs faster, Step back is off");
        break;
    case IC_BITMAP:
        if (I->bitmap_win) ide_bitmap_close(I);
        else ide_bitmap_open(I);
        break;
    case IC_MEM_DATA: case IC_MEM_STACK:
        I->mem_mode = id == IC_MEM_DATA ? MEM_DATA : MEM_STACK;
        I->mem_scroll = 0;
        break;
    case IC_MEM_ADDRESS: ide_goto_address(I); break;
    case IC_ECALLS: ide_ecalls(I); return;
    case IC_DEVICES: ide_devices(I); return;
    case IC_ABOUT:
        os_message_box(I->win, "About Assembly Editor",
                       "Assembly Editor for asmOS.\n"
                       "RISC-V (RV32IM) programs as RARS runs\n"
                       "them: a program runs the same in the\n"
                       "web editor's RISC-V Target.",
                       MB_OK, MB_INFO);
        return;
    }
    I->blink = 1;
    ed_follow(I);
    ide_title(I);
    ide_draw_all(I);
}

/* ------------------------------------------------------------------ keys */

/* The keys the whole window takes, whatever has the focus; answers 1 for one of them. The run
 * keys are RARS's (F3 assemble, F5 go, F7 step, F8 backstep, F9 pause, F11 stop, F12 reset); F10
 * is asmOS's, for the menu bar, and the browser keeps Ctrl+N for itself. */
static int ide_global_key(Ide *I, const Message *m) {
    int shift = m->mods & MOD_SHIFT, ctrl = m->mods & MOD_CTRL;
    int id = 0;
    switch (m->key) {
    case KEY_F2: id = IC_BREAK; break;
    case KEY_F3: id = IC_BUILD; break;
    case KEY_F5: id = shift ? IC_STOP : IC_RUN; break;
    case KEY_F7: id = IC_STEP; break;
    case KEY_F8: id = IC_BACK; break;
    case KEY_F9: id = IC_PAUSE; break;
    case KEY_F11: id = IC_STOP; break;
    case KEY_F12: id = IC_RESET; break;
    }
    if (ctrl) {
        switch (m->key) {
        case KEY_B: id = IC_BUILD; break;
        case KEY_S: id = IC_SAVE; break;
        case KEY_O: id = IC_OPEN; break;
        case KEY_G: id = IC_GOTO; break;
        }
    }
    if (!id) return 0;
    ide_command(I, id);
    return 1;
}

/* A key for the program: the line or the character a read ecall waits for, else the keyboard
 * receiver's queue. Answers 1 when the program took it. */
static int ide_program_key(Ide *I, const Message *m) {
    if (!ide_alive(I) || I->edited) return 0;
    RvMachine *machine = &I->machine;
    int enter = m->key == KEY_ENTER || m->key == KEY_KPENTER;
    if (machine->want == RV_WANT_CHAR) {
        if (!enter && !m->ch) return 1;
        rv_supply_char(machine, enter ? '\n' : m->ch);
        if (machine->faulted) ide_after_stop(I, RV_STOP_FAULT);
        else if (!I->running) ide_paused_status(I, "Paused");
        else snprintf(I->status, sizeof I->status, "Running");
        ide_draw_all(I);
        return 1;
    }
    if (machine->want) {
        if (enter) {
            ide_submit(I);
        } else if (m->key == KEY_BACKSPACE) {
            if (I->input_length) I->input_length--;
        } else if (m->ch >= 32 && m->ch < 127 && I->input_length < IDE_INPUT - 1) {
            I->input[I->input_length++] = (char)m->ch;
        }
        I->blink = 1;
        I->console_scroll = 0;
        ide_draw_bottom(I);
        return 1;
    }
    int ch = enter ? '\n' : m->key == KEY_BACKSPACE ? 8 : m->key == KEY_TAB ? '\t' : m->key == KEY_ESC ? 27 : m->ch;
    if (!ch) return 1;
    rv_key_push(machine, ch);
    return 1;
}

static void ide_editor_key(Ide *I, const Message *m) {
    int before = ed_line_of(I, I->cursor), had_selection = I->anchor != I->cursor, was_modified = I->modified;
    int had_session = I->session;
    int vertical = 0, moving = 0, lines = I->lines, ctrl = m->mods & MOD_CTRL;
    if (ctrl) {
        int id = m->key == KEY_C ? IC_COPY : m->key == KEY_X ? IC_CUT : m->key == KEY_V ? IC_PASTE
                 : m->key == KEY_A ? IC_SELECT_ALL : 0;
        if (id) {
            ide_command(I, id);
            return;
        }
        if (m->key == KEY_HOME || m->key == KEY_END) {
            I->cursor = m->key == KEY_HOME ? 0 : I->length;
            moving = 1;
        } else {
            return;
        }
    } else {
        int start = I->starts[before];
        switch (m->key) {
        case KEY_LEFT: I->cursor = max_int(0, I->cursor - 1); moving = 1; break;
        case KEY_RIGHT: I->cursor = min_int(I->length, I->cursor + 1); moving = 1; break;
        case KEY_HOME: {
            /* to the first character that is not a space, then to the line's start */
            int first = start;
            while (first < I->length && I->text[first] == ' ') first++;
            I->cursor = I->cursor == first ? start : first;
            moving = 1;
            break;
        }
        case KEY_END: I->cursor = ed_line_end(I, before); moving = 1; break;
        case KEY_UP: case KEY_DOWN: case KEY_PAGEUP: case KEY_PAGEDOWN: {
            int rows = ed_rows(I);
            int step = m->key == KEY_UP ? -1 : m->key == KEY_DOWN ? 1 : m->key == KEY_PAGEUP ? -rows : rows;
            I->cursor = ed_index_at(I, before + step, I->goal);
            if (m->key == KEY_PAGEUP || m->key == KEY_PAGEDOWN)
                I->top = clamp_int(I->top + step, 0, max_int(0, I->lines - 1));
            moving = vertical = 1;
            break;
        }
        case KEY_BACKSPACE:
            if (!ide_can_edit(I)) return;
            if (had_selection) ed_delete(I, sel_from(I), sel_to(I));
            else if (I->cursor > 0) ed_delete(I, I->cursor - 1, I->cursor);
            break;
        case KEY_DELETE:
            if (!ide_can_edit(I)) return;
            if (had_selection) ed_delete(I, sel_from(I), sel_to(I));
            else ed_delete(I, I->cursor, min_int(I->length, I->cursor + 1));
            break;
        case KEY_ENTER: case KEY_KPENTER: {
            if (!ide_can_edit(I)) return;
            /* the new line starts where this one's text does */
            char indent[64];
            int n = 0;
            indent[n++] = '\n';
            for (int i = start; i < I->length && I->text[i] == ' ' && n < 60 && i < sel_from(I); i++)
                indent[n++] = ' ';
            ed_insert(I, indent, n);
            break;
        }
        case KEY_TAB: {
            if (!ide_can_edit(I)) return;
            ed_insert(I, "\t", 1);
            break;
        }
        case KEY_ESC:
            I->anchor = I->cursor;
            break;
        default:
            if (m->ch >= 32 && m->ch < 127) {
                if (!ide_can_edit(I)) return;
                char ch = (char)m->ch;
                ed_insert(I, &ch, 1);
            } else {
                return;
            }
        }
    }
    /* an edit that ended the program takes its line's colour away too */
    int whole = had_selection || I->lines != lines || I->anchor != I->cursor || had_session != I->session;
    if (moving && !(m->mods & MOD_SHIFT)) I->anchor = I->cursor;
    else if (moving) whole = 1;
    if (!vertical) I->goal = I->cursor - I->starts[ed_line_of(I, I->cursor)];
    I->blink = 1;
    if (ed_follow(I)) whole = 1;
    int after = ed_line_of(I, I->cursor);
    if (I->modified != was_modified) ide_title(I);
    if (whole || moving) {
        if (whole || after != before) {
            ide_draw_editor(I);
            ide_draw_toolbar(I);
        } else {
            ide_draw_line(I, after);
        }
    } else {
        ide_draw_line(I, after);
        if (before != after) ide_draw_line(I, before);
        ide_draw_toolbar(I);
    }
}

static void ide_key(Ide *I, const Message *m) {
    if (ide_global_key(I, m)) return;
    if (I->focus == FOCUS_CONSOLE) {
        if (m->key == KEY_ESC && !(ide_alive(I) && I->machine.want)) {
            I->focus = FOCUS_EDITOR;
            ide_draw_all(I);
            return;
        }
        if (ide_program_key(I, m)) return;
        /* nothing runs: the keys go back to the editor */
        I->focus = FOCUS_EDITOR;
        ide_draw_bottom(I);
    }
    ide_editor_key(I, m);
}

/* ------------------------------------------------------------------ the pointer */

static void ide_mouse(Ide *I, const Message *m) {
    int changed;
    int clicked = track_buttons(&I->tracker, I->buttons, BUTTONS, m, &changed);
    if (changed) ide_draw_toolbar(I);
    if (clicked >= 0) {
        if (ide_button_enabled(I, clicked)) {
            static const int ids[BUTTONS] = { IC_BUILD, IC_RUN, IC_PAUSE, IC_STEP, IC_BACK, IC_STOP };
            ide_command(I, ids[clicked]);
        }
        return;
    }
    if (I->tracker.pressed >= 0 || (m->type == MSG_MOUSE_DOWN && rect_contains(I->tool_r, m->x, m->y))) return;

    if (scroll_bar_message(&I->scroll, m)) {
        if (I->scroll.top != I->top) {
            I->top = I->scroll.top;
            ide_draw_editor(I);
        } else {
            os_window_draw(I->win);
            ide_draw_scroll(I);
            os_window_update(I->win, I->scroll.r.x, I->scroll.r.y, I->scroll.r.w, I->scroll.r.h);
        }
        return;
    }
    if (m->type == MSG_MOUSE_WHEEL) {
        if (rect_contains(I->edit_r, m->x, m->y)) {
            I->top = clamp_int(I->top - m->wheel * 3, 0, max_int(0, I->lines - ed_rows(I) / 2));
            ide_draw_editor(I);
        } else if (rect_contains(I->console_r, m->x, m->y)) {
            if (I->tab == TAB_CONSOLE) I->console_scroll = max_int(0, I->console_scroll + m->wheel * 3);
            else I->problems_top = max_int(0, I->problems_top - m->wheel * 3);
            ide_draw_bottom(I);
        } else if (I->side_tab == SIDE_REGISTERS && rect_contains(I->mem_r, m->x, m->y)) {
            I->mem_scroll -= m->wheel * 2;
            ide_draw_side(I);
        }
        return;
    }
    if (m->type == MSG_MOUSE_UP) I->selecting = 0;
    int dragging = m->type == MSG_MOUSE_MOVE && I->selecting;
    if (m->type != MSG_MOUSE_DOWN && !dragging) return;

    if (dragging || rect_contains(I->text_r, m->x, m->y)) {
        int line = I->top + max_int(0, (m->y - I->text_r.y) / LINE_H);
        int column = I->left + max_int(0, (m->x - I->text_r.x - 2 + FONT_WIDTH / 2) / FONT_WIDTH);
        if (m->type == MSG_MOUSE_DOWN && m->button == BUTTON_RIGHT) {
            int id = os_popup_menu(I->win, m->x, m->y, IDE_CONTEXT_MENU);
            if (id == IC_BREAK) {
                ide_toggle_break(I, min_int(line, I->lines - 1));
                ide_draw_all(I);
            } else if (id) {
                ide_command(I, id);
            }
            return;
        }
        I->cursor = ed_index_at(I, line, column);
        if (m->type == MSG_MOUSE_DOWN) {
            I->selecting = 1;
            if (!(m->mods & MOD_SHIFT)) I->anchor = I->cursor;
            if (m->clicks == 2) {
                int a = I->cursor, b = I->cursor;
                while (a > 0 && ide_word_char(I->text[a - 1])) a--;
                while (b < I->length && ide_word_char(I->text[b])) b++;
                I->anchor = a;
                I->cursor = b;
            }
        }
        int was = I->focus;
        I->focus = FOCUS_EDITOR;
        I->goal = I->cursor - I->starts[ed_line_of(I, I->cursor)];
        I->blink = 1;
        ed_follow(I);
        ide_draw_editor(I);
        if (was != FOCUS_EDITOR) ide_draw_bottom(I);
        return;
    }
    if (m->button != BUTTON_LEFT) return;
    if (rect_contains(I->gutter_r, m->x, m->y)) {
        ide_toggle_break(I, I->top + (m->y - I->gutter_r.y) / LINE_H);
        ide_draw_editor(I);
        ide_draw_toolbar(I);
        return;
    }
    for (int i = 0; i < 2; i++)
        if (rect_contains(I->tabs[i], m->x, m->y)) {
            I->tab = i;
            ide_draw_bottom(I);
            return;
        }
    if (rect_contains(I->console_r, m->x, m->y)) {
        if (I->tab == TAB_PROBLEMS && I->built) {
            int line = ide_problem_line_at(I, m->y);
            if (line > 0) {
                /* to the line of the error */
                I->cursor = I->anchor = I->starts[clamp_int(line - 1, 0, I->lines - 1)];
                I->goal = 0;
                I->focus = FOCUS_EDITOR;
                ed_reveal(I, line - 1);
                ed_follow(I);
                snprintf(I->status, sizeof I->status, "Line %d", line);
                ide_draw_all(I);
            }
            return;
        }
        I->focus = FOCUS_CONSOLE;
        I->blink = 1;
        ide_draw_editor(I);
        ide_draw_bottom(I);
        return;
    }
    for (int i = 0; i < 3 && I->side_tab == SIDE_REGISTERS; i++)
        if (rect_contains(I->mem_buttons[i], m->x, m->y)) {
            if (i == MEM_ADDRESS) ide_goto_address(I);
            else I->mem_mode = i, I->mem_scroll = 0;
            ide_update_menus(I);
            ide_draw_side(I);
            ide_draw_toolbar(I);
            return;
        }
    for (int i = 0; i < 2; i++)
        if (rect_contains(I->side_tabs[i], m->x, m->y)) {
            I->side_tab = i;
            ide_draw_side(I);
            return;
        }
    if (I->side_tab == SIDE_BITMAP && rect_contains(I->side_r, m->x, m->y)) {
        /* the keys go to the program, as in the web editor's Screen */
        I->focus = FOCUS_CONSOLE;
        I->blink = 1;
        ide_draw_editor(I);
        ide_draw_bottom(I);
        return;
    }
    if (I->side_tab == SIDE_REGISTERS && rect_contains(I->regs_r, m->x, m->y)) {
        int row = (m->y - I->regs_r.y - HEAD_H - 3) / ROW_H, column = (m->x - I->regs_r.x - 4) / 106;
        if (row >= 0 && row < 16 && column >= 0 && column < 2) {
            I->selected = column * 16 + row;
            ide_draw_side(I);
        }
    }
}

/* ------------------------------------------------------------------ the windows' messages */

static void ide_bitmap_message(Ide *I, Message *m) {
    switch (m->type) {
    case MSG_CLOSE:
        ide_bitmap_close(I);
        ide_update_menus(I);
        break;
    case MSG_KEY:
        if (!ide_global_key(I, m)) ide_program_key(I, m);
        break;
    case MSG_MOUSE_DOWN: break;
    case MSG_QUIT: os_exit(0);
    case MSG_SETTINGS: case MSG_FOCUS: case MSG_BLUR: ide_bitmap_draw(I); break;
    default: os_default_message(m);
    }
}

static void ide_message(Ide *I, Message *m) {
    if (I->bitmap_win && m->win == I->bitmap_win) {
        ide_bitmap_message(I, m);
        return;
    }
    switch (m->type) {
    case MSG_KEY: ide_key(I, m); break;
    case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: case MSG_MOUSE_WHEEL: ide_mouse(I, m); break;
    case MSG_COMMAND: ide_command(I, m->id); break;
    case MSG_TIMER:
        I->blink = !I->blink;
        if (I->focus == FOCUS_EDITOR) ide_draw_line(I, ed_line_of(I, I->cursor));
        else if (I->machine.want) ide_draw_bottom(I);
        break;
    case MSG_RESIZE:
        ide_layout(I);
        ed_follow(I);
        ide_draw_all(I);
        break;
    case MSG_FOCUS: case MSG_BLUR: case MSG_SETTINGS: ide_draw_all(I); break;
    case MSG_DROP:
        if (os_app_for_file(m->text) == APP_IDE || os_app_for_file(m->text) == APP_NOTEPAD) ide_open_path(I, m->text);
        else os_message_box(I->win, "Assembly Editor", "Assembly Editor opens programs, .riscv Files.", MB_OK, MB_INFO);
        break;
    case MSG_CLOSE:
        if (ide_confirm(I)) os_exit(0);
        break;
    case MSG_QUIT: os_exit(0);
    default: os_default_message(m);
    }
}

void ide_main(const char *argument) {
    Ide *I = os_alloc(sizeof *I);
    if (!I) os_exit(1);
    I->text = os_alloc(IDE_CAPACITY + 1);
    I->starts = os_alloc((IDE_CAPACITY + 2) * sizeof *I->starts);
    I->console = os_alloc(IDE_CONSOLE);
    if (!I->text || !I->starts || !I->console) os_exit(1);
    DateTime now;
    os_time(&now);
    I->epoch_at = os_now_ms();
    I->epoch_ms = (ide_days(now.year, now.month, now.day) * 86400 + now.hour * 3600 + now.minute * 60 + now.second) * 1000;
    I->history = 1;
    I->tracker.pressed = -1;
    I->blink = 1;
    I->selected = 10;   /* a0 */
    memcpy(I->file_menu, IDE_FILE_MENU, sizeof IDE_FILE_MENU);
    memcpy(I->edit_menu, IDE_EDIT_MENU, sizeof IDE_EDIT_MENU);
    memcpy(I->run_menu, IDE_RUN_MENU, sizeof IDE_RUN_MENU);
    memcpy(I->view_menu, IDE_VIEW_MENU, sizeof IDE_VIEW_MENU);
    memcpy(I->help_menu, IDE_HELP_MENU, sizeof IDE_HELP_MENU);
    ide_examples(I);
    I->file_menu[2].submenu = I->example_items;
    I->menus[0] = (Menu){ "File", I->file_menu };
    I->menus[1] = (Menu){ "Edit", I->edit_menu };
    I->menus[2] = (Menu){ "Run", I->run_menu };
    I->menus[3] = (Menu){ "View", I->view_menu };
    I->menus[4] = (Menu){ "Help", I->help_menu };
    I->menus[5] = (Menu){ NULL, NULL };

    I->win = os_window_create("Assembly Editor", &ICON_IDE, 632, 404, WIN_RESIZABLE | WIN_CENTER);
    if (!I->win) os_exit(1);
    os_window_set_menu(I->win, I->menus);
    os_window_min_size(I->win, 540, 330);
    if (!(argument && argument[0] && ide_load(I, argument))) {
        ed_set_text(I, IDE_TEMPLATE, (long)strlen(IDE_TEMPLATE));
        if (argument && argument[0]) copy_text(I->path, sizeof I->path, argument);
        snprintf(I->status, sizeof I->status, "Build (F3), then Run (F5)");
    }
    I->modified = 0;
    ide_layout(I);
    ide_title(I);
    ide_draw_all(I);
    os_set_timer(I->win, IDE_BLINK, 530);
    for (;;) {
        Message m;
        if (I->running && !I->machine.want) {
            while (I->running && os_peek_message(&m)) ide_message(I, &m);
            if (!I->running || I->machine.want) continue;
            long wait = I->wake_at - os_now_ms();
            if (wait > 0) {
                ide_refresh(I);
                os_sleep((int)min_int((int)wait, 10));
                continue;
            }
            I->wake_at = 0;
            ide_slice(I);
            if (I->running) ide_refresh(I);
            continue;
        }
        os_get_message(&m);
        ide_message(I, &m);
    }
}
