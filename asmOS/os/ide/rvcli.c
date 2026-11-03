/*
 * rvcli: the Assembly Editor's assembler and interpreter as a command-line program, for the
 * computer the Project is edited on. It is not part of asmOS (nothing compiles it into the
 * Build); the tests build it with the host's gcc and run each program through it and through the
 * RARS Core, then compare what the two report.
 *
 *     gcc -O2 -o rvcli os/ide/rvcli.c
 *     rvcli [--input FILE] [--keys TEXT] [--limit N] [--screen] [--stepback-check [--journal N]]
 *           [--bench] program.s
 *
 * It prints a report: the errors' lines, or the machine code, the data, the console output, how the
 * program ended, the registers and pc. Program time is a virtual clock that starts at 2000-01-01
 * and moves only with sleeps, and a generator the program did not seed starts from 42, as the
 * tests' RARS driver answers both, so a run is the same every time.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "rv_asm.c"
#include "rv_cpu.c"

typedef struct {
    char *text;
    size_t length, capacity;
    int64_t clock;
    int time_reads;
} Console;

static void console_write(void *context, const char *text, int length) {
    Console *c = context;
    if (c->length + (size_t)length + 1 > c->capacity) {
        c->capacity = (c->length + (size_t)length + 1) * 2;
        c->text = realloc(c->text, c->capacity);
    }
    memcpy(c->text + c->length, text, (size_t)length);
    c->length += (size_t)length;
}

static void console_rewind(void *context, uint64_t length) {
    Console *c = context;
    if (length < c->length) c->length = (size_t)length;
}

static int64_t console_time(void *context) {
    ((Console *)context)->time_reads++;
    return ((Console *)context)->clock;
}
static int64_t console_seed(void *context) { (void)context; return 42; }

static char *read_file(const char *path, size_t *length) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = malloc((size_t)size + 1);
    if (data && fread(data, 1, (size_t)size, f) != (size_t)size) size = 0;
    if (data) data[size] = 0;
    fclose(f);
    *length = (size_t)size;
    return data;
}

/* As JSON.stringify writes a string, byte for byte. */
static void print_json(const char *text, size_t length) {
    putchar('"');
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        switch (c) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\b': fputs("\\b", stdout); break;
        case '\f': fputs("\\f", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (c < 0x20) printf("\\u%04x", c);
            else putchar(c);
        }
    }
    putchar('"');
}

typedef struct {
    const char *input;
    size_t input_length, cursor;
    const char *keys;
    uint64_t limit;
} Run;

/* Runs to the end, the limit or a fault, answering the read ecalls from the input and moving the
 * clock with the sleeps. */
static int run_program(RvMachine *m, Console *console, Run *run, uint64_t budget) {
    int stop = RV_STOP_NONE;
    while (budget) {
        uint32_t slice = budget > 1000000 ? 1000000 : (uint32_t)budget;
        uint64_t before = m->steps;
        stop = rv_run(m, slice, 0);
        uint64_t ran = m->steps - before;
        budget = ran >= budget ? 0 : budget - ran;
        if (stop == RV_STOP_INPUT) {
            if (m->want == RV_WANT_CHAR) {
                if (run->cursor < run->input_length) {
                    char ch[2] = { run->input[run->cursor++], 0 };
                    rv_supply_line(m, ch);
                } else {
                    rv_supply_line(m, "");
                }
            } else {
                size_t from = run->cursor, to = from;
                while (to < run->input_length && run->input[to] != '\n') to++;
                char *line = malloc(to - from + 1);
                memcpy(line, run->input + from, to - from);
                line[to - from] = 0;
                run->cursor = to < run->input_length ? to + 1 : to;
                rv_supply_line(m, line);
                free(line);
            }
            if (m->faulted) return RV_STOP_FAULT;
            budget = budget ? budget - 1 : 0;
            continue;
        }
        if (stop == RV_STOP_SLEEP) {
            console->clock += m->sleep_ms;
            continue;
        }
        if (stop == RV_STOP_BREAKPOINT || stop == RV_STOP_LIMIT) continue;
        return stop;
    }
    return RV_STOP_LIMIT;
}

typedef struct {
    uint32_t x[32], pc;
    int exited, faulted, exit_code, cliff;
    uint64_t steps, output;
} State;

static State state_of(const RvMachine *m) {
    State s;
    memcpy(s.x, m->x, sizeof s.x);
    s.pc = m->pc;
    s.exited = m->exited;
    s.faulted = m->faulted;
    s.exit_code = m->exit_code;
    s.cliff = m->cliff;
    s.steps = m->steps;
    s.output = m->output;
    return s;
}

/* Describes the first difference between two machines' states and memories, or answers NULL. */
typedef uint8_t **Pages[1024];

/* A copy of a machine's memory. */
static void snapshot(const RvMachine *m, Pages copy) {
    for (int t = 0; t < 1024; t++) {
        copy[t] = NULL;
        if (!m->pages[t]) continue;
        copy[t] = calloc(1024, sizeof(uint8_t *));
        for (int i = 0; i < 1024; i++)
            if (m->pages[t][i]) {
                copy[t][i] = malloc(RV_PAGE_SIZE);
                memcpy(copy[t][i], m->pages[t][i], RV_PAGE_SIZE);
            }
    }
}

static void free_snapshot(Pages copy) {
    for (int t = 0; t < 1024; t++) {
        if (!copy[t]) continue;
        for (int i = 0; i < 1024; i++) free(copy[t][i]);
        free(copy[t]);
    }
}

static const char *differ(uint8_t **const *a, const State *sa, uint8_t **const *b, const State *sb, char *why, size_t size) {
    for (int i = 0; i < 32; i++)
        if (sa->x[i] != sb->x[i]) return snprintf(why, size, "x%d %08x vs %08x", i, sa->x[i], sb->x[i]), why;
    if (sa->pc != sb->pc) return snprintf(why, size, "pc %08x vs %08x", sa->pc, sb->pc), why;
    if (sa->exited != sb->exited || sa->faulted != sb->faulted || sa->exit_code != sb->exit_code || sa->cliff != sb->cliff)
        return snprintf(why, size, "end state"), why;
    if (sa->steps != sb->steps) return snprintf(why, size, "steps %llu vs %llu", (unsigned long long)sa->steps, (unsigned long long)sb->steps), why;
    if (sa->output != sb->output) return snprintf(why, size, "output %llu vs %llu", (unsigned long long)sa->output, (unsigned long long)sb->output), why;
    for (int t = 0; t < 1024; t++) {
        if (!a[t] && !b[t]) continue;
        for (int i = 0; i < 1024; i++) {
            const uint8_t *pa = a[t] ? a[t][i] : NULL, *pb = b[t] ? b[t][i] : NULL;
            if (!pa && !pb) continue;
            for (int k = 0; k < (int)RV_PAGE_SIZE; k++) {
                uint8_t va = pa ? pa[k] : 0, vb = pb ? pb[k] : 0;
                if (va != vb) {
                    uint32_t at = (uint32_t)t << 22 | (uint32_t)i << 12 | (uint32_t)k;
                    return snprintf(why, size, "memory at %08x: %02x vs %02x", at, va, vb), why;
                }
            }
        }
    }
    return NULL;
}

static double seconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    const char *path = NULL, *input_path = NULL;
    Run run = { 0 };
    run.limit = 5000000;
    int screen = 0, stepback = 0, bench = 0, journal = 2000000;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--input") && i + 1 < argc) input_path = argv[++i];
        else if (!strcmp(argv[i], "--keys") && i + 1 < argc) run.keys = argv[++i];
        else if (!strcmp(argv[i], "--limit") && i + 1 < argc) run.limit = strtoull(argv[++i], NULL, 10);
        else if (!strcmp(argv[i], "--screen")) screen = 1;
        else if (!strcmp(argv[i], "--stepback-check")) stepback = 1;
        else if (!strcmp(argv[i], "--bench")) bench = 3;
        else if (!strcmp(argv[i], "--bench-off")) bench = 1;
        else if (!strcmp(argv[i], "--bench-on")) bench = 2;
        else if (!strcmp(argv[i], "--journal") && i + 1 < argc) journal = atoi(argv[++i]);
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "usage: rvcli [--input FILE] [--keys TEXT] [--limit N] [--screen] [--stepback-check] [--bench] program.s\n");
        return 2;
    }
    size_t length;
    char *source = read_file(path, &length);
    if (!source) {
        fprintf(stderr, "rvcli: cannot read %s\n", path);
        return 2;
    }
    if (input_path) {
        char *input = read_file(input_path, &run.input_length);
        run.input = input ? input : "";
    } else {
        run.input = "";
    }

    static RvProgram program;
    rv_assemble(&program, source, length);
    for (int i = 0; i < program.warning_count && i < RV_MAX_WARNINGS; i++)
        printf("# warning %d: %s\n", program.warnings[i].line, program.warnings[i].text);
    printf("ERRORS %d\n", program.error_count);
    for (int i = 0; i < program.error_count && i < RV_MAX_ERRORS; i++)
        printf("ERROR %d\n# %d: %s\n", program.errors[i].line, program.errors[i].line, program.errors[i].text);
    if (program.error_count) {
        rv_program_free(&program);
        free(source);
        if (input_path) free((char *)run.input);
        return 0;
    }
    if (screen) {
        RvScreen *s = &program.screen;
        printf("SCREEN %d %d %d %d %d %d %08x\n", s->present, s->line, s->width, s->height, s->unit_w, s->unit_h, s->base);
    }
    printf("TEXT");
    for (int i = 0; i < program.text_words; i++) printf(" %08x", program.text[i]);
    printf("\n");

    Console console = { 0 };
    console.clock = 946684800000LL;
    console.capacity = 64;
    console.text = malloc(console.capacity);
    RvHost host = { &console, console_write, console_rewind, console_time, NULL, console_seed };
    static RvMachine machine;
    RvMachine *m = &machine;
    if (rv_machine_init(m, &program, &host, stepback ? journal : 0, 0) != 0) {
        fprintf(stderr, "rvcli: out of memory\n");
        return 2;
    }
    /* the data window the RARS driver reads too: every 16 bytes that are not all zeroes */
    for (uint32_t a = RV_DATA_SEGMENT; a < RV_DATA_SEGMENT + 0x50000; a += 16) {
        uint8_t bytes[16];
        rv_read(m, a, bytes, 16);
        int any = 0;
        for (int k = 0; k < 16; k++) any |= bytes[k];
        if (!any) continue;
        printf("DATA %08x ", a);
        for (int k = 0; k < 16; k++) printf("%02x", bytes[k]);
        printf("\n");
    }

    if (bench) {
        /* instructions a second, the journal off then on */
        for (int on = 0; on < 2; on++) {
            if (!(bench & (1 << on))) continue;
            RvMachine b;
            rv_machine_init(&b, &program, &host, on ? 65536 : 0, 0);
            double start = seconds();
            run_program(&b, &console, &run, run.limit);
            double took = seconds() - start;
            printf("# bench journal %s: %llu instructions in %.3f s, %.1f M/s\n", on ? "on" : "off",
                   (unsigned long long)b.steps, took, (double)b.steps / took / 1e6);
            rv_machine_free(&b);
            console.length = 0;
            run.cursor = 0;
        }
        return 0;
    }

    for (const char *k = run.keys; k && *k; k++) {
        int ch = (unsigned char)*k;
        if (ch == '\\' && k[1] == 'n') ch = '\n', k++;
        rv_key_push(m, ch);
    }
    int stop = run_program(m, &console, &run, run.limit);
    printf("OUTPUT ");
    print_json(console.text, console.length);
    printf("\n");
    if (stop == RV_STOP_FAULT) printf("END fault %08x\n# %s (line %d)\n", m->pc, m->fault, rv_line_of(&program, m->pc));
    else if (m->exited && m->cliff) printf("END cliff\n");
    else if (m->exited) printf("END exit %d\n", m->exit_code);
    else printf("END limit\n");
    printf("REGS");
    for (int i = 1; i < 32; i++) printf(" %08x", m->x[i]);
    printf("\nPC %08x\n", m->pc);

    if (stepback) {
        char why[160];
        State end = state_of(m);
        static Pages end_pages;
        snapshot(m, end_pages);
        size_t end_output = console.length;
        char *end_text = malloc(end_output + 1);
        memcpy(end_text, console.text, end_output);
        const char *problem = NULL;
        /* a few steps back and forward again: the same end, for a program that reads no input and
         * no clock (Step back does not turn the clock back) */
        if (!run.input_length && !console.time_reads && m->steps > 0) {
            uint64_t back = m->steps < 37 ? m->steps : 37;
            for (uint64_t i = 0; i < back; i++) rv_step_back(m);
            run_program(m, &console, &run, stop == RV_STOP_LIMIT ? back : run.limit);
            State again = state_of(m);
            if (stop == RV_STOP_LIMIT) again.steps = end.steps;   /* the budget was spent before */
            problem = differ((uint8_t **const *)m->pages, &again, (uint8_t **const *)end_pages, &end, why, sizeof why);
            if (!problem && (console.length != end_output || memcmp(console.text, end_text, end_output)))
                problem = "the output differs after stepping back and running again";
        }
        /* all the way back: the machine as rv_reset left it, when the journal kept every step */
        if (!problem && stop != RV_STOP_LIMIT) while (rv_step_back(m)) {}
        if (!problem && stop != RV_STOP_LIMIT && m->steps == 0) {
            static RvMachine fresh;
            rv_machine_init(&fresh, &program, NULL, 0, 0);
            for (const char *k = run.keys; k && *k; k++) {
                int ch = (unsigned char)*k;
                if (ch == '\\' && k[1] == 'n') ch = '\n', k++;
                rv_key_push(&fresh, ch);
            }
            State back = state_of(m), start = state_of(&fresh);
            problem = differ((uint8_t **const *)m->pages, &back, (uint8_t **const *)fresh.pages, &start, why, sizeof why);
            if (!problem && console.length) problem = "output remains after stepping back to the start";
            if (!problem && (m->key_count != fresh.key_count)) problem = "the keyboard queue differs";
            rv_machine_free(&fresh);
            /* and forward again to the same end */
            if (!problem) {
                run.cursor = 0;
                console.clock = 946684800000LL;
                run_program(m, &console, &run, run.limit);
                State again = state_of(m);
                problem = differ((uint8_t **const *)m->pages, &again, (uint8_t **const *)end_pages, &end, why, sizeof why);
                if (!problem && (console.length != end_output || memcmp(console.text, end_text, end_output)))
                    problem = "the output differs after running again from the start";
            }
        }
        if (problem) printf("STEPBACK %s\n", problem);
        else printf("STEPBACK ok\n");
        free(end_text);
        free_snapshot(end_pages);
    }
    rv_machine_free(m);
    rv_program_free(&program);
    free(console.text);
    free(source);
    if (input_path) free((char *)run.input);
    return 0;
}
