/*
 * A RISC-V assembler and interpreter for RV32IM that behave as RARS 1.6 does, so that one `.s`
 * file runs the same in the web editor's RISC-V Target and in asmOS's Assembly Editor.
 *
 * Plain C with no part of asmOS: os/ide/ide.c includes rv_asm.c and rv_cpu.c into its own
 * compilation, and os/ide/rvcli.c builds the same two into a program for the computer the Project
 * is edited on, which the tests run beside RARS to compare the two.
 *
 * Memory comes from RV_ALLOC(size), which answers zeroed memory, RV_REALLOC(pointer, size) and
 * RV_FREE(pointer). A host defines them before it includes the .c files (asmOS: os_alloc,
 * os_realloc, os_free); otherwise they are calloc, realloc and free. Nothing here keeps state
 * outside the RvProgram and RvMachine it is handed, so two editors can run at once.
 */
#ifndef RV_H
#define RV_H

#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ RARS's memory map */

#define RV_TEXT_BASE 0x00400000u      /* .text, and where execution starts without a global main */
#define RV_TEXT_LIMIT 0x10000000u     /* the text segment ends before this */
#define RV_DATA_SEGMENT 0x10000000u   /* the data segment: static data and the heap */
#define RV_GP_VALUE 0x10008000u       /* gp's first value */
#define RV_DATA_BASE 0x10010000u      /* .data */
#define RV_HEAP_BASE 0x10040000u      /* the first address sbrk (ecall 9) hands out */
#define RV_DATA_LIMIT 0x10400000u     /* the data segment ends before this */
#define RV_STACK_LIMIT 0x7fbffffcu    /* the stack holds the addresses above this... */
#define RV_STACK_BASE 0x7ffffffcu     /* ...up to the word at this one */
#define RV_SP_VALUE 0x7fffeffcu       /* sp's first value */
#define RV_MMIO_BASE 0xffff0000u      /* memory-mapped devices, to the top of memory */

/* The keyboard and display of RARS's Keyboard and Display MMIO Simulator. Bit 0 of a control
 * register is Ready; setting bit 1, interrupt enable, stops the program with an error, as in the
 * web editor, which polls rather than interrupts. */
#define RV_KEY_CONTROL 0xffff0000u     /* reads 1 while a typed character waits */
#define RV_KEY_DATA 0xffff0004u        /* reading takes the character */
#define RV_DISPLAY_CONTROL 0xffff0008u /* always reads 1: ready */
#define RV_DISPLAY_DATA 0xffff000cu    /* a byte stored here goes to the console */

#define RV_PAGE_BITS 12
#define RV_PAGE_SIZE (1u << RV_PAGE_BITS)

/* ------------------------------------------------------------------ assembling */

enum { RV_MAX_ERRORS = 64, RV_ERROR_TEXT = 112, RV_NAME_SIZE = 40, RV_MAX_WARNINGS = 16 };

typedef struct {
    int line;                    /* 1 based */
    char text[RV_ERROR_TEXT];    /* a sentence: "Unknown instruction 'ad'" */
} RvError;

typedef struct {
    char name[RV_NAME_SIZE];
    uint32_t address;
    int line;                    /* where it is defined, 1 based */
    int global;                  /* named by .globl */
    int text;                    /* a label in .text rather than .data */
} RvSymbol;

/*
 * The bitmap display a program asks for with a comment such as
 *     # @screen width=256 height=256 unit=4 base=display
 * read the way the web editor reads it (packages/asm-editor/src/lib/languages/mars/screenDirective.ts):
 * the first such comment counts, names are case-blind and ignore - and _ (unitWidth, unit-width),
 * unit sets both unit sizes, sizes are clamped to 1-1024 pixels and 1-32 per unit, base is an
 * address (decimal or 0x) or a label, rounded down to a word. Anything left out keeps the web
 * editor's default: 512 by 256 pixels, unit 1, base 0x10010000. The grid is width / unit_w words
 * across (at least 1) and height / unit_h down; each word is a pixel 0x00RRGGBB.
 */
typedef struct {
    int present;                 /* the program has a @screen comment */
    int line;                    /* its line, 1 based, or 0 */
    int width, height;           /* in pixels */
    int unit_w, unit_h;          /* pixels per word */
    uint32_t base;
} RvScreen;

typedef struct {
    uint32_t *text;              /* the machine code, one word an instruction, from RV_TEXT_BASE */
    int *text_line;              /* the source line of each word, 1 based */
    int text_words;
    uint8_t *data;               /* the static data's first values... */
    uint32_t data_base;          /* ...from this address (RV_DATA_BASE unless `.data ADDRESS` went lower) */
    uint32_t data_size;          /* bytes, .space included */
    RvSymbol *symbols;
    int symbol_count;
    uint32_t entry;              /* the global main, else RV_TEXT_BASE */
    RvScreen screen;
    int line_count;              /* lines in the source */
    RvError errors[RV_MAX_ERRORS];
    int error_count;             /* errors found, which may be more than RV_MAX_ERRORS kept */
    /* What RARS accepts with a warning and assembles all the same: an unknown directive (ignored),
     * an instruction in .data (ignored), a .byte or .half value cut to fit. They do not stop a run. */
    RvError warnings[RV_MAX_WARNINGS];
    int warning_count;
} RvProgram;

/*
 * Assembles RARS assembly language in two passes: labels, # comments, x0-x31 and the ABI register
 * names (fp too), .text .data .word .half .byte .ascii .asciz .string .space .align .globl
 * (.global) .eqv, %hi() and %lo(), character literals and decimal, hexadecimal and negative
 * numbers, every RV32IM instruction and RARS's common pseudo-instructions, expanded into the same
 * instructions RARS expands them into. An error names its line and assembling goes on with the
 * next line, so one Build lists several. `program` must be zeroed or freed; it holds the result
 * either way, and the program can only run when error_count is 0. Answers error_count.
 */
int rv_assemble(RvProgram *program, const char *source, size_t length);
void rv_program_free(RvProgram *program);

/* The source line of the instruction at `address`, or 0 outside the program's text. */
int rv_line_of(const RvProgram *program, uint32_t address);
/* The address of the first instruction a source line assembled into, or 0 when it has none. */
uint32_t rv_address_of_line(const RvProgram *program, int line);
/* The symbol at exactly `address`, preferring a text label to a data one, or NULL. */
const RvSymbol *rv_symbol_at(const RvProgram *program, uint32_t address);
/* "zero", "ra", "sp", ... "t6" */
const char *rv_register_name(int index);

/* ------------------------------------------------------------------ running */

/* What the interpreter asks of the program around it. Any function may be NULL. */
typedef struct {
    void *context;
    /* Console output: print ecalls, the echo of what a read ecall was given, and the display
     * data register. `text` is not NUL-terminated. */
    void (*write)(void *context, const char *text, int length);
    /* Step back took back output: keep only the first `length` bytes written since rv_reset. */
    void (*rewind)(void *context, uint64_t length);
    /* Milliseconds since 1970-01-01 UTC, for ecall 30. NULL answers 0. */
    int64_t (*time_ms)(void *context);
    /* A note for ecalls 31 and 33, already put in RARS's ranges. */
    void (*midi)(void *context, int pitch, int duration_ms, int instrument, int volume);
    /* A seed, 0 to 2^48 - 1, for a random generator the program did not seed with ecall 40.
     * NULL seeds from time_ms. */
    int64_t (*random_seed)(void *context);
} RvHost;

/* Why rv_run stopped. */
enum {
    RV_STOP_NONE,
    RV_STOP_LIMIT,         /* it ran `limit` instructions */
    RV_STOP_BREAKPOINT,    /* pc is on a breakpoint, its instruction not run yet; or ebreak ran */
    RV_STOP_EXIT,          /* exit (10), exit with a code (93), or the program ran off the end of its text */
    RV_STOP_FAULT,         /* a runtime error: see RvMachine.fault */
    RV_STOP_INPUT,         /* the ecall at pc waits for input: see RvMachine.want */
    RV_STOP_SLEEP,         /* ecall 32 or 33 ran: wait RvMachine.sleep_ms, then go on */
};

/* What the read ecall at pc waits for. */
enum { RV_WANT_NONE, RV_WANT_INT, RV_WANT_STRING, RV_WANT_CHAR };

/* One instruction, decoded once when the machine is reset. */
typedef struct {
    uint8_t op, rd, rs1, rs2;
    int32_t imm;
} RvCode;

/* The journal of what each instruction changed, so that Step back can take it back. */
typedef struct {
    uint32_t kind;         /* rv_cpu.c's own */
    uint32_t a, b, c;
} RvJournalEntry;

typedef struct RvMachine {
    /* --- what the editor shows; read freely, change only through the calls below */
    uint32_t x[32];
    uint32_t pc;
    uint64_t steps;            /* instructions run since rv_reset (less those stepped back) */
    int stop;                  /* the last RV_STOP_ */
    int exited;                /* the program ended: exit, exit with a code, or off the end */
    int exit_code;             /* exit 93's a0, else 0 */
    int cliff;                 /* it ended by running off the end of its text */
    int faulted;               /* a runtime error stopped it; pc is on the instruction that failed,
                                * or, for a jump to where no instruction is, where it jumped */
    char fault[160];           /* the error: "Load address not aligned to word boundary: 0x10010001" */
    int want;                  /* RV_WANT_: the read ecall at pc waits for rv_supply_ */
    int want_size;             /* RV_WANT_STRING: the buffer's size, a1 */
    int sleep_ms;              /* RV_STOP_SLEEP: the milliseconds to wait */
    uint64_t output;           /* bytes written through host.write since rv_reset */
    /* The memory a bitmap display shows: a store into [watch_lo, watch_hi) sets watch_dirty, and
     * the editor clears it once it drew the picture again. */
    uint32_t watch_lo, watch_hi;
    int watch_dirty;
    uint32_t watch_from, watch_to;   /* while watch_dirty: the lowest and highest address stored */

    /* --- the interpreter's own */
    const RvProgram *program;
    RvHost host;
    RvCode *code;              /* the text, decoded, one per word */
    uint8_t *breaks;           /* one per word: a breakpoint is set */
    uint8_t **pages[1024];     /* memory: pages[address >> 22][(address >> 12) & 1023], or NULL */
    int page_count, page_limit;
    uint32_t heap;             /* the next address sbrk hands out */
    int keys[64];              /* characters typed for the keyboard receiver, a ring */
    int key_head, key_count;
    struct {
        int id, seeded;
        uint64_t seed;
    } random[16];
    int random_count;
    RvJournalEntry *journal;   /* a ring of journal_size entries; each step starts with a marker */
    int journal_size, journal_head, journal_count;
    int journal_on;
    uint8_t *real_op;          /* one per word: the op a breakpoint's BREAK stands in for */
    uint32_t read_tag, write_tag;   /* the page (address >> 12) the last load and store used... */
    const uint8_t *read_page;  /* ...and where it is; a never-written page reads from a page of zeroes */
    uint8_t *write_page;
    int receiver_armed;        /* RV_KEY_DATA shows a typed character */
    int side;                  /* what an access through the slow path left to do: rv_cpu.c's own */
} RvMachine;

/*
 * Prepares a machine for an assembled program (error_count 0), which must outlive it, and resets
 * it. `journal_size` is how many journal entries Step back keeps, rounded up to a power of two (a
 * step takes one, an ecall a few more, a read string one a byte stored, so it must be well above
 * the longest input line); 0 keeps none. A full journal drops its oldest steps. `page_limit` caps the 4 KiB pages of memory the
 * program may touch (0: 4096, 16 MiB). Answers 0, or -1 when memory runs out.
 */
int rv_machine_init(RvMachine *m, const RvProgram *program, const RvHost *host, int journal_size,
                    int page_limit);
void rv_machine_free(RvMachine *m);

/* Back to the start: registers as RARS sets them (sp, gp, pc at the entry, the rest 0), memory
 * holding the static data and nothing else, the heap, the keyboard, the generators and the
 * journal emptied. Breakpoints stay. */
void rv_reset(RvMachine *m);

/*
 * Runs at most `limit` instructions and answers why it stopped (also in m->stop). With
 * `breakpoints` set it stops before an instruction that has a breakpoint, unless that is the first
 * instruction of this call, so that Run from a breakpoint goes on past it. An ended or faulted
 * program runs nothing and answers its stop again; one that waits for input answers RV_STOP_INPUT
 * until it is given it. A run that runs nothing does not touch the journal.
 */
int rv_run(RvMachine *m, uint32_t limit, int breakpoints);

/* Takes back the last instruction that ran, the ecall a program waits in included, and a fault
 * or an end with it: registers, memory, pc, the heap, the keyboard, the generators, and output
 * through host.rewind. Answers 1, or 0 when the journal holds nothing more. */
int rv_step_back(RvMachine *m);
int rv_can_step_back(const RvMachine *m);
/* Turns the journal on (when journal_size allows) or off; either way it is emptied, so Step back
 * never takes back a step it did not see. */
void rv_journal(RvMachine *m, int on);

/* Breakpoints are kept by address, an instruction's; one on any other address is ignored. */
void rv_set_breakpoint(RvMachine *m, uint32_t address, int on);
void rv_clear_breakpoints(RvMachine *m);

/* Gives the waiting read ecall its input and completes it, so that pc moves on, as the web editor
 * does: read integer (5) takes a line, trimmed, as Java's Integer.parseInt reads it, and stops the
 * program with "invalid integer input (syscall 5)" otherwise; read string (8) stores it with
 * fgets's limits; read character (12) takes one character, Enter as 10 (rv_supply_line gives it
 * the line's first character, and an empty line stops the program with "invalid char input").
 * The line or character is echoed to the console, with a newline after a line, and the whole is
 * one step for Step back; a refused input's step takes back only its echo. Answers 0, or -1 when
 * nothing waits. */
int rv_supply_line(RvMachine *m, const char *line);
int rv_supply_char(RvMachine *m, int ch);

/* A character typed for the keyboard receiver at RV_KEY_DATA; the oldest is dropped when the
 * queue is full. */
void rv_key_push(RvMachine *m, int ch);

/* Copies memory out for the views, never faulting: what was never written reads 0, and the text
 * segment reads as the program's machine code (which the program itself may not read). Answers 1
 * when all of it is memory the program may use, 0 when part is not. */
int rv_read(const RvMachine *m, uint32_t address, void *out, uint32_t length);
/* The page holding `address`, RV_PAGE_SIZE bytes from address & ~(RV_PAGE_SIZE - 1), or NULL when
 * the program never wrote there (it reads 0): what a bitmap display copies from. */
const uint8_t *rv_page(const RvMachine *m, uint32_t address);

#endif
