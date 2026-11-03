/*
 * The interpreter of rv.h: RV32IM with RARS's ecalls, memory map and Keyboard and Display MMIO.
 *
 * The text is decoded once, at rv_machine_init, into one RvCode a word, which a switch runs; a
 * write to an ALU instruction's x0 is decoded as a nop, so x0 stays 0 without being reset. Memory
 * is a two-level table of 4 KiB pages, made on the first store; a page never written reads as
 * zeroes. The last page a load used and the last a store used are remembered, so most accesses
 * cost a compare. A breakpoint replaces its instruction's op by BREAK and keeps the real one
 * aside, so a run without breakpoints pays nothing for them.
 *
 * Every instruction can journal what it changes, in a ring the size the host chose: a marker with
 * its pc and the register or memory it wrote, and after it whatever else it changed (the heap, a
 * generator, the output, a key it took). Step back pops back to the marker and puts each thing
 * back. The run loop exists twice, with and without the journal, so a run without it pays nothing.
 */
#include "rv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RV_ALLOC
#define RV_ALLOC(size) calloc(1, (size))
#define RV_REALLOC(pointer, size) realloc((pointer), (size))
#define RV_FREE(pointer) free(pointer)
#endif

/* A store at `a` inside the watched memory: what the display must show again. */
static inline void rv_watched(RvMachine *m, uint32_t a) {
    if (!m->watch_dirty) {
        m->watch_dirty = 1;
        m->watch_from = m->watch_to = a;
    } else if (a < m->watch_from) {
        m->watch_from = a;
    } else if (a > m->watch_to) {
        m->watch_to = a;
    }
}

enum {
    O_ILLEGAL, O_NOP, O_LUI, O_AUIPC, O_JAL, O_J, O_JALR, O_JR, O_BEQ, O_BNE, O_BLT, O_BGE, O_BLTU, O_BGEU,
    O_LB, O_LH, O_LW, O_LBU, O_LHU, O_SB, O_SH, O_SW, O_LI, O_ADDI, O_SLTI, O_SLTIU, O_XORI, O_ORI, O_ANDI,
    O_SLLI, O_SRLI, O_SRAI, O_ADD, O_SUB, O_SLL, O_SLT, O_SLTU, O_XOR, O_SRL, O_SRA, O_OR, O_AND, O_MUL,
    O_MULH, O_MULHSU, O_MULHU, O_DIV, O_DIVU, O_REM, O_REMU, O_ECALL, O_EBREAK, O_BREAK
};

/* Journal entries. A marker (J_STEP) starts each step: a = its pc; with J_REG, b = the register's
 * old value; with J_MEM, b = the address and c the old bytes. The entries after it, J_REG
 * (a = old value), J_MEM (a = address, b = old bytes), J_HEAP, J_RANDOM, J_OUTPUT, J_KEY and
 * J_EXIT, are its other changes. Bits 8-15 of kind hold a register or a size. */
enum { J_NONE = 0, J_REG, J_MEM, J_HEAP, J_RANDOM, J_OUTPUT, J_KEY, J_EXIT };
#define J_STEP 0x80000000u
#define J_UNCOUNTED 0x40000000u

/* What an access through the slow path left for the instruction to do after it. */
enum { SIDE_NONE, SIDE_KEY_TAKEN, SIDE_TRANSMIT, SIDE_CONTROL };

static const uint8_t rv_zero_page[RV_PAGE_SIZE];

/* ------------------------------------------------------------------ decoding */

static RvCode rv_decode(uint32_t w) {
    RvCode c = { O_ILLEGAL, (uint8_t)((w >> 7) & 31), (uint8_t)((w >> 15) & 31), (uint8_t)((w >> 20) & 31), 0 };
    uint32_t f3 = (w >> 12) & 7, f7 = w >> 25;
    int32_t imm_i = (int32_t)w >> 20;
    int32_t imm_s = (int32_t)((w & 0xfe000000u) | ((w >> 7) & 31) << 20) >> 20;
    uint32_t b = (w >> 31) << 12 | ((w >> 7) & 1) << 11 | ((w >> 25) & 63) << 5 | ((w >> 8) & 15) << 1;
    int32_t imm_b = (int32_t)(b << 19) >> 19;
    uint32_t j = (w >> 31) << 20 | ((w >> 12) & 255) << 12 | ((w >> 20) & 1) << 11 | ((w >> 21) & 1023) << 1;
    int32_t imm_j = (int32_t)(j << 11) >> 11;
    switch (w & 127) {
    case 0x37: c.op = O_LUI; c.imm = (int32_t)(w & 0xfffff000u); break;
    case 0x17: c.op = O_AUIPC; c.imm = (int32_t)(w & 0xfffff000u); break;
    case 0x6f: c.op = c.rd ? O_JAL : O_J; c.imm = imm_j; break;
    case 0x67: if (f3 == 0) c.op = c.rd ? O_JALR : O_JR, c.imm = imm_i; break;
    case 0x63: {
        static const uint8_t ops[8] = { O_BEQ, O_BNE, O_ILLEGAL, O_ILLEGAL, O_BLT, O_BGE, O_BLTU, O_BGEU };
        c.op = ops[f3];
        c.imm = imm_b;
        break;
    }
    case 0x03: {
        static const uint8_t ops[8] = { O_LB, O_LH, O_LW, O_ILLEGAL, O_LBU, O_LHU, O_ILLEGAL, O_ILLEGAL };
        c.op = ops[f3];
        c.imm = imm_i;
        break;
    }
    case 0x23: {
        static const uint8_t ops[8] = { O_SB, O_SH, O_SW, O_ILLEGAL, O_ILLEGAL, O_ILLEGAL, O_ILLEGAL, O_ILLEGAL };
        c.op = ops[f3];
        c.imm = imm_s;
        break;
    }
    case 0x13:
        c.imm = imm_i;
        switch (f3) {
        case 0: c.op = c.rs1 == 0 ? O_LI : O_ADDI; break;
        case 2: c.op = O_SLTI; break;
        case 3: c.op = O_SLTIU; break;
        case 4: c.op = O_XORI; break;
        case 6: c.op = O_ORI; break;
        case 7: c.op = O_ANDI; break;
        case 1: if (f7 == 0) c.op = O_SLLI, c.imm = c.rs2; break;
        case 5: if (f7 == 0) c.op = O_SRLI, c.imm = c.rs2;
                else if (f7 == 0x20) c.op = O_SRAI, c.imm = c.rs2;
                break;
        }
        if (c.op != O_ILLEGAL && c.rd == 0) c.op = O_NOP;
        break;
    case 0x33:
        if (f7 == 0) {
            static const uint8_t ops[8] = { O_ADD, O_SLL, O_SLT, O_SLTU, O_XOR, O_SRL, O_OR, O_AND };
            c.op = ops[f3];
        } else if (f7 == 0x20) {
            c.op = f3 == 0 ? O_SUB : f3 == 5 ? O_SRA : O_ILLEGAL;
        } else if (f7 == 1) {
            static const uint8_t ops[8] = { O_MUL, O_MULH, O_MULHSU, O_MULHU, O_DIV, O_DIVU, O_REM, O_REMU };
            c.op = ops[f3];
        }
        if (c.op != O_ILLEGAL && c.rd == 0) c.op = O_NOP;
        break;
    case 0x73:
        if (w == 0x00000073) c.op = O_ECALL;
        else if (w == 0x00100073) c.op = O_EBREAK;
        break;
    case 0x0f:
        if (f3 == 0 || f3 == 1) c.op = O_NOP;
        break;
    }
    if (c.op == O_LUI || c.op == O_AUIPC) {
        if (c.rd == 0) c.op = O_NOP;
    }
    return c;
}

/* ------------------------------------------------------------------ memory */

/* Which part of memory an access starting at `a` is in, as Memory.java decides it. */
enum { REGION_NONE, REGION_DATA, REGION_STACK, REGION_MMIO, REGION_TEXT };
static int rv_region(uint32_t a) {
    if (a >= RV_DATA_SEGMENT && a < RV_DATA_LIMIT) return REGION_DATA;
    if (a > RV_STACK_LIMIT && a <= RV_STACK_BASE) return REGION_STACK;
    if (a >= RV_MMIO_BASE) return REGION_MMIO;
    if (a >= RV_TEXT_BASE && a < RV_TEXT_LIMIT) return REGION_TEXT;
    return REGION_NONE;
}

/* Whether all of a's page is in a's part of memory, so that the page can be remembered: the stack's
 * two end pages hold addresses past it. */
static int rv_whole_page(uint32_t a) {
    uint32_t page = a >> 12;
    return page != RV_STACK_BASE >> 12 && page != RV_STACK_LIMIT >> 12;
}

static uint8_t *rv_page_of(const RvMachine *m, uint32_t a) {
    uint8_t **table = m->pages[a >> 22];
    return table ? table[(a >> 12) & 1023] : NULL;
}

const uint8_t *rv_page(const RvMachine *m, uint32_t address) { return rv_page_of(m, address); }

/* The page for a store, made if need be; NULL when the program has used all it may. */
static uint8_t *rv_page_make(RvMachine *m, uint32_t a) {
    uint8_t **table = m->pages[a >> 22];
    if (!table) {
        table = RV_ALLOC(1024 * sizeof(uint8_t *));
        if (!table) return NULL;
        m->pages[a >> 22] = table;
    }
    uint8_t **slot = &table[(a >> 12) & 1023];
    if (!*slot) {
        if (m->page_count >= m->page_limit) return NULL;
        *slot = RV_ALLOC(RV_PAGE_SIZE);
        if (!*slot) return NULL;
        m->page_count++;
        if (m->read_tag == a >> 12) m->read_page = *slot;
    }
    return *slot;
}

static void rv_fault(RvMachine *m, const char *format, uint32_t a) {
    m->faulted = 1;
    snprintf(m->fault, sizeof m->fault, format, a);
}

static uint32_t rv_get(const uint8_t *p, int size) {
    if (size == 4) return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    if (size == 2) return (uint32_t)p[0] | (uint32_t)p[1] << 8;
    return p[0];
}

static void rv_put(uint8_t *p, int size, uint32_t v) {
    p[0] = (uint8_t)v;
    if (size >= 2) p[1] = (uint8_t)(v >> 8);
    if (size == 4) p[2] = (uint8_t)(v >> 16), p[3] = (uint8_t)(v >> 24);
}

static void rv_mmio_word(RvMachine *m, uint32_t a, uint32_t v) {
    uint8_t *p = rv_page_make(m, a);
    if (p) rv_put(p + (a & (RV_PAGE_SIZE - 1)), 4, v);
}

/* Shows the next typed character in the receiver's data register and sets Ready, or clears Ready
 * when there is none, as MarsDevices.refreshReceiver does. */
static void rv_refresh_receiver(RvMachine *m) {
    if (!m->key_count) {
        m->receiver_armed = 0;
        rv_mmio_word(m, RV_KEY_CONTROL, 0);
        return;
    }
    int key = m->keys[m->key_head];
    rv_mmio_word(m, RV_KEY_DATA, key > 0xff ? '?' : (uint32_t)key);
    rv_mmio_word(m, RV_KEY_CONTROL, 1);
    m->receiver_armed = 1;
}

/* A load the remembered page does not hold. Answers the value, or sets the fault. */
static uint32_t rv_load_slow(RvMachine *m, uint32_t a, int size) {
    m->side = SIDE_NONE;
    int region = rv_region(a);
    if (region == REGION_TEXT) {
        rv_fault(m, "Cannot read directly from the text segment: 0x%08x", a);
        return 0;
    }
    if (region == REGION_NONE) {
        rv_fault(m, "Load address out of range: 0x%08x", a);
        return 0;
    }
    const uint8_t *p = rv_page_of(m, a);
    if (region == REGION_MMIO) {
        uint32_t v = p ? rv_get(p + (a & (RV_PAGE_SIZE - 1)), size) : 0;
        if (a <= RV_KEY_DATA + 3 && a + (uint32_t)size > RV_KEY_DATA) m->side = SIDE_KEY_TAKEN;
        return v;
    }
    if (!p) p = rv_zero_page;
    if (rv_whole_page(a)) {
        m->read_tag = a >> 12;
        m->read_page = p;
    }
    return rv_get(p + (a & (RV_PAGE_SIZE - 1)), size);
}

/* Where a store the remembered page does not hold goes, or NULL with the fault set. */
static uint8_t *rv_store_slow(RvMachine *m, uint32_t a, int size, uint32_t v) {
    m->side = SIDE_NONE;
    int region = rv_region(a);
    if (region == REGION_TEXT) {
        rv_fault(m, "Cannot write directly to the text segment: 0x%08x", a);
        return NULL;
    }
    if (region == REGION_NONE) {
        rv_fault(m, "Store address out of range: 0x%08x", a);
        return NULL;
    }
    uint8_t *p = rv_page_make(m, a);
    if (!p) {
        m->faulted = 1;
        snprintf(m->fault, sizeof m->fault, "Out of memory: the program used all of its %d KiB", m->page_limit * 4);
        return NULL;
    }
    if (region == REGION_MMIO) {
        uint32_t end = a + (uint32_t)size;
        int receiver = a <= RV_KEY_CONTROL + 3 && end > RV_KEY_CONTROL;
        int transmitter = a <= RV_DISPLAY_CONTROL + 3 && end > RV_DISPLAY_CONTROL;
        if ((receiver || transmitter) && (v & 2)) {
            m->faulted = 1;
            snprintf(m->fault, sizeof m->fault,
                     "Interrupt-driven I/O is not supported: bit 1 of the %s control register at 0x%08x was set. "
                     "Poll the Ready bit instead",
                     receiver ? "receiver" : "transmitter", receiver ? RV_KEY_CONTROL : RV_DISPLAY_CONTROL);
            return NULL;
        }
        if (receiver || transmitter) m->side = SIDE_CONTROL;
        else if (a <= RV_DISPLAY_DATA + 3 && end > RV_DISPLAY_DATA) m->side = SIDE_TRANSMIT;
        return p + (a & (RV_PAGE_SIZE - 1));
    }
    if (rv_whole_page(a)) {
        m->write_tag = a >> 12;
        m->write_page = p;
    }
    return p + (a & (RV_PAGE_SIZE - 1));
}

int rv_read(const RvMachine *m, uint32_t address, void *out, uint32_t length) {
    uint8_t *to = out;
    int all = 1;
    const RvProgram *program = m->program;
    while (length) {
        uint32_t a = address, chunk = RV_PAGE_SIZE - (a & (RV_PAGE_SIZE - 1));
        if (chunk > length) chunk = length;
        int region = rv_region(a);
        if (region == REGION_TEXT) {
            for (uint32_t i = 0; i < chunk; i++) {
                uint32_t at = a + i, index = (at - RV_TEXT_BASE) >> 2;
                to[i] = index < (uint32_t)program->text_words ? (uint8_t)(program->text[index] >> (8 * (at & 3))) : 0;
            }
        } else if (region == REGION_NONE) {
            memset(to, 0, chunk);
            all = 0;
        } else {
            const uint8_t *p = rv_page_of(m, a);
            if (p) memcpy(to, p + (a & (RV_PAGE_SIZE - 1)), chunk);
            else memset(to, 0, chunk);
            /* the stack ends at its base word */
            if (region == REGION_STACK && a + chunk - 1 > RV_STACK_BASE + 3) all = 0;
        }
        to += chunk;
        address += chunk;
        length -= chunk;
    }
    return all;
}

/* ------------------------------------------------------------------ the journal */

/* Makes room in a full ring: the oldest steps go, markers and all, a sixteenth of the ring at a
 * time, so that a long run with a full journal pays for this once in thousands of steps. */
static void rv_journal_drop(RvMachine *m) {
    int tail = m->journal_head - m->journal_count;
    if (tail < 0) tail += m->journal_size;
    int drop = m->journal_size / 16;
    do {
        m->journal_count--;
        if (++tail == m->journal_size) tail = 0;
    } while (m->journal_count && (--drop > 0 || !(m->journal[tail].kind & J_STEP)));
}

static RvJournalEntry *rv_journal_push(RvMachine *m) {
    if (m->journal_count == m->journal_size) rv_journal_drop(m);
    RvJournalEntry *e = &m->journal[m->journal_head];
    if (++m->journal_head == m->journal_size) m->journal_head = 0;
    m->journal_count++;
    return e;
}

static void rv_note(RvMachine *m, uint32_t kind, uint32_t a, uint32_t b, uint32_t c) {
    if (!m->journal_on) return;
    RvJournalEntry *e = rv_journal_push(m);
    e->kind = kind;
    e->a = a;
    e->b = b;
    e->c = c;
}

/* Starts the journal of a step whose changes come after it. */
static void rv_note_step(RvMachine *m, uint32_t pc) { rv_note(m, J_STEP, pc, 0, 0); }

static void rv_note_reg(RvMachine *m, int r) { rv_note(m, J_REG | (uint32_t)r << 8, m->x[r], 0, 0); }

/* A store's old bytes, kept for Step back. */
static void rv_note_mem(RvMachine *m, uint32_t a, int size) {
    if (!m->journal_on) return;
    uint8_t old[4] = { 0, 0, 0, 0 };
    const uint8_t *p = rv_page_of(m, a);
    if (p) memcpy(old, p + (a & (RV_PAGE_SIZE - 1)), (size_t)size);
    rv_note(m, J_MEM | (uint32_t)size << 8, a, rv_get(old, 4), 0);
}

static void rv_set_reg(RvMachine *m, int r, uint32_t v) {
    if (!r) return;
    rv_note_reg(m, r);
    m->x[r] = v;
}

void rv_journal(RvMachine *m, int on) {
    m->journal_on = on && m->journal_size > 0;
    m->journal_head = m->journal_count = 0;
}

int rv_can_step_back(const RvMachine *m) { return m->journal_count > 0 || m->faulted || m->want; }

/* ------------------------------------------------------------------ output */

static void rv_write(RvMachine *m, const char *text, int length) {
    if (length <= 0) return;
    rv_note(m, J_OUTPUT, (uint32_t)m->output, (uint32_t)(m->output >> 32), 0);
    m->output += (uint64_t)length;
    if (m->host.write) m->host.write(m->host.context, text, length);
}

/* A character 0-255, as RARS prints it: the Latin-1 character, in UTF-8. */
static void rv_write_char(RvMachine *m, uint32_t c) {
    char bytes[2];
    c &= 0xff;
    if (c < 0x80) {
        bytes[0] = (char)c;
        rv_write(m, bytes, 1);
    } else {
        bytes[0] = (char)(0xc0 | c >> 6);
        bytes[1] = (char)(0x80 | (c & 63));
        rv_write(m, bytes, 2);
    }
}

/* What a store into the MMIO registers does once it is made, as MarsDevices does. */
static void rv_store_side(RvMachine *m, int side, uint32_t v) {
    if (side == SIDE_TRANSMIT) {
        rv_write_char(m, v);
    } else if (side == SIDE_CONTROL) {
        rv_mmio_word(m, RV_DISPLAY_CONTROL, 1);
        rv_mmio_word(m, RV_KEY_CONTROL, m->receiver_armed ? 1 : 0);
    }
}

/* A read of the receiver's data register takes the character it showed. */
static void rv_load_side(RvMachine *m) {
    if (m->receiver_armed && m->key_count) {
        rv_note(m, J_KEY, (uint32_t)m->keys[m->key_head], 0, 0);
        m->key_head = (m->key_head + 1) & 63;
        m->key_count--;
    }
    rv_refresh_receiver(m);
}

void rv_key_push(RvMachine *m, int ch) {
    if (m->key_count == 64) {
        m->key_head = (m->key_head + 1) & 63;
        m->key_count--;
    }
    m->keys[(m->key_head + m->key_count) & 63] = ch;
    m->key_count++;
    rv_refresh_receiver(m);
}

/* ------------------------------------------------------------------ java.util.Random */

#define RV_RANDOM_MULTIPLIER 0x5DEECE66DULL
#define RV_RANDOM_MASK ((1ULL << 48) - 1)

static int32_t rv_random_next(uint64_t *seed, int bits) {
    *seed = (*seed * RV_RANDOM_MULTIPLIER + 0xB) & RV_RANDOM_MASK;
    return (int32_t)(uint32_t)(*seed >> (48 - bits));
}

/* The generator numbered `id`, made from the host's seed on its first use; NULL when there are too
 * many. Its state before is journaled. */
static uint64_t *rv_random_get(RvMachine *m, int id, int seeding, int64_t seed) {
    int i = 0;
    while (i < m->random_count && m->random[i].id != id) i++;
    if (i == m->random_count) {
        if (i == 16) return NULL;
        rv_note(m, J_RANDOM | (uint32_t)m->random_count << 8, (uint32_t)i, 0, 0);
        m->random_count++;
        m->random[i].id = id;
        m->random[i].seeded = 1;
        if (!seeding) {
            int64_t from = m->host.random_seed ? m->host.random_seed(m->host.context)
                                               : (m->host.time_ms ? m->host.time_ms(m->host.context) : 0);
            m->random[i].seed = ((uint64_t)from ^ RV_RANDOM_MULTIPLIER) & RV_RANDOM_MASK;
        }
    } else {
        rv_note(m, J_RANDOM | (uint32_t)m->random_count << 8, (uint32_t)i, (uint32_t)m->random[i].seed,
                (uint32_t)(m->random[i].seed >> 32));
    }
    if (seeding) m->random[i].seed = ((uint64_t)seed ^ RV_RANDOM_MULTIPLIER) & RV_RANDOM_MASK;
    return &m->random[i].seed;
}

static int32_t rv_random_below(uint64_t *seed, int32_t bound) {
    int32_t r = rv_random_next(seed, 31);
    int32_t mask = bound - 1;
    if ((bound & mask) == 0) return (int32_t)(((int64_t)bound * (int64_t)r) >> 31);
    for (int32_t u = r; (int32_t)((uint32_t)u - (uint32_t)(r = u % bound) + (uint32_t)mask) < 0; u = rv_random_next(seed, 31)) {}
    return r;
}

/* ------------------------------------------------------------------ ecalls */

static void rv_end(RvMachine *m, int code, int cliff) {
    rv_note(m, J_EXIT, (uint32_t)(m->exited | m->cliff << 1), (uint32_t)m->exit_code, 0);
    m->exited = 1;
    m->cliff = cliff;
    m->exit_code = code;
}

/* Whether the program may read `length` bytes from `a` (length -1: up to a NUL, which `*length`
 * then gets); sets the fault when not. */
static int rv_readable(RvMachine *m, uint32_t a, int32_t *length) {
    for (int32_t i = 0; *length < 0 || i < *length; i++) {
        uint32_t at = a + (uint32_t)i;
        int region = rv_region(at);
        if (region == REGION_NONE || region == REGION_TEXT) {
            rv_fault(m, region == REGION_TEXT ? "Cannot read directly from the text segment: 0x%08x"
                                               : "Load address out of range: 0x%08x", at);
            return 0;
        }
        if (*length < 0) {
            const uint8_t *p = rv_page_of(m, at);
            if (!p || !p[at & (RV_PAGE_SIZE - 1)]) {
                *length = i;
                return 1;
            }
        }
    }
    return 1;
}

/* Writes `length` bytes of memory from `a` to the console, a piece at a time. */
static void rv_print_memory(RvMachine *m, uint32_t a, int32_t length) {
    char piece[256];
    int n = 0;
    for (int32_t i = 0; i < length; i++, a++) {
        const uint8_t *p = rv_page_of(m, a);
        piece[n++] = (char)(p ? p[a & (RV_PAGE_SIZE - 1)] : 0);
        if (n == (int)sizeof piece) rv_write(m, piece, n), n = 0;
    }
    rv_write(m, piece, n);
}

/* Runs the ecall at pc. Answers a stop, or 0 to go on; m->pc is where to go on from. */
static int rv_ecall(RvMachine *m, uint32_t pc) {
    uint32_t *x = m->x;
    int32_t a0 = (int32_t)x[10], a1 = (int32_t)x[11];
    int service = (int)x[17];
    char text[48];
    m->pc = pc;
    switch (service) {
    case 5: m->want = RV_WANT_INT; return RV_STOP_INPUT;
    case 8: m->want = RV_WANT_STRING; m->want_size = a1; return RV_STOP_INPUT;
    case 12: m->want = RV_WANT_CHAR; return RV_STOP_INPUT;
    case 1: case 34: case 35: case 36: case 4: case 11: case 9: case 10: case 93: case 30: case 31: case 32: case 33:
    case 40: case 41: case 42: case 64:
        break;
    default:
        m->faulted = 1;
        snprintf(m->fault, sizeof m->fault, "invalid or unimplemented syscall service: %d", service);
        return RV_STOP_FAULT;
    }
    /* checks that change nothing come before the journal */
    if (service == 9 && a0 < 0) {
        m->faulted = 1;
        snprintf(m->fault, sizeof m->fault, "request (%d) is negative heap amount (syscall 9)", a0);
        return RV_STOP_FAULT;
    }
    if (service == 9 && (((uint64_t)m->heap + (uint64_t)(uint32_t)a0 + 3) & ~3ULL) >= RV_DATA_LIMIT) {
        m->faulted = 1;
        snprintf(m->fault, sizeof m->fault, "request (%d) exceeds available heap storage (syscall 9)", a0);
        return RV_STOP_FAULT;
    }
    if (service == 42 && a1 <= 0) {
        m->faulted = 1;
        snprintf(m->fault, sizeof m->fault, "Upper bound of range cannot be negative (syscall 42)");
        return RV_STOP_FAULT;
    }
    int32_t length = -1;
    if (service == 4 && !rv_readable(m, (uint32_t)a0, &length)) return RV_STOP_FAULT;
    if (service == 64) {
        length = (int32_t)x[12];
        if (length > 0 && (a0 == 1 || a0 == 2) && !rv_readable(m, (uint32_t)a1, &length)) return RV_STOP_FAULT;
    }
    if (service == 41 || service == 42) {
        int known = 0;
        for (int i = 0; i < m->random_count; i++) known |= m->random[i].id == a0;
        if (!known && m->random_count == 16) {
            m->faulted = 1;
            snprintf(m->fault, sizeof m->fault, "Too many random generators: at most 16");
            return RV_STOP_FAULT;
        }
    }
    rv_note_step(m, pc);
    int stop = 0;
    m->pc = pc + 4;
    switch (service) {
    case 1: rv_write(m, text, snprintf(text, sizeof text, "%d", a0)); break;
    case 34: rv_write(m, text, snprintf(text, sizeof text, "0x%08x", (uint32_t)a0)); break;
    case 36: rv_write(m, text, snprintf(text, sizeof text, "%u", (uint32_t)a0)); break;
    case 35:
        for (int i = 0; i < 32; i++) text[i] = (char)('0' + (((uint32_t)a0 >> (31 - i)) & 1));
        rv_write(m, text, 32);
        break;
    case 11: rv_write_char(m, (uint32_t)a0); break;
    case 4: rv_print_memory(m, (uint32_t)a0, length); break;
    case 64:
        if (length < 0 || (a0 != 1 && a0 != 2)) {
            rv_set_reg(m, 10, (uint32_t)-1);
            break;
        }
        rv_print_memory(m, (uint32_t)a1, length);
        rv_set_reg(m, 10, (uint32_t)length);
        break;
    case 9: {
        uint32_t old = m->heap;
        rv_note(m, J_HEAP, old, 0, 0);
        m->heap = (uint32_t)(((uint64_t)old + (uint32_t)a0 + 3) & ~3ULL);
        rv_set_reg(m, 10, old);
        break;
    }
    case 10: rv_end(m, 0, 0); stop = RV_STOP_EXIT; break;
    case 93: rv_end(m, a0, 0); stop = RV_STOP_EXIT; break;
    case 30: {
        int64_t now = m->host.time_ms ? m->host.time_ms(m->host.context) : 0;
        rv_set_reg(m, 10, (uint32_t)now);
        rv_set_reg(m, 11, (uint32_t)((uint64_t)now >> 32));
        break;
    }
    case 31: case 33: {
        int pitch = a0, duration = a1, instrument = (int32_t)x[12], volume = (int32_t)x[13];
        if (pitch < 0 || pitch > 127) pitch = 60;
        if (duration < 0) duration = 1000;
        if (instrument < 0 || instrument > 127) instrument = 0;
        if (volume < 0 || volume > 127) volume = 100;
        if (m->host.midi) m->host.midi(m->host.context, pitch, duration, instrument, volume);
        if (service == 33) {
            m->sleep_ms = duration;
            stop = RV_STOP_SLEEP;
        }
        break;
    }
    case 32: m->sleep_ms = a0 > 0 ? a0 : 0; stop = RV_STOP_SLEEP; break;
    case 40:
        if (!rv_random_get(m, a0, 1, (int64_t)a1)) {
            /* a seventeenth generator: the oldest is reseeded in its place */
            m->random[0].id = a0;
            m->random[0].seed = ((uint64_t)(int64_t)a1 ^ RV_RANDOM_MULTIPLIER) & RV_RANDOM_MASK;
        }
        break;
    case 41: case 42: {
        uint64_t *seed = rv_random_get(m, a0, 0, 0);
        rv_set_reg(m, 10, (uint32_t)(service == 41 ? rv_random_next(seed, 32) : rv_random_below(seed, a1)));
        break;
    }
    }
    return stop;
}

/* Completes a read ecall: the input's effect, its echo and the move past the ecall are one step. */
static int rv_supply(RvMachine *m, const char *line, int length, int is_line) {
    if (!m->want || m->exited || m->faulted) return -1;
    int want = m->want;
    uint32_t pc = m->pc;
    m->want = RV_WANT_NONE;
    /* uncounted until it completes: Step back from a refused input takes back only its echo */
    rv_note(m, J_STEP | J_UNCOUNTED, pc, 0, 0);
    int marker = m->journal_on ? (m->journal_head + m->journal_size - 1) % m->journal_size : -1;
    rv_write(m, line, length);
    if (is_line) rv_write(m, "\n", 1);
    if (want == RV_WANT_CHAR) {
        if (!length) {
            m->faulted = 1;
            snprintf(m->fault, sizeof m->fault, "invalid char input (syscall 12)");
            m->stop = RV_STOP_FAULT;
            return 0;
        }
        /* the first character, as Java reads it: a UTF-8 sequence is one character */
        uint32_t c = (unsigned char)line[0];
        if (c >= 0xc0 && length >= 2) {
            int extra = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : 1;
            c &= 0x3f >> extra;
            for (int i = 1; i <= extra && i < length; i++) c = c << 6 | ((unsigned char)line[i] & 63);
            if (c >= 0x10000) c = 0xd800 + ((c - 0x10000) >> 10);   /* its first UTF-16 unit */
        }
        rv_set_reg(m, 10, c);
    } else if (want == RV_WANT_INT) {
        int from = 0, to = length;
        while (from < to && (unsigned char)line[from] <= ' ') from++;
        while (to > from && (unsigned char)line[to - 1] <= ' ') to--;
        int negative = 0, i = from, ok = to > from;
        if (ok && (line[i] == '-' || line[i] == '+')) negative = line[i++] == '-';
        int64_t value = 0;
        if (i >= to) ok = 0;
        for (; ok && i < to; i++) {
            if (line[i] < '0' || line[i] > '9') ok = 0;
            else value = value * 10 + (line[i] - '0');
            if (value > 2147483648LL) ok = 0;
        }
        if (negative) value = -value;
        if (!ok || value > 2147483647LL) {
            m->faulted = 1;
            snprintf(m->fault, sizeof m->fault, "invalid integer input (syscall 5)");
            m->stop = RV_STOP_FAULT;
            return 0;
        }
        rv_set_reg(m, 10, (uint32_t)(int32_t)value);
    } else {
        uint32_t buffer = m->x[10];
        int max = m->want_size - 1, terminate = 1;
        if (max < 0) max = 0, terminate = 0;
        /* at most `max` characters, then at most `max` of their bytes */
        int bytes = 0, characters = 0;
        while (bytes < length && characters < max) {
            unsigned char c = (unsigned char)line[bytes];
            int size = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 1;
            bytes += size;
            characters += size == 4 ? 2 : 1;
        }
        if (bytes > length) bytes = length;
        if (bytes > max) bytes = max;
        int total = bytes + (bytes < max) + terminate;
        for (int i = 0; i < total; i++) {
            int region = rv_region(buffer + (uint32_t)i);
            if (region == REGION_NONE || region == REGION_TEXT) {
                rv_fault(m, region == REGION_TEXT ? "Cannot write directly to the text segment: 0x%08x"
                                                   : "Store address out of range: 0x%08x", buffer + (uint32_t)i);
                m->stop = RV_STOP_FAULT;
                return 0;
            }
        }
        for (int i = 0; i < total; i++) {
            uint32_t a = buffer + (uint32_t)i;
            uint8_t *p = rv_page_make(m, a);
            if (!p) {
                rv_fault(m, "Out of memory at 0x%08x", a);
                m->stop = RV_STOP_FAULT;
                return 0;
            }
            rv_note_mem(m, a, 1);
            p[a & (RV_PAGE_SIZE - 1)] = (uint8_t)(i < bytes ? line[i] : i == bytes && bytes < max ? '\n' : 0);
            if (a - m->watch_lo < m->watch_hi - m->watch_lo) rv_watched(m, a);
        }
    }
    if (marker >= 0 && m->journal_count && m->journal[marker].kind == (J_STEP | J_UNCOUNTED) && m->journal[marker].a == pc)
        m->journal[marker].kind = J_STEP;
    m->pc = pc + 4;
    m->steps++;
    m->stop = RV_STOP_NONE;
    return 0;
}

int rv_supply_line(RvMachine *m, const char *line) { return rv_supply(m, line, (int)strlen(line), m->want != RV_WANT_CHAR); }

int rv_supply_char(RvMachine *m, int ch) {
    char bytes[4];
    int n;
    uint32_t c = (uint32_t)ch;
    if (c < 0x80) bytes[0] = (char)c, n = 1;
    else if (c < 0x800) bytes[0] = (char)(0xc0 | c >> 6), bytes[1] = (char)(0x80 | (c & 63)), n = 2;
    else bytes[0] = (char)(0xe0 | c >> 12), bytes[1] = (char)(0x80 | ((c >> 6) & 63)), bytes[2] = (char)(0x80 | (c & 63)), n = 3;
    if (m->want == RV_WANT_CHAR) return rv_supply(m, bytes, n, 0);
    bytes[n] = 0;
    return rv_supply(m, bytes, n, 1);
}

/* ------------------------------------------------------------------ running */

/* Ends the program that ran off its text, as RARS does: pc moves one more word on. The change
 * joins the step that got there, or is a step of its own when there was none. */
static void rv_cliff(RvMachine *m, uint32_t pc) {
    if (m->journal_on && !m->journal_count) rv_note(m, J_STEP | J_UNCOUNTED, pc, 0, 0);
    rv_end(m, 0, 1);
    m->pc = pc + 4;
}

static int rv_fetch_fault(RvMachine *m, uint32_t pc) {
    if (rv_region(pc) == REGION_TEXT && !(pc & 3)) {
        rv_cliff(m, pc);
        return RV_STOP_EXIT;
    }
    m->pc = pc;
    rv_fault(m, "Instruction fetch outside the program: 0x%08x", pc);
    return RV_STOP_FAULT;
}

/* A guest word or halfword at p: one host access on a little-endian host, as x86 is. */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define RV_GET32(p) ({ uint32_t v_; memcpy(&v_, (p), 4); v_; })
#define RV_GET16(p) ({ uint16_t v_; memcpy(&v_, (p), 2); (uint32_t)v_; })
#define RV_PUT32(p, v) do { uint32_t v_ = (v); memcpy((p), &v_, 4); } while (0)
#define RV_PUT16(p, v) do { uint16_t v_ = (uint16_t)(v); memcpy((p), &v_, 2); } while (0)
#else
#define RV_GET32(p) rv_get((p), 4)
#define RV_GET16(p) rv_get((p), 2)
#define RV_PUT32(p, v) rv_put((p), 4, (v))
#define RV_PUT16(p, v) rv_put((p), 2, (v))
#endif

/*
 * The run loop. It keeps pc, the instruction count and the journal's place in locals, so that the
 * common instruction is a fetch, a switch, its work and, with the journal, three stores; whatever
 * calls out (an ecall, a device, the end) hands them back to the machine first (RV_SYNC) and takes
 * them again after (RV_RELOAD).
 */
static inline __attribute__((always_inline)) int rv_loop(RvMachine *m, uint32_t limit, int breakpoints, const int journal) {
    uint32_t *x = m->x;
    uint32_t pc = m->pc, n = 0;
    const RvCode *code = m->code;
    const uint32_t words = (uint32_t)m->program->text_words;
    RvJournalEntry *ring = m->journal, *e;
    /* the ring's size is a power of two; `room` is how many entries fit before it is full */
    uint32_t head = (uint32_t)m->journal_head, room = (uint32_t)(m->journal_size - m->journal_count);
    const uint32_t mask = (uint32_t)m->journal_size - 1;
    int stop = RV_STOP_LIMIT;
#define RV_SYNC() do { m->pc = pc; m->steps += n; n = 0; if (journal) m->journal_head = (int)head, m->journal_count = m->journal_size - (int)room; } while (0)
#define RV_RELOAD() do { pc = m->pc; if (journal) head = (uint32_t)m->journal_head, room = (uint32_t)(m->journal_size - m->journal_count); } while (0)
#define RV_PUSH() do {                                                                 \
        if (!room) {                                                                   \
            m->journal_head = (int)head, m->journal_count = m->journal_size;           \
            rv_journal_drop(m);                                                        \
            room = (uint32_t)(m->journal_size - m->journal_count);                     \
        }                                                                              \
        e = &ring[head];                                                               \
        head = (head + 1) & mask;                                                      \
        room--;                                                                        \
    } while (0)
#define RV_STEP_REG() do { if (journal) { RV_PUSH(); e->kind = J_STEP | J_REG | (uint32_t)c->rd << 8; e->a = pc; e->b = x[c->rd]; } } while (0)
#define RV_STEP_ONLY() do { if (journal) { RV_PUSH(); e->kind = J_STEP; e->a = pc; } } while (0)
#define RV_FAIL() do { stop = RV_STOP_FAULT; goto out; } while (0)
    while (n < limit) {
        uint32_t index = (pc - RV_TEXT_BASE) >> 2;
        if (index >= words) {
            RV_SYNC();
            stop = rv_fetch_fault(m, pc);
            RV_RELOAD();
            goto out;
        }
        const RvCode *c = &code[index];
        unsigned op = c->op;
    again:
        switch (op) {
#define RV_ALU(expr) RV_STEP_REG(); x[c->rd] = (expr); pc += 4; break
        case O_NOP: RV_STEP_ONLY(); pc += 4; break;
        case O_LUI: RV_ALU((uint32_t)c->imm);
        case O_AUIPC: RV_ALU(pc + (uint32_t)c->imm);
        case O_LI: RV_ALU((uint32_t)c->imm);
        case O_ADDI: RV_ALU(x[c->rs1] + (uint32_t)c->imm);
        case O_SLTI: RV_ALU((int32_t)x[c->rs1] < c->imm);
        case O_SLTIU: RV_ALU(x[c->rs1] < (uint32_t)c->imm);
        case O_XORI: RV_ALU(x[c->rs1] ^ (uint32_t)c->imm);
        case O_ORI: RV_ALU(x[c->rs1] | (uint32_t)c->imm);
        case O_ANDI: RV_ALU(x[c->rs1] & (uint32_t)c->imm);
        case O_SLLI: RV_ALU(x[c->rs1] << c->imm);
        case O_SRLI: RV_ALU(x[c->rs1] >> c->imm);
        case O_SRAI: RV_ALU((uint32_t)((int32_t)x[c->rs1] >> c->imm));
        case O_ADD: RV_ALU(x[c->rs1] + x[c->rs2]);
        case O_SUB: RV_ALU(x[c->rs1] - x[c->rs2]);
        case O_SLL: RV_ALU(x[c->rs1] << (x[c->rs2] & 31));
        case O_SLT: RV_ALU((int32_t)x[c->rs1] < (int32_t)x[c->rs2]);
        case O_SLTU: RV_ALU(x[c->rs1] < x[c->rs2]);
        case O_XOR: RV_ALU(x[c->rs1] ^ x[c->rs2]);
        case O_SRL: RV_ALU(x[c->rs1] >> (x[c->rs2] & 31));
        case O_SRA: RV_ALU((uint32_t)((int32_t)x[c->rs1] >> (x[c->rs2] & 31)));
        case O_OR: RV_ALU(x[c->rs1] | x[c->rs2]);
        case O_AND: RV_ALU(x[c->rs1] & x[c->rs2]);
        case O_MUL: RV_ALU(x[c->rs1] * x[c->rs2]);
        case O_MULH: RV_ALU((uint32_t)((uint64_t)((int64_t)(int32_t)x[c->rs1] * (int64_t)(int32_t)x[c->rs2]) >> 32));
        case O_MULHSU: RV_ALU((uint32_t)((uint64_t)((int64_t)(int32_t)x[c->rs1] * (int64_t)(uint64_t)x[c->rs2]) >> 32));
        case O_MULHU: RV_ALU((uint32_t)(((uint64_t)x[c->rs1] * (uint64_t)x[c->rs2]) >> 32));
        case O_DIV: {
            int32_t a = (int32_t)x[c->rs1], b = (int32_t)x[c->rs2];
            RV_ALU(b == 0 ? 0xffffffffu : (a == INT32_MIN && b == -1) ? (uint32_t)a : (uint32_t)(a / b));
        }
        case O_DIVU: {
            uint32_t a = x[c->rs1], b = x[c->rs2];
            RV_ALU(b == 0 ? 0xffffffffu : a / b);
        }
        case O_REM: {
            int32_t a = (int32_t)x[c->rs1], b = (int32_t)x[c->rs2];
            RV_ALU(b == 0 ? (uint32_t)a : (a == INT32_MIN && b == -1) ? 0u : (uint32_t)(a % b));
        }
        case O_REMU: {
            uint32_t a = x[c->rs1], b = x[c->rs2];
            RV_ALU(b == 0 ? a : a % b);
        }
        case O_JAL: RV_STEP_REG(); x[c->rd] = pc + 4; pc += (uint32_t)c->imm; break;
        case O_J: RV_STEP_ONLY(); pc += (uint32_t)c->imm; break;
        case O_JALR: case O_JR: {
            uint32_t target = (x[c->rs1] + (uint32_t)c->imm) & ~1u;
            if (op == O_JALR) {
                RV_STEP_REG();
                x[c->rd] = pc + 4;
            } else {
                RV_STEP_ONLY();
            }
            pc = target;
            if (target & 3) {
                /* the jump is made, and fetching there fails, as in RARS */
                n++;
                rv_fault(m, "Instruction address not aligned to a word: 0x%08x", target);
                RV_FAIL();
            }
            break;
        }
#define RV_BRANCH(cond) RV_STEP_ONLY(); pc += (cond) ? (uint32_t)c->imm : 4u; break
        case O_BEQ: RV_BRANCH(x[c->rs1] == x[c->rs2]);
        case O_BNE: RV_BRANCH(x[c->rs1] != x[c->rs2]);
        case O_BLT: RV_BRANCH((int32_t)x[c->rs1] < (int32_t)x[c->rs2]);
        case O_BGE: RV_BRANCH((int32_t)x[c->rs1] >= (int32_t)x[c->rs2]);
        case O_BLTU: RV_BRANCH(x[c->rs1] < x[c->rs2]);
        case O_BGEU: RV_BRANCH(x[c->rs1] >= x[c->rs2]);
        /* a load: the remembered page, or the slow path, which may fault or have a device to tell */
#define RV_LOAD(bytes, get, convert, misaligned) {                                     \
            uint32_t a = x[c->rs1] + (uint32_t)c->imm, v;                               \
            int side = 0;                                                               \
            if (a & (bytes - 1)) {                                                      \
                rv_fault(m, misaligned, a);                                             \
                RV_FAIL();                                                              \
            }                                                                           \
            if ((a >> 12) == m->read_tag) {                                             \
                v = get(m->read_page + (a & (RV_PAGE_SIZE - 1)));                       \
            } else {                                                                    \
                v = rv_load_slow(m, a, bytes);                                          \
                if (m->faulted) RV_FAIL();                                              \
                side = m->side;                                                         \
            }                                                                           \
            RV_STEP_REG();                                                              \
            if (c->rd) x[c->rd] = convert;                                              \
            if (side) {                                                                 \
                RV_SYNC();                                                              \
                rv_load_side(m);                                                        \
                RV_RELOAD();                                                            \
            }                                                                           \
            pc += 4;                                                                    \
            break;                                                                      \
        }
#define RV_GET8(p) (*(p))
        case O_LW: RV_LOAD(4, RV_GET32, v, "Load address not aligned to word boundary: 0x%08x")
        case O_LH: RV_LOAD(2, RV_GET16, (uint32_t)(int32_t)(int16_t)v, "Load address not aligned on halfword boundary: 0x%08x")
        case O_LHU: RV_LOAD(2, RV_GET16, v, "Load address not aligned on halfword boundary: 0x%08x")
        case O_LB: RV_LOAD(1, RV_GET8, (uint32_t)(int32_t)(int8_t)v, "")
        case O_LBU: RV_LOAD(1, RV_GET8, v, "")
        /* a store: its old bytes go into the journal first */
#define RV_STORE(bytes, get, put, misaligned) {                                        \
            uint32_t a = x[c->rs1] + (uint32_t)c->imm, v = x[c->rs2];                   \
            int side = 0;                                                               \
            uint8_t *p;                                                                 \
            if (a & (bytes - 1)) {                                                      \
                rv_fault(m, misaligned, a);                                             \
                RV_FAIL();                                                              \
            }                                                                           \
            if ((a >> 12) == m->write_tag) {                                            \
                p = m->write_page + (a & (RV_PAGE_SIZE - 1));                           \
            } else {                                                                    \
                p = rv_store_slow(m, a, bytes, v);                                      \
                if (!p) RV_FAIL();                                                      \
                side = m->side;                                                         \
            }                                                                           \
            if (journal) {                                                              \
                RV_PUSH();                                                              \
                e->kind = J_STEP | J_MEM | (uint32_t)bytes << 8;                        \
                e->a = pc;                                                              \
                e->b = a;                                                               \
                e->c = get(p);                                                          \
            }                                                                           \
            put(p, v);                                                                  \
            if (a - m->watch_lo < m->watch_hi - m->watch_lo) rv_watched(m, a);        \
            if (side) {                                                                 \
                RV_SYNC();                                                              \
                rv_store_side(m, side, v);                                              \
                RV_RELOAD();                                                            \
            }                                                                           \
            pc += 4;                                                                    \
            break;                                                                      \
        }
#define RV_PUT8(p, v) (*(p) = (uint8_t)(v))
        case O_SW: RV_STORE(4, RV_GET32, RV_PUT32, "Store address not aligned to word boundary: 0x%08x")
        case O_SH: RV_STORE(2, RV_GET16, RV_PUT16, "Store address not aligned on halfword boundary: 0x%08x")
        case O_SB: RV_STORE(1, RV_GET8, RV_PUT8, "")
        case O_ECALL: {
            RV_SYNC();
            int r = rv_ecall(m, pc);
            RV_RELOAD();
            if (r == RV_STOP_INPUT || r == RV_STOP_FAULT) {
                stop = r;
                goto out;
            }
            if (r) {
                n++;
                stop = r;
                goto out;
            }
            break;
        }
        case O_EBREAK:
            RV_STEP_ONLY();
            pc += 4;
            n++;
            stop = RV_STOP_BREAKPOINT;
            goto out;
        case O_BREAK:
            if (breakpoints && n > 0) {
                stop = RV_STOP_BREAKPOINT;
                goto out;
            }
            op = m->real_op[index];
            goto again;
        case O_ILLEGAL:
            rv_fault(m, "Illegal instruction 0x%08x", m->program->text[index]);
            RV_FAIL();
        default:
            __builtin_unreachable();
        }
        n++;
    }
    /* the call that runs the last instruction ends the program, as in RARS */
    if ((pc - RV_TEXT_BASE) >> 2 >= words && rv_region(pc) == REGION_TEXT && !(pc & 3)) {
        RV_SYNC();
        rv_cliff(m, pc);
        RV_RELOAD();
        stop = RV_STOP_EXIT;
    }
out:
    RV_SYNC();
    return stop;
#undef RV_SYNC
#undef RV_RELOAD
#undef RV_PUSH
#undef RV_STEP_REG
#undef RV_STEP_ONLY
#undef RV_FAIL
#undef RV_ALU
#undef RV_BRANCH
#undef RV_LOAD
#undef RV_STORE
}

int rv_run(RvMachine *m, uint32_t limit, int breakpoints) {
    if (m->exited) return m->stop = RV_STOP_EXIT;
    if (m->faulted) return m->stop = RV_STOP_FAULT;
    if (m->want) return m->stop = RV_STOP_INPUT;
    if (!limit) return m->stop = RV_STOP_LIMIT;
    int stop = m->journal_on ? rv_loop(m, limit, breakpoints, 1) : rv_loop(m, limit, breakpoints, 0);
    return m->stop = stop;
}

int rv_step_back(RvMachine *m) {
    int changed = 0;
    if (m->faulted) m->faulted = 0, m->fault[0] = 0, changed = 1;
    if (m->want) m->want = RV_WANT_NONE, changed = 1;
    while (m->journal_count) {
        if (--m->journal_head < 0) m->journal_head = m->journal_size - 1;
        m->journal_count--;
        RvJournalEntry *e = &m->journal[m->journal_head];
        uint32_t kind = e->kind, type = kind & 0xff, detail = (kind >> 8) & 0xff;
        int step = (kind & J_STEP) != 0;
        uint32_t a = step ? e->b : e->a, b = step ? e->c : e->b;
        changed = 1;
        switch (type) {
        case J_REG: m->x[detail] = step ? e->b : e->a; break;
        case J_MEM: {
            uint8_t *p = rv_page_of(m, a);
            if (p) rv_put(p + (a & (RV_PAGE_SIZE - 1)), (int)detail, b);
            if (a - m->watch_lo < m->watch_hi - m->watch_lo) rv_watched(m, a);
            break;
        }
        case J_HEAP: m->heap = e->a; break;
        case J_RANDOM:
            if ((int)detail < m->random_count) m->random_count = (int)detail;
            else m->random[e->a].seed = (uint64_t)e->b | (uint64_t)e->c << 32;
            break;
        case J_OUTPUT:
            m->output = (uint64_t)e->a | (uint64_t)e->b << 32;
            if (m->host.rewind) m->host.rewind(m->host.context, m->output);
            break;
        case J_KEY:
            if (m->key_count == 64) m->key_count--;
            m->key_head = (m->key_head + 63) & 63;
            m->keys[m->key_head] = (int)e->a;
            m->key_count++;
            break;
        case J_EXIT:
            m->exited = e->a & 1;
            m->cliff = (e->a >> 1) & 1;
            m->exit_code = (int)e->b;
            break;
        }
        if (step) {
            m->pc = e->a;
            if (!(kind & J_UNCOUNTED) && m->steps) m->steps--;
            break;
        }
    }
    if (changed) {
        /* the devices answer for now: Ready for the keys waiting, the display always ready */
        rv_mmio_word(m, RV_DISPLAY_CONTROL, 1);
        rv_refresh_receiver(m);
        m->stop = RV_STOP_NONE;
    }
    return changed;
}

/* ------------------------------------------------------------------ breakpoints */

void rv_set_breakpoint(RvMachine *m, uint32_t address, int on) {
    uint32_t index = (address - RV_TEXT_BASE) >> 2;
    if ((address & 3) || index >= (uint32_t)m->program->text_words) return;
    if (on && !m->breaks[index]) {
        m->breaks[index] = 1;
        m->real_op[index] = m->code[index].op;
        m->code[index].op = O_BREAK;
    } else if (!on && m->breaks[index]) {
        m->breaks[index] = 0;
        m->code[index].op = m->real_op[index];
    }
}

void rv_clear_breakpoints(RvMachine *m) {
    for (int i = 0; i < m->program->text_words; i++)
        if (m->breaks[i]) rv_set_breakpoint(m, RV_TEXT_BASE + 4u * (uint32_t)i, 0);
}

/* ------------------------------------------------------------------ the machine */

static void rv_free_pages(RvMachine *m) {
    for (int t = 0; t < 1024; t++) {
        if (!m->pages[t]) continue;
        for (int i = 0; i < 1024; i++) RV_FREE(m->pages[t][i]);
        RV_FREE(m->pages[t]);
        m->pages[t] = NULL;
    }
    m->page_count = 0;
}

void rv_reset(RvMachine *m) {
    const RvProgram *p = m->program;
    rv_free_pages(m);
    m->read_tag = m->write_tag = 0xffffffffu;
    m->read_page = NULL;
    m->write_page = NULL;
    memset(m->x, 0, sizeof m->x);
    m->x[2] = RV_SP_VALUE;
    m->x[3] = RV_GP_VALUE;
    m->pc = p->entry;
    m->steps = 0;
    m->stop = RV_STOP_NONE;
    m->exited = m->exit_code = m->cliff = m->faulted = 0;
    m->fault[0] = 0;
    m->want = RV_WANT_NONE;
    m->want_size = 0;
    m->sleep_ms = 0;
    m->output = 0;
    m->watch_dirty = 1;
    m->watch_from = m->watch_lo;
    m->watch_to = m->watch_hi ? m->watch_hi - 1 : 0;
    m->heap = RV_HEAP_BASE;
    m->key_head = m->key_count = 0;
    m->random_count = 0;
    m->journal_head = m->journal_count = 0;
    /* the static data, a page at a time, leaving pages of zeroes unmade */
    for (uint32_t offset = 0; offset < p->data_size;) {
        uint32_t a = p->data_base + offset, chunk = RV_PAGE_SIZE - (a & (RV_PAGE_SIZE - 1));
        if (chunk > p->data_size - offset) chunk = p->data_size - offset;
        int zero = 1;
        for (uint32_t i = 0; i < chunk && zero; i++) zero = p->data[offset + i] == 0;
        if (!zero) {
            uint8_t *page = rv_page_make(m, a);
            if (page) memcpy(page + (a & (RV_PAGE_SIZE - 1)), p->data + offset, chunk);
        }
        offset += chunk;
    }
    rv_mmio_word(m, RV_DISPLAY_CONTROL, 1);
    rv_mmio_word(m, RV_KEY_DATA, 0);
    rv_refresh_receiver(m);
}

int rv_machine_init(RvMachine *m, const RvProgram *program, const RvHost *host, int journal_size, int page_limit) {
    memset(m, 0, sizeof *m);
    m->program = program;
    if (host) m->host = *host;
    m->page_limit = page_limit > 0 ? page_limit : 4096;
    int words = program->text_words > 0 ? program->text_words : 1;
    m->code = RV_ALLOC((size_t)words * sizeof(RvCode));
    m->breaks = RV_ALLOC((size_t)words);
    m->real_op = RV_ALLOC((size_t)words);
    /* a power of two, so that the run loop's ring wraps with a mask */
    m->journal_size = 0;
    if (journal_size > 0) {
        m->journal_size = 1;
        while (m->journal_size < journal_size && m->journal_size < (1 << 28)) m->journal_size *= 2;
    }
    if (m->journal_size) m->journal = RV_ALLOC((size_t)m->journal_size * sizeof(RvJournalEntry));
    if (!m->code || !m->breaks || !m->real_op || (m->journal_size && !m->journal)) {
        rv_machine_free(m);
        return -1;
    }
    for (int i = 0; i < program->text_words; i++) m->code[i] = rv_decode(program->text[i]);
    m->journal_on = m->journal_size > 0;
    rv_reset(m);
    return 0;
}

void rv_machine_free(RvMachine *m) {
    rv_free_pages(m);
    RV_FREE(m->code);
    RV_FREE(m->breaks);
    RV_FREE(m->real_op);
    RV_FREE(m->journal);
    m->code = NULL;
    m->breaks = m->real_op = NULL;
    m->journal = NULL;
    m->journal_size = m->journal_count = 0;
}
