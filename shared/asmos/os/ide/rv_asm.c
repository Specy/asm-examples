/*
 * The assembler of rv.h: RARS 1.6's assembly language for RV32IM, in two passes.
 *
 * It follows RARS's own assembler (rars/assembler/Assembler.java, Tokenizer.java, TokenTypes.java
 * and OperandFormat.java) closely enough that a program assembles to the same machine code and the
 * same data, word for word, and that what RARS rejects is rejected here too:
 *
 * - A line is cut into tokens as RARS cuts it: spaces, tabs and commas part them, ( ) : are tokens
 *   of their own, a + or - joins the number after it unless an identifier comes before it, 'c' is
 *   the character's number, and an identifier named by .eqv is replaced by its text first.
 * - An integer is classed by the bits it needs (5 unsigned, 12 signed, 20 unsigned, 32), and an
 *   instruction takes its first form whose operands all fit; `li t0, 3000` is therefore the
 *   two-instruction form, as in RARS.
 * - A pseudo-instruction expands through RARS's own templates (PseudoOps.java), kept here as text.
 * - .word and .half align themselves unless .align 0 said not to, and a label just before them
 *   moves with them.
 *
 * The first pass lays out the text and the data and defines the labels; the second encodes the
 * instructions, now that every label has its address. An error names its line and the next line
 * is assembled all the same, so a Build lists several. Plain C, no asmOS: see rv.h.
 */
#include "rv.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RV_ALLOC
#define RV_ALLOC(size) calloc(1, (size))
#define RV_REALLOC(pointer, size) realloc((pointer), (size))
#define RV_FREE(pointer) free(pointer)
#endif

/* ------------------------------------------------------------------ tables */

static const char *const RV_ABI_NAMES[32] = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6",
};

const char *rv_register_name(int index) { return index >= 0 && index < 32 ? RV_ABI_NAMES[index] : "?"; }

/* Every mnemonic RARS knows for RV32: a token spelled like one is an operator, never a label
 * reference, as in RARS. Those this assembler does not assemble are refused with a sentence. */
static const char RV_RARS_MNEMONICS[] =
    "fadd.d fadd.s fclass.d fclass.s fcvt.d.s fcvt.d.w fcvt.d.wu fcvt.s.d fcvt.s.w fcvt.s.wu fcvt.w.d "
    "fcvt.w.s fcvt.wu.d fcvt.wu.s fdiv.d fdiv.s feq.d feq.s fld fle.d fle.s flt.d flt.s flw fmadd.d "
    "fmadd.s fmax.d fmax.s fmin.d fmin.s fmsub.d fmsub.s fmul.d fmul.s fmv.s.x fmv.x.s fnmadd.d "
    "fnmadd.s fnmsub.d fnmsub.s fsd fsgnj.d fsgnjn.d fsgnjn.s fsgnj.s fsgnjx.d fsgnjx.s fsqrt.d "
    "fsqrt.s fsub.d fsub.s fsw csrrc csrrci csrrs csrrsi csrrw csrrwi uret wfi amoadd.w amoand.w "
    "amomax.w amomaxu.w amomin.w amominu.w amoor.w amoswap.w amoxor.w lr.w sc.w andn clz cpop ctz max "
    "maxu min minu orc.b orn rev8 rol ror rori sext.b sext.h xnor zext.h sh1add sh2add sh3add bclr "
    "bclri bext bexti binv binvi bset bseti fmv.s fabs.s fneg.s fmv.d fabs.d fneg.d csrr csrw csrs "
    "csrc csrwi csrsi csrci frcsr fscsr frsr fssr frrm fsrm frflags fsflags rdcycle rdtime rdinstret "
    "rdcycleh rdtimeh rdinstreth fmv.x.w fmv.w.x fgt.s fge.s fgt.d fge.d";

static const char RV_CSR_NAMES[] =
    "ustatus fflags frm fcsr uie utvec uscratch uepc ucause utval uip cycle time instret cycleh timeh instreth";
static const char RV_ROUNDING[] = "rne rtz rdn rup rmm dyn";

/* How an instruction's fields sit in its word. */
enum { F_R, F_I, F_SH, F_LOAD, F_S, F_B, F_U, F_J, F_JALR, F_NONE, F_FENCE };

/*
 * One form of an instruction: what its operands look like, and either the fixed bits of a basic
 * instruction or RARS's templates for a pseudo-instruction. The operand kinds are
 *     r a register   5 an integer of 0-31   i of 12 signed bits   u of 20 bits   w of 32 bits
 *     l a label      ( )  parentheses       H %hi   L %lo
 * In a template, RGn is the register of operand token n (the mnemonic is token 0, parentheses
 * count), VLn and VHn the low 12 and high 20 bits of its value (the high part rounded for the
 * sign of the low), PCLn and PCHn the same of its label's distance from the first instruction,
 * LLn and LHn of its label's address, and LAB the label of the last operand. A mnemonic's forms
 * are tried in this order, which is RARS's, and the first that fits is taken.
 */
typedef struct {
    const char *name;
    const char *pattern;
    const char *expand;
    uint8_t format;
    uint32_t bits;
    const char *example;
} RvForm;

static const RvForm RV_FORMS[] = {
    { "add", "rrr", NULL, F_R, 0x00000033, "add t1, t2, t3" },
    { "sub", "rrr", NULL, F_R, 0x40000033, "sub t1, t2, t3" },
    { "sll", "rrr", NULL, F_R, 0x00001033, "sll t1, t2, t3" },
    { "slt", "rrr", NULL, F_R, 0x00002033, "slt t1, t2, t3" },
    { "sltu", "rrr", NULL, F_R, 0x00003033, "sltu t1, t2, t3" },
    { "xor", "rrr", NULL, F_R, 0x00004033, "xor t1, t2, t3" },
    { "srl", "rrr", NULL, F_R, 0x00005033, "srl t1, t2, t3" },
    { "sra", "rrr", NULL, F_R, 0x40005033, "sra t1, t2, t3" },
    { "or", "rrr", NULL, F_R, 0x00006033, "or t1, t2, t3" },
    { "and", "rrr", NULL, F_R, 0x00007033, "and t1, t2, t3" },
    { "mul", "rrr", NULL, F_R, 0x02000033, "mul t1, t2, t3" },
    { "mulh", "rrr", NULL, F_R, 0x02001033, "mulh t1, t2, t3" },
    { "mulhsu", "rrr", NULL, F_R, 0x02002033, "mulhsu t1, t2, t3" },
    { "mulhu", "rrr", NULL, F_R, 0x02003033, "mulhu t1, t2, t3" },
    { "div", "rrr", NULL, F_R, 0x02004033, "div t1, t2, t3" },
    { "divu", "rrr", NULL, F_R, 0x02005033, "divu t1, t2, t3" },
    { "rem", "rrr", NULL, F_R, 0x02006033, "rem t1, t2, t3" },
    { "remu", "rrr", NULL, F_R, 0x02007033, "remu t1, t2, t3" },
    { "addi", "rri", NULL, F_I, 0x00000013, "addi t1, t2, -100" },
    { "slti", "rri", NULL, F_I, 0x00002013, "slti t1, t2, -100" },
    { "sltiu", "rri", NULL, F_I, 0x00003013, "sltiu t1, t2, -100" },
    { "xori", "rri", NULL, F_I, 0x00004013, "xori t1, t2, -100" },
    { "ori", "rri", NULL, F_I, 0x00006013, "ori t1, t2, -100" },
    { "andi", "rri", NULL, F_I, 0x00007013, "andi t1, t2, -100" },
    { "slli", "rr5", NULL, F_SH, 0x00001013, "slli t1, t2, 10" },
    { "srli", "rr5", NULL, F_SH, 0x00005013, "srli t1, t2, 10" },
    { "srai", "rr5", NULL, F_SH, 0x40005013, "srai t1, t2, 10" },
    { "lb", "ri(r)", NULL, F_LOAD, 0x00000003, "lb t1, -100(t2)" },
    { "lh", "ri(r)", NULL, F_LOAD, 0x00001003, "lh t1, -100(t2)" },
    { "lw", "ri(r)", NULL, F_LOAD, 0x00002003, "lw t1, -100(t2)" },
    { "lbu", "ri(r)", NULL, F_LOAD, 0x00004003, "lbu t1, -100(t2)" },
    { "lhu", "ri(r)", NULL, F_LOAD, 0x00005003, "lhu t1, -100(t2)" },
    { "sb", "ri(r)", NULL, F_S, 0x00000023, "sb t1, -100(t2)" },
    { "sh", "ri(r)", NULL, F_S, 0x00001023, "sh t1, -100(t2)" },
    { "sw", "ri(r)", NULL, F_S, 0x00002023, "sw t1, -100(t2)" },
    { "beq", "rrl", NULL, F_B, 0x00000063, "beq t1, t2, label" },
    { "bne", "rrl", NULL, F_B, 0x00001063, "bne t1, t2, label" },
    { "blt", "rrl", NULL, F_B, 0x00004063, "blt t1, t2, label" },
    { "bge", "rrl", NULL, F_B, 0x00005063, "bge t1, t2, label" },
    { "bltu", "rrl", NULL, F_B, 0x00006063, "bltu t1, t2, label" },
    { "bgeu", "rrl", NULL, F_B, 0x00007063, "bgeu t1, t2, label" },
    { "lui", "ru", NULL, F_U, 0x00000037, "lui t1, 100000" },
    { "auipc", "ru", NULL, F_U, 0x00000017, "auipc t1, 100000" },
    { "jal", "rl", NULL, F_J, 0x0000006f, "jal t1, label" },
    { "jalr", "rri", NULL, F_JALR, 0x00000067, "jalr t1, t2, -100" },
    { "ecall", "", NULL, F_NONE, 0x00000073, "ecall" },
    { "ebreak", "", NULL, F_NONE, 0x00100073, "ebreak" },
    { "fence", "55", NULL, F_FENCE, 0x0000000f, "fence 1, 1" },
    { "fence.i", "", NULL, F_NONE, 0x0000100f, "fence.i" },

    { "nop", "", "addi x0, x0, 0", 0, 0, "nop" },
    { "not", "rr", "xori RG1, RG2, -1", 0, 0, "not t1, t2" },
    { "mv", "rr", "add RG1, x0, RG2", 0, 0, "mv t1, t2" },
    { "neg", "rr", "sub RG1, x0, RG2", 0, 0, "neg t1, t2" },
    { "sgt", "rrr", "slt RG1, RG3, RG2", 0, 0, "sgt t1, t2, t3" },
    { "sgtu", "rrr", "sltu RG1, RG3, RG2", 0, 0, "sgtu t1, t2, t3" },
    { "seqz", "rr", "sltiu RG1, RG2, 1", 0, 0, "seqz t1, t2" },
    { "snez", "rr", "sltu RG1, x0, RG2", 0, 0, "snez t1, t2" },
    { "sgtz", "rr", "slt RG1, x0, RG2", 0, 0, "sgtz t1, t2" },
    { "sltz", "rr", "slt RG1, RG2, x0", 0, 0, "sltz t1, t2" },
    { "b", "l", "jal x0, LAB", 0, 0, "b label" },
    { "beqz", "rl", "beq RG1, x0, LAB", 0, 0, "beqz t1, label" },
    { "bnez", "rl", "bne RG1, x0, LAB", 0, 0, "bnez t1, label" },
    { "bgez", "rl", "bge RG1, x0, LAB", 0, 0, "bgez t1, label" },
    { "bltz", "rl", "blt RG1, x0, LAB", 0, 0, "bltz t1, label" },
    { "bgtz", "rl", "blt x0, RG1, LAB", 0, 0, "bgtz t1, label" },
    { "blez", "rl", "bge x0, RG1, LAB", 0, 0, "blez t1, label" },
    { "bgt", "rrl", "blt RG2, RG1, LAB", 0, 0, "bgt t1, t2, label" },
    { "bgtu", "rrl", "bltu RG2, RG1, LAB", 0, 0, "bgtu t1, t2, label" },
    { "ble", "rrl", "bge RG2, RG1, LAB", 0, 0, "ble t1, t2, label" },
    { "bleu", "rrl", "bgeu RG2, RG1, LAB", 0, 0, "bleu t1, t2, label" },
    { "j", "l", "jal x0, LAB", 0, 0, "j label" },
    { "jal", "l", "jal x1, LAB", 0, 0, "jal label" },
    { "jr", "r", "jalr x0, RG1, 0", 0, 0, "jr t0" },
    { "jalr", "r", "jalr x1, RG1, 0", 0, 0, "jalr t0" },
    { "jr", "ri", "jalr x0, RG1, VL2", 0, 0, "jr t0, -100" },
    { "jalr", "ri", "jalr x1, RG1, VL2", 0, 0, "jalr t0, -100" },
    { "jalr", "ri(r)", "jalr RG1, RG4, VL2", 0, 0, "jalr t0, -100(t1)" },
    { "ret", "", "jalr x0, x1, 0", 0, 0, "ret" },
    { "call", "l", "auipc x6, PCH1; jalr x1, x6, PCL1", 0, 0, "call label" },
    { "tail", "l", "auipc x6, PCH1; jalr x0, x6, PCL1", 0, 0, "tail label" },
    { "li", "ri", "addi RG1, x0, VL2", 0, 0, "li t1, -100" },
    { "li", "rw", "lui RG1, VH2; addi RG1, RG1, VL2", 0, 0, "li t1, 100000" },
    { "la", "rl", "auipc RG1, PCH2; addi RG1, RG1, PCL2", 0, 0, "la t1, label" },
#define RV_LOAD_FORMS(op)                                                                   \
    { op, "r(r)", op " RG1, 0(RG3)", 0, 0, op " t1, (t2)" },                                \
    { op, "ri", op " RG1, VL2(x0)", 0, 0, op " t1, -100" },                                 \
    { op, "rw", "lui RG1, VH2; " op " RG1, VL2(RG1)", 0, 0, op " t1, 100000" },             \
    { op, "rl", "auipc RG1, PCH2; " op " RG1, PCL2(RG1)", 0, 0, op " t1, label" }
#define RV_STORE_FORMS(op)                                                                  \
    { op, "r(r)", op " RG1, 0(RG3)", 0, 0, op " t1, (t2)" },                                \
    { op, "ri", op " RG1, VL2(x0)", 0, 0, op " t1, -100" },                                 \
    { op, "rwr", "lui RG3, VH2; " op " RG1, VL2(RG3)", 0, 0, op " t1, 100000, t2" },        \
    { op, "rlr", "auipc RG3, PCH2; " op " RG1, PCL2(RG3)", 0, 0, op " t1, label, t2" }
    RV_LOAD_FORMS("lw"), RV_STORE_FORMS("sw"), RV_LOAD_FORMS("lh"), RV_STORE_FORMS("sh"),
    RV_LOAD_FORMS("lb"), RV_STORE_FORMS("sb"), RV_LOAD_FORMS("lhu"), RV_LOAD_FORMS("lbu"),
    { "zext.b", "rr", "andi RG1, RG2, 255", 0, 0, "zext.b t1, t2" },
    { "lui", "rH(l)", "lui RG1, LH4", 0, 0, "lui t1, %hi(label)" },
    { "addi", "rrL(l)", "addi RG1, RG2, LL5", 0, 0, "addi t1, t2, %lo(label)" },
#define RV_LO_FORM(op) { op, "rL(l)(r)", op " RG1, LL4(RG7)", 0, 0, op " t1, %lo(label)(t2)" }
    RV_LO_FORM("lb"), RV_LO_FORM("lh"), RV_LO_FORM("lw"), RV_LO_FORM("lbu"), RV_LO_FORM("lhu"),
    RV_LO_FORM("sb"), RV_LO_FORM("sh"), RV_LO_FORM("sw"),
};
enum { RV_FORM_COUNT = sizeof RV_FORMS / sizeof RV_FORMS[0] };

/* The directives RARS knows, by name; those marked unsupported are refused with a sentence. */
enum {
    D_DATA, D_TEXT, D_WORD, D_DWORD, D_ASCII, D_ASCIZ, D_STRING, D_BYTE, D_ALIGN, D_HALF, D_SPACE,
    D_DOUBLE, D_FLOAT, D_EXTERN, D_GLOBL, D_GLOBAL, D_EQV, D_MACRO, D_END_MACRO, D_INCLUDE, D_SECTION,
    D_BSS, D_SBSS, D_ZERO, D_COMM, D_LCOMM, D_P2ALIGN, D_BALIGN, D_2BYTE, D_4BYTE, D_8BYTE, D_COUNT
};
static const char *const RV_DIRECTIVES[D_COUNT] = {
    ".data", ".text", ".word", ".dword", ".ascii", ".asciz", ".string", ".byte", ".align", ".half",
    ".space", ".double", ".float", ".extern", ".globl", ".global", ".eqv", ".macro", ".end_macro",
    ".include", ".section", ".bss", ".sbss", ".zero", ".comm", ".lcomm", ".p2align", ".balign",
    ".2byte", ".4byte", ".8byte",
};
/* Directives a C compiler writes that place nothing: RARS passes over them in silence. */
static const char RV_METADATA_DIRECTIVES[] =
    ".file .ident .version .option .attribute .size .type .local .weak .hidden .protected .internal "
    ".addrsig .addrsig_sym";

/* ------------------------------------------------------------------ tokens */

enum {
    TK_NONE, TK_DIRECTIVE, TK_OPERATOR, TK_REG, TK_FREG, TK_CSR, TK_RMODE, TK_IDENT, TK_LPAREN,
    TK_RPAREN, TK_COLON, TK_PLUS, TK_MINUS, TK_INT, TK_REAL, TK_STRING, TK_HI, TK_LO, TK_TAG, TK_ERROR
};
/* How many bits an integer needs, in the order RARS classes it (TokenTypes.matchTokenType). */
enum { IC_5, IC_6, IC_12, IC_12U, IC_20, IC_32, IC_64 };

typedef struct {
    uint8_t type, iclass;
    int16_t reg;            /* TK_REG: its number; TK_DIRECTIVE: D_; TK_OPERATOR: 1 when supported */
    int start, length;      /* in the line */
    int32_t value;          /* TK_INT */
} RvTok;

/* An operand kept from the first pass to the second. */
typedef struct {
    uint8_t type, iclass;
    int16_t reg;
    int32_t value;          /* TK_INT: the number; TK_IDENT: the name's place in the arena */
} RvOperand;

enum { RV_MAX_TOKENS = 48, RV_MAX_OPERANDS = 9 };

typedef struct {
    int line;
    uint32_t address;
    int16_t form;
    uint8_t count;          /* operands */
    RvOperand operand[RV_MAX_OPERANDS];
} RvStatement;

/* A name in the arena, and what it stands for. */
typedef struct {
    int name, length;
    int value;
} RvEntry;

typedef struct {
    RvEntry *entries;
    int *slots;             /* entry index + 1, 0 when empty */
    int count, capacity, slot_count;
} RvTable;

typedef struct {
    uint32_t offset;        /* in the data image */
    int length, name, line;
} RvForward;

typedef struct {
    RvProgram *program;
    const char *source;
    size_t source_length;
    char *arena;
    int arena_size, arena_capacity;
    RvTable keywords;       /* registers, CSR and FP names, exact case: value = type << 16 | number */
    RvTable operators;      /* mnemonics and directives, lower case: value = type << 16 | number */
    RvTable symbols;        /* value = index into program->symbols */
    RvTable eqv;            /* value = place of the replacement text in the arena */
    int symbol_capacity;
    int *symbol_names;      /* each symbol's name in the arena, beside program->symbols */
    RvStatement *statements;
    int statement_count, statement_capacity;
    uint8_t *data;          /* the image, from RV_DATA_SEGMENT */
    uint32_t data_capacity, data_end;
    uint32_t data_low;      /* the lowest address placed, or 0 */
    uint32_t text_address, data_address;
    int in_data, auto_align, data_directive;
    RvForward *forwards;
    int forward_count, forward_capacity;
    int globals[256], global_lines[256], global_count;
    char *line;             /* the line being assembled, after .eqv */
    int line_capacity;
    int line_number;
    int out_of_memory;
    int screen_label, screen_label_length;
    int token_overflow, token_unterminated;
    int16_t form_next[RV_FORM_COUNT];   /* the next form of the same mnemonic, or -1 */
} RvAsm;

/* ------------------------------------------------------------------ memory and errors */

static int rv_grow(RvAsm *a, void **pointer, int *capacity, int need, size_t size) {
    if (need <= *capacity) return 1;
    int grown = *capacity ? *capacity : 16;
    while (grown < need) grown *= 2;
    void *bigger = RV_REALLOC(*pointer, (size_t)grown * size);
    if (!bigger) {
        a->out_of_memory = 1;
        return 0;
    }
    memset((char *)bigger + (size_t)*capacity * size, 0, (size_t)(grown - *capacity) * size);
    *pointer = bigger;
    *capacity = grown;
    return 1;
}

static void rv_message(RvError *list, int *count, int max, int line, const char *format, va_list args) {
    if (*count < max) {
        list[*count].line = line;
        vsnprintf(list[*count].text, RV_ERROR_TEXT, format, args);
    }
    (*count)++;
}

static void rv_error(RvAsm *a, int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rv_message(a->program->errors, &a->program->error_count, RV_MAX_ERRORS, line, format, args);
    va_end(args);
}

static void rv_warning(RvAsm *a, int line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    rv_message(a->program->warnings, &a->program->warning_count, RV_MAX_WARNINGS, line, format, args);
    va_end(args);
}

/* Copies text into the arena, with a 0 after it; answers its place, or -1. */
static int rv_intern(RvAsm *a, const char *text, int length) {
    if (!rv_grow(a, (void **)&a->arena, &a->arena_capacity, a->arena_size + length + 1, 1)) return -1;
    int at = a->arena_size;
    memcpy(a->arena + at, text, length);
    a->arena[at + length] = 0;
    a->arena_size += length + 1;
    return at;
}

/* ------------------------------------------------------------------ name tables */

static uint32_t rv_hash(const char *text, int length, int fold) {
    uint32_t h = 2166136261u;
    for (int i = 0; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (fold && c >= 'A' && c <= 'Z') c += 32;
        h = (h ^ c) * 16777619u;
    }
    return h;
}

static int rv_same(const char *a, const char *b, int length, int fold) {
    for (int i = 0; i < length; i++) {
        unsigned char x = (unsigned char)a[i], y = (unsigned char)b[i];
        if (fold) {
            if (x >= 'A' && x <= 'Z') x += 32;
            if (y >= 'A' && y <= 'Z') y += 32;
        }
        if (x != y) return 0;
    }
    return 1;
}

/* The entry of a name, or NULL. Names in a folded table are compared without case. */
static RvEntry *rv_find(RvAsm *a, RvTable *t, const char *text, int length, int fold) {
    if (!t->slot_count) return NULL;
    uint32_t mask = (uint32_t)t->slot_count - 1, at = rv_hash(text, length, fold) & mask;
    for (;;) {
        int index = t->slots[at];
        if (!index) return NULL;
        RvEntry *e = &t->entries[index - 1];
        if (e->length == length && rv_same(a->arena + e->name, text, length, fold)) return e;
        at = (at + 1) & mask;
    }
}

static RvEntry *rv_add(RvAsm *a, RvTable *t, const char *text, int length, int value, int fold) {
    if ((t->count + 1) * 2 > t->slot_count) {
        int slots = t->slot_count ? t->slot_count * 2 : 64;
        int *bigger = RV_ALLOC((size_t)slots * sizeof(int));
        if (!bigger) {
            a->out_of_memory = 1;
            return NULL;
        }
        for (int i = 0; i < t->count; i++) {
            RvEntry *e = &t->entries[i];
            uint32_t at = rv_hash(a->arena + e->name, e->length, fold) & (uint32_t)(slots - 1);
            while (bigger[at]) at = (at + 1) & (uint32_t)(slots - 1);
            bigger[at] = i + 1;
        }
        RV_FREE(t->slots);
        t->slots = bigger;
        t->slot_count = slots;
    }
    if (!rv_grow(a, (void **)&t->entries, &t->capacity, t->count + 1, sizeof(RvEntry))) return NULL;
    int name = rv_intern(a, text, length);
    if (name < 0) return NULL;
    RvEntry *e = &t->entries[t->count];
    e->name = name;
    e->length = length;
    e->value = value;
    uint32_t at = rv_hash(text, length, fold) & (uint32_t)(t->slot_count - 1);
    while (t->slots[at]) at = (at + 1) & (uint32_t)(t->slot_count - 1);
    t->slots[at] = ++t->count;
    return e;
}

static void rv_table_free(RvTable *t) {
    RV_FREE(t->entries);
    RV_FREE(t->slots);
    memset(t, 0, sizeof *t);
}

/* Adds each space-separated word of a list. */
static void rv_add_words(RvAsm *a, RvTable *t, const char *list, int value, int fold) {
    while (*list) {
        while (*list == ' ') list++;
        int length = 0;
        while (list[length] && list[length] != ' ') length++;
        if (length && !rv_find(a, t, list, length, fold)) rv_add(a, t, list, length, value, fold);
        list += length;
    }
}

static void rv_tables(RvAsm *a) {
    char name[8];
    for (int i = 0; i < 32; i++) {
        rv_add(a, &a->keywords, RV_ABI_NAMES[i], (int)strlen(RV_ABI_NAMES[i]), TK_REG << 16 | i, 0);
        int length = snprintf(name, sizeof name, "x%d", i);
        rv_add(a, &a->keywords, name, length, TK_REG << 16 | i, 0);
        length = snprintf(name, sizeof name, "f%d", i);
        rv_add(a, &a->keywords, name, length, TK_FREG << 16 | i, 0);
    }
    rv_add(a, &a->keywords, "fp", 2, TK_REG << 16 | 8, 0);
    static const char *const fp_prefixes[] = { "ft", "fs", "fa" };
    static const int fp_counts[] = { 12, 12, 8 };
    for (int p = 0; p < 3; p++)
        for (int i = 0; i < fp_counts[p]; i++) {
            int length = snprintf(name, sizeof name, "%s%d", fp_prefixes[p], i);
            rv_add(a, &a->keywords, name, length, TK_FREG << 16 | i, 0);
        }
    rv_add_words(a, &a->keywords, RV_CSR_NAMES, TK_CSR << 16, 0);
    rv_add_words(a, &a->keywords, RV_ROUNDING, TK_RMODE << 16, 0);
    /* a mnemonic this assembler takes stands for its first form; the forms of one mnemonic are
     * chained in table order */
    for (int i = RV_FORM_COUNT - 1; i >= 0; i--) {
        int length = (int)strlen(RV_FORMS[i].name);
        RvEntry *e = rv_find(a, &a->operators, RV_FORMS[i].name, length, 1);
        a->form_next[i] = e ? (int16_t)((e->value & 0xffff) - 1) : -1;
        if (e) e->value = TK_OPERATOR << 16 | (i + 1);
        else rv_add(a, &a->operators, RV_FORMS[i].name, length, TK_OPERATOR << 16 | (i + 1), 1);
    }
    rv_add_words(a, &a->operators, RV_RARS_MNEMONICS, TK_OPERATOR << 16, 1);
    for (int i = 0; i < D_COUNT; i++)
        rv_add(a, &a->operators, RV_DIRECTIVES[i], (int)strlen(RV_DIRECTIVES[i]), TK_DIRECTIVE << 16 | i, 1);
}

/* ------------------------------------------------------------------ numbers, as RARS reads them */

/* Binary.stringToIntFast: decimal, 0x hexadecimal, a leading 0 for octal, an optional minus;
 * the sums wrap as Java's int arithmetic does, and only a negative result is refused. */
static int rv_int_fast(const char *s, int n, int32_t *out) {
    if (n == 0) return 0;
    char first = s[0];
    if (!((first >= '0' && first <= '9') || first == '-')) return 0;
    uint32_t result = 0;
    int i = first == '-';
    if (i >= n) return 0;
    if (n > 2 + i && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        if (n > 10 + i) return 0;
        for (i += 2; i < n; i++) {
            char c = s[i];
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (d < 0) return 0;
            result = result * 16 + (uint32_t)d;
        }
    } else if (first == '0') {
        if (n > 12 + i) return 0;
        for (; i < n; i++) {
            if (s[i] < '0' || s[i] > '7') return 0;
            result = result * 8 + (uint32_t)(s[i] - '0');
        }
        if ((int32_t)result < 0) return 0;
    } else {
        if (n > 10 + i) return 0;
        for (; i < n; i++) {
            if (s[i] < '0' || s[i] > '9') return 0;
            result = result * 10 + (uint32_t)(s[i] - '0');
        }
        if ((int32_t)result < 0) return 0;
    }
    if (result == 0x80000000u && first == '-') {
        *out = (int32_t)result;
        return 1;
    }
    if ((int32_t)result < 0 && first == '-') return 0;
    if (first == '-') result = 0u - result;
    *out = (int32_t)result;
    return 1;
}

/* Long.decode, and RARS's 0x with sixteen digits: a sign, then 0x, # or a leading 0 for octal. */
static int rv_long(const char *s, int n, int64_t *out) {
    int i = 0, negative = 0, base = 10;
    if (i < n && (s[i] == '-' || s[i] == '+')) negative = s[i++] == '-';
    if (n - i >= 2 && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) base = 16, i += 2;
    else if (i < n && s[i] == '#') base = 16, i++;
    else if (n - i >= 2 && s[i] == '0') base = 8, i++;
    if (i >= n) return 0;
    uint64_t value = 0;
    int start = i;
    for (; i < n; i++) {
        char c = s[i];
        int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 99;
        if (d >= base) return 0;
        if (value > (UINT64_MAX - (uint64_t)d) / (uint64_t)base) return 0;
        value = value * (uint64_t)base + (uint64_t)d;
    }
    if (value <= (uint64_t)INT64_MAX || (negative && value == (uint64_t)INT64_MAX + 1)) {
        *out = negative ? (int64_t)(0 - value) : (int64_t)value;
        return 1;
    }
    /* "0x" and exactly sixteen digits: two's complement */
    if (!negative && base == 16 && n == 18 && start == 2) {
        *out = (int64_t)value;
        return 1;
    }
    return 0;
}

/* Answers 32 for a number that fits an int, 64 for one that needs a long, 0 for no number. */
static int rv_number(const char *s, int n, int64_t *value) {
    int32_t small;
    if (rv_int_fast(s, n, &small)) {
        *value = small;
        return 32;
    }
    int64_t big;
    if (!rv_long(s, n, &big)) return 0;
    *value = big;
    return big >= INT32_MIN && big <= INT32_MAX ? 32 : 64;
}

static int rv_class(int32_t v) {
    if (v >= 0 && v <= 31) return IC_5;
    if (v >= 0 && v <= 64) return IC_6;
    if (v >= -2048 && v <= 2047) return IC_12;
    if (v >= 0 && v <= 0xfff) return IC_12U;
    if (v >= 0 && v <= 0xfffff) return IC_20;
    return IC_32;
}

static int rv_ident_char(char c, int first) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '.' || c == '$' ||
           (!first && c >= '0' && c <= '9') || (unsigned char)c >= 0x80;
}

/* ------------------------------------------------------------------ the tokenizer */

/* Classes a token as TokenTypes.matchTokenType does. */
static void rv_classify(RvAsm *a, RvTok *t) {
    const char *s = a->line + t->start;
    int n = t->length;
    t->type = TK_ERROR;
    if (n == 1) {
        switch (s[0]) {
        case '(': t->type = TK_LPAREN; return;
        case ')': t->type = TK_RPAREN; return;
        case ':': t->type = TK_COLON; return;
        case '+': t->type = TK_PLUS; return;
        case '-': t->type = TK_MINUS; return;
        }
    }
    if (s[0] == '\'') return;
    if (s[0] == '@') {
        t->type = TK_TAG;   /* @function and the like, which only metadata directives take */
        return;
    }
    if (n == 3 && s[0] == '%' && s[1] == 'h' && s[2] == 'i') {
        t->type = TK_HI;
        return;
    }
    if (n == 3 && s[0] == '%' && s[1] == 'l' && s[2] == 'o') {
        t->type = TK_LO;
        return;
    }
    if (s[0] == '%') {
        t->type = TK_TAG;   /* a macro parameter, which only .macro takes */
        return;
    }
    RvEntry *e = rv_find(a, &a->keywords, s, n, 0);
    if (e) {
        t->type = (uint8_t)(e->value >> 16);
        t->reg = (int16_t)(e->value & 0xffff);
        return;
    }
    /* x and a number in another spelling, as RegisterBlock.getRegister takes it: x05 is x5 */
    if (n >= 2 && s[0] == 'x') {
        int32_t number;
        if (rv_int_fast(s + 1, n - 1, &number) && number >= 0 && number < 32) {
            t->type = TK_REG;
            t->reg = (int16_t)number;
            return;
        }
    }
    int numeric = (s[0] >= '0' && s[0] <= '9') || ((s[0] == '-' || s[0] == '+') && n > 1 && s[1] >= '0' && s[1] <= '9');
    if (numeric) {
        int64_t value;
        int kind = rv_number(s, n, &value);
        if (kind) {
            t->type = TK_INT;
            t->value = (int32_t)value;
            t->iclass = kind == 64 ? IC_64 : (uint8_t)rv_class((int32_t)value);
            return;
        }
        char text[64];
        if (n < (int)sizeof text) {
            memcpy(text, s, n);
            text[n] = 0;
            char *end;
            strtod(text, &end);
            if (*end == 0) {
                t->type = TK_REAL;
                return;
            }
        }
    } else if (s[0] == '.' && n > 1 && s[1] >= '0' && s[1] <= '9' && n < 64) {
        char text[64];
        memcpy(text, s, n);
        text[n] = 0;
        char *end;
        strtod(text, &end);
        if (*end == 0) {
            t->type = TK_REAL;
            return;
        }
    }
    if (s[0] == '"') {
        t->type = TK_STRING;
        return;
    }
    e = rv_find(a, &a->operators, s, n, 1);
    if (e) {
        t->type = (uint8_t)(e->value >> 16);
        t->reg = (int16_t)(e->value & 0xffff);
        return;
    }
    if (!rv_ident_char(s[0], 1)) return;
    for (int i = 1; i < n; i++)
        if (!rv_ident_char(s[i], 0)) return;
    t->type = TK_IDENT;
}

/* A character literal's number, as Tokenizer.preprocessCharacterLiteral reads it, or -1. */
static int rv_char_literal(const char *s, int n) {
    if (n < 3 || s[0] != '\'' || s[n - 1] != '\'') return -1;
    s++, n -= 2;
    if (s[0] != '\\') return n == 1 ? (unsigned char)s[0] : -1;
    if (n == 2) {
        static const char escaped[] = "'\"\\ntbrf0";
        static const int values[] = { 39, 34, 92, 10, 9, 8, 13, 12, 0 };
        const char *at = memchr(escaped, s[1], sizeof escaped - 1);
        return at ? values[at - escaped] : -1;
    }
    if (n == 4) {
        int value = 0;
        for (int i = 1; i < 4; i++) {
            if (s[i] < '0' || s[i] > '7') return -1;
            value = value * 8 + s[i] - '0';
        }
        return value <= 255 ? value : -1;
    }
    return -1;
}

/* Cuts a.line[0..length) into tokens, as Tokenizer.tokenizeLine does; answers how many. */
static int rv_tokenize(RvAsm *a, int length, RvTok *tokens) {
    const char *line = a->line;
    int count = 0, start = -1, quoted = 0;
    a->token_overflow = a->token_unterminated = 0;
#define RV_EMIT(from, to)                                                                    \
    do {                                                                                     \
        if (count < RV_MAX_TOKENS) {                                                         \
            RvTok *t_ = &tokens[count++];                                                    \
            memset(t_, 0, sizeof *t_);                                                       \
            t_->start = (from);                                                              \
            t_->length = (to) - (from);                                                      \
            rv_classify(a, t_);                                                              \
        } else {                                                                             \
            a->token_overflow = 1;                                                           \
        }                                                                                    \
    } while (0)
    for (int i = 0; i < length; i++) {
        char c = line[i];
        if (quoted) {
            if (c == '"' && line[i - 1] != '\\') {
                RV_EMIT(start, i + 1);
                start = -1;
                quoted = 0;
            }
            continue;
        }
        switch (c) {
        case '#':
            if (start >= 0) RV_EMIT(start, i);
            start = -1;
            i = length;
            break;
        case ' ': case '\t': case ',': case '\r':
            if (start >= 0) RV_EMIT(start, i);
            start = -1;
            break;
        case '+': case '-':
            /* the sign of an exponent, 1.2e-5, stays in its token */
            if (start >= 0 && i + 1 < length && line[i + 1] >= '0' && line[i + 1] <= '9' &&
                (line[i - 1] == 'e' || line[i - 1] == 'E'))
                break;
            if (start >= 0) RV_EMIT(start, i);
            start = i;
            if (!((count == 0 || tokens[count - 1].type != TK_IDENT) && i + 1 < length && line[i + 1] >= '0' &&
                  line[i + 1] <= '9')) {
                RV_EMIT(i, i + 1);
                start = -1;
            }
            break;
        case ':': case '(': case ')':
            if (start >= 0) RV_EMIT(start, i);
            RV_EMIT(i, i + 1);
            start = -1;
            break;
        case '"':
            if (start >= 0) RV_EMIT(start, i);
            start = i;
            quoted = 1;
            break;
        case '\'': {
            if (start >= 0) RV_EMIT(start, i);
            int from = i, ahead = length - i - 1;
            if (ahead >= 2) {
                i++;
                if (line[i] != '\'') {
                    i++;
                    if (!((line[i] == '\'' && line[i - 1] != '\\') || ahead == 2)) {
                        i++;
                        if (!(line[i] == '\'' || ahead == 3) && ahead >= 5) {
                            i++;
                            if (line[i] != '\'') i++;
                        }
                    }
                }
            }
            int value = rv_char_literal(line + from, i + 1 - from);
            if (count < RV_MAX_TOKENS) {
                RvTok *t = &tokens[count++];
                memset(t, 0, sizeof *t);
                t->start = from;
                t->length = i + 1 - from;
                if (value >= 0) {
                    t->type = TK_INT;
                    t->value = value;
                    t->iclass = (uint8_t)rv_class(value);
                } else {
                    t->type = TK_ERROR;
                }
            }
            start = -1;
            break;
        }
        default:
            if (start < 0) start = i;
            break;
        }
    }
    if (start >= 0) {
        a->token_unterminated = quoted;
        RV_EMIT(start, length);
    }
#undef RV_EMIT
    return count;
}

/* Reports what the last rv_tokenize found wrong: the tokens it could not class, and the line. */
static void rv_report_tokens(RvAsm *a, const RvTok *tokens, int count) {
    if (a->token_unterminated) rv_error(a, a->line_number, "This string has no closing quote");
    if (a->token_overflow) rv_error(a, a->line_number, "This line has too many parts");
    for (int i = 0; i < count; i++) {
        if (tokens[i].type != TK_ERROR) continue;
        const char *text = a->line + tokens[i].start;
        int length = tokens[i].length > 30 ? 30 : tokens[i].length;
        if (text[0] == '\'') rv_error(a, a->line_number, "%.*s is not one character in quotes", length, text);
        else rv_error(a, a->line_number, "'%.*s' is not something an instruction can use", length, text);
    }
}

/*
 * Tokenizes the current line and carries out .eqv, as Tokenizer.processEqv does: a .eqv line
 * defines its name, and on any other line an identifier with a definition is replaced by its
 * text, one at a time, the line cut into tokens again after each.
 */
static int rv_tokenize_line(RvAsm *a, int length, RvTok *tokens) {
    for (int round = 0;; round++) {
        int count = rv_tokenize(a, length, tokens);
        int at = count > 2 && tokens[0].type == TK_DIRECTIVE ? 0 : count > 2 && tokens[2].type == TK_DIRECTIVE ? 2 : -1;
        if (at >= 0 && tokens[at].reg == D_EQV) {
            rv_report_tokens(a, tokens, count);
            if (count < at + 3) {
                rv_error(a, a->line_number, ".eqv needs a name and what it stands for");
                return count;
            }
            if (tokens[at + 1].type != TK_IDENT) {
                rv_error(a, a->line_number, ".eqv needs a name first: .eqv NAME, value");
                return count;
            }
            const char *name = a->line + tokens[at + 1].start;
            int name_length = tokens[at + 1].length;
            for (int i = at + 2; i < count; i++)
                if (tokens[i].length == name_length && !memcmp(a->line + tokens[i].start, name, name_length)) {
                    rv_error(a, a->line_number, ".eqv cannot define %.*s with itself", name_length, name);
                    return count;
                }
            int from = tokens[at + 2].start, to = tokens[count - 1].start + tokens[count - 1].length;
            RvEntry *old = rv_find(a, &a->eqv, name, name_length, 0);
            if (old) {
                const char *text = a->arena + old->value;
                if ((int)strlen(text) != to - from || memcmp(text, a->line + from, to - from))
                    rv_error(a, a->line_number, "'%.*s' is already defined with .eqv", name_length, name);
                return count;
            }
            int text = rv_intern(a, a->line + from, to - from);
            if (text >= 0) rv_add(a, &a->eqv, name, name_length, text, 0);
            return count;
        }
        int replaced = 0;
        if (a->eqv.count && round < 64) {
            for (int i = 0; i < count; i++) {
                if (tokens[i].type != TK_IDENT) continue;
                RvEntry *e = rv_find(a, &a->eqv, a->line + tokens[i].start, tokens[i].length, 0);
                if (!e) continue;
                const char *text = a->arena + e->value;
                int text_length = (int)strlen(text), cut = tokens[i].length;
                int grown = length - cut + text_length;
                if (!rv_grow(a, (void **)&a->line, &a->line_capacity, grown + 1, 1)) return 0;
                memmove(a->line + tokens[i].start + text_length, a->line + tokens[i].start + cut,
                        length - tokens[i].start - cut);
                memcpy(a->line + tokens[i].start, text, text_length);
                length = grown;
                a->line[length] = 0;
                replaced = 1;
                break;
            }
        }
        if (!replaced) {
            rv_report_tokens(a, tokens, count);
            return count;
        }
    }
}

/* ------------------------------------------------------------------ symbols */

static RvSymbol *rv_symbol(RvAsm *a, const char *name, int length) {
    RvEntry *e = rv_find(a, &a->symbols, name, length, 0);
    return e ? &a->program->symbols[e->value] : NULL;
}

static void rv_define(RvAsm *a, const RvTok *t) {
    const char *name = a->line + t->start;
    if (rv_symbol(a, name, t->length)) {
        rv_error(a, a->line_number, "Label '%.*s' is already defined", t->length > 40 ? 40 : t->length, name);
        return;
    }
    RvProgram *p = a->program;
    if (!rv_grow(a, (void **)&p->symbols, &a->symbol_capacity, p->symbol_count + 1, sizeof(RvSymbol))) return;
    int names = a->symbol_capacity;
    (void)names;
    RvSymbol *s = &p->symbols[p->symbol_count];
    snprintf(s->name, sizeof s->name, "%.*s", t->length, name);
    s->address = a->in_data ? a->data_address : a->text_address;
    s->line = a->line_number;
    s->text = !a->in_data;
    rv_add(a, &a->symbols, name, t->length, p->symbol_count, 0);
    p->symbol_count++;
}

/* The address of a label named in the arena, or -1 with an error. */
static int64_t rv_resolve(RvAsm *a, int name, int line) {
    RvEntry *e = rv_find(a, &a->symbols, a->arena + name, (int)strlen(a->arena + name), 0);
    if (e) return a->program->symbols[e->value].address;
    rv_error(a, line, "No label named '%.40s'", a->arena + name);
    return -1;
}

/* ------------------------------------------------------------------ the data segment */

/* Moves the data address up to a boundary, and a label just defined at the old address with it,
 * as Assembler.alignToBoundary does. */
static void rv_align_data(RvAsm *a, uint32_t boundary) {
    if (boundary <= 1) return;
    uint32_t old = a->data_address, rest = old % boundary;
    if (!rest) return;
    a->data_address = old + boundary - rest;
    RvProgram *p = a->program;
    for (int i = p->symbol_count - 1; i >= 0; i--)
        if (!p->symbols[i].text && p->symbols[i].address == old) p->symbols[i].address = a->data_address;
}

/* Puts bytes at the data address and moves past them; answers their address, or 0. */
static uint32_t rv_place(RvAsm *a, const uint8_t *bytes, uint32_t length, int reserve) {
    uint32_t at = a->data_address;
    if (at < RV_DATA_SEGMENT || at >= RV_DATA_LIMIT || length > RV_DATA_LIMIT - at) {
        rv_error(a, a->line_number, "0x%08x is outside the data segment (0x%08x to 0x%08x)", at, RV_DATA_SEGMENT,
                 RV_DATA_LIMIT - 1);
        return 0;
    }
    uint32_t end = at + length - RV_DATA_SEGMENT;
    if (!reserve || bytes) {
        if (end > a->data_capacity) {
            uint32_t capacity = a->data_capacity ? a->data_capacity : 4096;
            while (capacity < end) capacity *= 2;
            uint8_t *bigger = RV_REALLOC(a->data, capacity);
            if (!bigger) {
                a->out_of_memory = 1;
                return 0;
            }
            memset(bigger + a->data_capacity, 0, capacity - a->data_capacity);
            a->data = bigger;
            a->data_capacity = capacity;
        }
        if (bytes) memcpy(a->data + (at - RV_DATA_SEGMENT), bytes, length);
    }
    if (end > a->data_end) a->data_end = end;
    if (length && (!a->data_low || at < a->data_low)) a->data_low = at;
    a->data_address = at + length;
    return at;
}

static uint32_t rv_place_value(RvAsm *a, uint32_t value, int size) {
    uint8_t bytes[4] = { (uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24) };
    if (a->auto_align) rv_align_data(a, (uint32_t)size);
    return rv_place(a, bytes, (uint32_t)size, 0);
}

static int rv_size_of(int directive) {
    return directive == D_BYTE ? 1 : directive == D_HALF ? 2 : 4;
}

/* One value of .word, .half or .byte: a number, truncated as RARS does with a warning, or a label,
 * whose address may be filled in once the label is defined. */
static void rv_store_integer(RvAsm *a, const RvTok *t, int directive) {
    int size = rv_size_of(directive);
    if (t->type == TK_INT) {
        int32_t value = t->value;
        if (directive == D_DWORD) {
            /* a long needs two words; this assembler keeps 32 bits of each number */
            rv_place_value(a, (uint32_t)value, 4);
            rv_place_value(a, value < 0 ? 0xffffffffu : 0, 4);
            return;
        }
        if (t->iclass == IC_64)
            rv_warning(a, a->line_number, "%.*s does not fit in 32 bits and is cut to its low bits", t->length, a->line + t->start);
        else if ((directive == D_BYTE && (value < -128 || value > 255)) || (directive == D_HALF && (value < -32768 || value > 65535)))
            rv_warning(a, a->line_number, "%d does not fit in a %s and is cut to its low bits", value,
                       directive == D_BYTE ? "byte" : "halfword");
        rv_place_value(a, (uint32_t)value, size);
        return;
    }
    if (t->type == TK_IDENT) {
        RvSymbol *s = rv_symbol(a, a->line + t->start, t->length);
        if (s) {
            rv_place_value(a, s->address, size == 8 ? 4 : size);
            return;
        }
        uint32_t at = rv_place_value(a, 0, size == 8 ? 4 : size);
        if (!at) return;
        if (!rv_grow(a, (void **)&a->forwards, &a->forward_capacity, a->forward_count + 1, sizeof(RvForward))) return;
        RvForward *f = &a->forwards[a->forward_count++];
        f->offset = at - RV_DATA_SEGMENT;
        f->length = size;
        f->name = rv_intern(a, a->line + t->start, t->length);
        f->line = a->line_number;
        return;
    }
    rv_error(a, a->line_number, "'%.*s' is not a number or a label", t->length > 40 ? 40 : t->length, a->line + t->start);
}

/* .word, .half, .byte and .dword: a list, or "value : count". */
static void rv_store_numeric(RvAsm *a, const RvTok *t, int count, int directive) {
    int first = t[0].type == TK_DIRECTIVE;
    if (count == 4 && t[2].type == TK_COLON) {
        if (t[1].type != TK_INT || t[3].type != TK_INT) {
            rv_error(a, a->line_number, "Write a repeated value as value : count, with two numbers");
            return;
        }
        if (t[3].value <= 0) {
            rv_error(a, a->line_number, "The repeat count must be positive");
            return;
        }
        if (!a->in_data) return;
        if (a->auto_align) rv_align_data(a, (uint32_t)rv_size_of(directive));
        for (int32_t i = 0; i < t[3].value && !a->out_of_memory && a->program->error_count < RV_MAX_ERRORS; i++)
            rv_store_integer(a, &t[1], directive);
        return;
    }
    for (int i = first; i < count; i++) rv_store_integer(a, &t[i], directive);
}

/* The bytes of a string literal, RARS's escapes decoded, as UTF-8; answers how many. */
static int rv_unescape(RvAsm *a, const char *s, int n, uint8_t *out) {
    int length = 0;
    for (int j = 1; j < n - 1; j++) {
        uint32_t c = (unsigned char)s[j];
        if (c == '\\' && j + 1 < n - 1 + 1) {
            c = (unsigned char)s[++j];
            switch (c) {
            case 'n': c = '\n'; break;
            case 't': c = '\t'; break;
            case 'r': c = '\r'; break;
            case 'b': c = '\b'; break;
            case 'f': c = '\f'; break;
            case '0': c = 0; break;
            case 'u': {
                if (j + 5 > n - 1) {
                    rv_error(a, a->line_number, "\\u needs four hexadecimal digits");
                    j = n;
                    continue;
                }
                uint32_t unit = 0;
                for (int k = 1; k <= 4; k++) {
                    char h = s[j + k];
                    int d = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1;
                    if (d < 0) {
                        rv_error(a, a->line_number, "\\u%.4s is not a character", s + j + 1);
                        unit = '?';
                        break;
                    }
                    unit = unit * 16 + (uint32_t)d;
                }
                j += 4;
                /* a surrogate pair names one character */
                if (unit >= 0xd800 && unit < 0xdc00 && j + 6 < n - 1 + 1 && s[j + 1] == '\\' && s[j + 2] == 'u') {
                    uint32_t low = 0;
                    int ok = 1;
                    for (int k = 3; k <= 6; k++) {
                        char h = s[j + k];
                        int d = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1;
                        if (d < 0) ok = 0;
                        low = low * 16 + (uint32_t)(d < 0 ? 0 : d);
                    }
                    if (ok && low >= 0xdc00 && low < 0xe000) {
                        unit = 0x10000 + ((unit - 0xd800) << 10) + (low - 0xdc00);
                        j += 6;
                    }
                }
                if (unit >= 0xd800 && unit < 0xe000) unit = '?';
                if (unit < 0x80) out[length++] = (uint8_t)unit;
                else if (unit < 0x800) {
                    out[length++] = (uint8_t)(0xc0 | unit >> 6);
                    out[length++] = (uint8_t)(0x80 | (unit & 63));
                } else if (unit < 0x10000) {
                    out[length++] = (uint8_t)(0xe0 | unit >> 12);
                    out[length++] = (uint8_t)(0x80 | ((unit >> 6) & 63));
                    out[length++] = (uint8_t)(0x80 | (unit & 63));
                } else {
                    out[length++] = (uint8_t)(0xf0 | unit >> 18);
                    out[length++] = (uint8_t)(0x80 | ((unit >> 12) & 63));
                    out[length++] = (uint8_t)(0x80 | ((unit >> 6) & 63));
                    out[length++] = (uint8_t)(0x80 | (unit & 63));
                }
                continue;
            }
            default: break;   /* \\ \' \" and any other: the character itself */
            }
        }
        out[length++] = (uint8_t)c;
    }
    return length;
}

static void rv_store_strings(RvAsm *a, const RvTok *t, int count, int directive) {
    for (int i = t[0].type == TK_DIRECTIVE; i < count; i++) {
        if (t[i].type != TK_STRING) {
            rv_error(a, a->line_number, "'%.*s' is not a string in quotes", t[i].length > 40 ? 40 : t[i].length, a->line + t[i].start);
            continue;
        }
        uint8_t *bytes = RV_ALLOC((size_t)t[i].length * 2 + 4);
        if (!bytes) {
            a->out_of_memory = 1;
            return;
        }
        int length = rv_unescape(a, a->line + t[i].start, t[i].length, bytes);
        if (directive != D_ASCII) bytes[length++] = 0;
        rv_place(a, bytes, (uint32_t)length, 0);
        RV_FREE(bytes);
    }
}

static int rv_is_int(const RvTok *t) { return t->type == TK_INT; }

static void rv_directive(RvAsm *a, RvTok *t, int count) {
    int d = t[0].reg, line = a->line_number;
    const char *name = RV_DIRECTIVES[d];
    switch (d) {
    case D_EQV: return;
    case D_DATA: case D_BSS: case D_SBSS:
        a->in_data = 1;
        a->auto_align = 1;
        if (count > 1 && rv_is_int(&t[1])) a->data_address = (uint32_t)t[1].value;
        return;
    case D_TEXT:
        a->in_data = 0;
        if (count > 1 && rv_is_int(&t[1]) && (uint32_t)t[1].value != a->text_address)
            rv_error(a, line, ".text with an address is not supported here: the text starts at 0x%08x", RV_TEXT_BASE);
        return;
    case D_SECTION:
        /* a known directive's name is not a section name to RARS: .section .text is refused */
        if (count >= 2 && (t[1].type == TK_STRING || t[1].type == TK_IDENT)) {
            const char *s = a->line + t[1].start + (t[1].type == TK_STRING);
            if (!strncmp(s, ".data", 5) || !strncmp(s, ".rodata", 7) || !strncmp(s, ".sdata", 6) || !strncmp(s, ".bss", 4) ||
                !strncmp(s, ".sbss", 5))
                a->in_data = 1;
            else if (!strncmp(s, ".text", 5))
                a->in_data = 0;
            else if (strncmp(s, ".note", 5))
                rv_warning(a, line, "Section %.*s is ignored", t[1].length, a->line + t[1].start);
        } else if (count >= 2) {
            rv_error(a, line, ".section needs a section name");
        }
        return;
    case D_WORD: case D_4BYTE: case D_HALF: case D_2BYTE: case D_BYTE: case D_DWORD: case D_8BYTE: {
        int directive = d == D_4BYTE ? D_WORD : d == D_2BYTE ? D_HALF : d == D_8BYTE ? D_DWORD : d;
        a->data_directive = directive;
        if (!a->in_data) {
            rv_error(a, line, "%s belongs in .data, not in .text", name);
            return;
        }
        if (count > 1) rv_store_numeric(a, t, count, directive);
        return;
    }
    case D_ASCII: case D_ASCIZ: case D_STRING:
        a->data_directive = d;
        if (!a->in_data) {
            rv_error(a, line, "%s belongs in .data, not in .text", name);
            return;
        }
        rv_store_strings(a, t, count, d);
        return;
    case D_ALIGN: case D_P2ALIGN: case D_BALIGN: {
        if (count != 2) {
            rv_error(a, line, "%s needs one number", name);
            return;
        }
        if (!rv_is_int(&t[1]) || t[1].value < (d == D_BALIGN)) {
            rv_error(a, line, "%s needs a %s number", name, d == D_BALIGN ? "positive" : "non-negative");
            return;
        }
        int32_t value = t[1].value;
        if (d == D_BALIGN) rv_align_data(a, (uint32_t)value);
        else if (value < 2 && !a->in_data) rv_align_data(a, 4);
        else if (value == 0) a->auto_align = 0;
        else rv_align_data(a, value >= 31 ? 0x80000000u : 1u << value);
        return;
    }
    case D_SPACE: case D_ZERO:
        if (!a->in_data) {
            rv_error(a, line, "%s belongs in .data, not in .text", name);
            return;
        }
        if (count != 2) {
            rv_error(a, line, "%s needs one number: how many bytes", name);
            return;
        }
        if (!rv_is_int(&t[1]) || t[1].value < 0) {
            rv_error(a, line, "%s needs a non-negative number", name);
            return;
        }
        if (t[1].value > 0) rv_place(a, NULL, (uint32_t)t[1].value, 1);
        return;
    case D_GLOBL: case D_GLOBAL:
        if (count < 2) {
            rv_error(a, line, "%s needs a label", name);
            return;
        }
        for (int i = 1; i < count; i++) {
            if (t[i].type != TK_IDENT) {
                rv_error(a, line, "%s takes labels", name);
                return;
            }
            if (a->global_count < 256) {
                a->global_lines[a->global_count] = line;
                a->globals[a->global_count++] = rv_intern(a, a->line + t[i].start, t[i].length);
            }
        }
        return;
    default:
        rv_error(a, line, "%s is not supported here", name);
        return;
    }
}

/* ------------------------------------------------------------------ instructions */

/* Whether a token can be an operand of kind `kind`; *why says what is wrong when not:
 * 1 the wrong kind, 2 out of range, 3 too large for a signed immediate. */
static int rv_fits(char kind, const RvTok *t, int *why) {
    *why = 1;
    switch (kind) {
    case 'r': return t->type == TK_REG;
    case 'l': return t->type == TK_IDENT || t->type == TK_OPERATOR;
    case '(': return t->type == TK_LPAREN;
    case ')': return t->type == TK_RPAREN;
    case 'H': return t->type == TK_HI;
    case 'L': return t->type == TK_LO;
    }
    if (t->type != TK_INT || t->iclass == IC_64) return 0;
    int c = t->iclass;
    *why = 2;
    switch (kind) {
    case '5': return c == IC_5;
    case 'i':
        if (c == IC_12U) *why = 3;
        return c == IC_5 || c == IC_6 || c == IC_12;
    case 'u': return c != IC_32;
    case 'w': return 1;
    }
    return 0;
}

static int rv_form_fits(const RvForm *f, const RvTok *t, int count) {
    int n = (int)strlen(f->pattern), why;
    if (n != count) return 0;
    for (int i = 0; i < n; i++)
        if (!rv_fits(f->pattern[i], &t[i], &why)) return 0;
    return 1;
}

static int rv_form_words(const RvForm *f) {
    if (!f->expand) return 1;
    int words = 1;
    for (const char *s = f->expand; *s; s++) words += *s == ';';
    return words;
}

/* Says why no form of the instruction fits its operands, as OperandFormat does. */
static void rv_operand_error(RvAsm *a, int first, int last, const RvTok *t, int count) {
    (void)last;
    const RvForm *f = &RV_FORMS[first];
    for (int i = first; i >= 0; i = a->form_next[i])
        if ((int)strlen(RV_FORMS[i].pattern) == count) {
            f = &RV_FORMS[i];
            break;
        }
    int n = (int)strlen(f->pattern), line = a->line_number;
    if (n != count) {
        rv_error(a, line, "Too %s operands for %s. Write it as: %s", count < n ? "few" : "many", f->name, f->example);
        return;
    }
    for (int i = 0; i < n; i++) {
        int why;
        if (rv_fits(f->pattern[i], &t[i], &why)) continue;
        const char *text = a->line + t[i].start;
        int length = t[i].length > 30 ? 30 : t[i].length;
        if (why == 3)
            rv_error(a, line, "%.*s is too large for a signed 12-bit immediate (-2048 to 2047)", length, text);
        else if (why == 2)
            rv_error(a, line, "%.*s is out of range: %s needs %s", length, text, f->name,
                     f->pattern[i] == '5' ? "0 to 31" : f->pattern[i] == 'i' ? "-2048 to 2047" : "0 to 0xfffff");
        else if (t[i].type == TK_FREG)
            rv_error(a, line, "%.*s is a floating-point register; RV32IM has none", length, text);
        else
            rv_error(a, line, "%.*s is not right here. Write it as: %s", length, text, f->example);
        return;
    }
}

static void rv_instruction(RvAsm *a, RvTok *t, int count) {
    const char *name = a->line + t[0].start;
    int length = t[0].length, line = a->line_number;
    if (!t[0].reg) {
        rv_error(a, line, "%.*s is not supported here: the Assembly Editor runs RV32IM, without floating point, CSRs, "
                 "atomics or bit manipulation", length > 20 ? 20 : length, name);
        return;
    }
    int first = t[0].reg - 1, last = first, chosen = -1;
    for (int i = first; i >= 0; i = a->form_next[i]) {
        last = i;
        if (rv_form_fits(&RV_FORMS[i], t + 1, count - 1)) {
            chosen = i;
            break;
        }
    }
    if (chosen < 0) {
        rv_operand_error(a, first, last, t + 1, count - 1);
        return;
    }
    if (count - 1 > RV_MAX_OPERANDS) return;
    RvStatement *s;
    if (!rv_grow(a, (void **)&a->statements, &a->statement_capacity, a->statement_count + 1, sizeof(RvStatement))) return;
    s = &a->statements[a->statement_count++];
    s->line = line;
    s->address = a->text_address;
    s->form = (int16_t)chosen;
    s->count = (uint8_t)(count - 1);
    for (int i = 1; i < count; i++) {
        RvOperand *o = &s->operand[i - 1];
        o->type = t[i].type == TK_OPERATOR ? TK_IDENT : t[i].type;
        o->iclass = t[i].iclass;
        o->reg = t[i].reg;
        o->value = t[i].value;
        if (o->type == TK_IDENT) o->value = rv_intern(a, a->line + t[i].start, t[i].length);
    }
    a->text_address += 4u * (uint32_t)rv_form_words(&RV_FORMS[chosen]);
}

/* ------------------------------------------------------------------ the first pass */

static int rv_is_metadata(const char *name, int length) {
    if (length >= 5 && rv_same(name, ".cfi_", 5, 1)) return 1;
    const char *list = RV_METADATA_DIRECTIVES;
    while (*list) {
        while (*list == ' ') list++;
        int n = 0;
        while (list[n] && list[n] != ' ') n++;
        if (n == length && rv_same(list, name, n, 1)) return 1;
        list += n;
    }
    return 0;
}

static void rv_line(RvAsm *a, int length) {
    RvTok tokens[RV_MAX_TOKENS];
    int count = rv_tokenize_line(a, length, tokens);
    RvTok *t = tokens;
    /* one label, as RARS takes one; an instruction's name may be a label too */
    if (count >= 2 && (t[0].type == TK_IDENT || t[0].type == TK_OPERATOR) && t[1].type == TK_COLON) {
        rv_define(a, &t[0]);
        t += 2;
        count -= 2;
    }
    if (count <= 0) return;
    switch (t[0].type) {
    case TK_DIRECTIVE:
        rv_directive(a, t, count);
        return;
    case TK_IDENT:
        if (a->line[t[0].start] == '.') {
            if (!rv_is_metadata(a->line + t[0].start, t[0].length))
                rv_warning(a, a->line_number, "RARS does not know the %.*s directive, so it is ignored", t[0].length,
                           a->line + t[0].start);
            return;
        }
        break;
    }
    if (a->in_data) {
        int type = t[0].type;
        if (type == TK_PLUS || type == TK_MINUS || type == TK_STRING || type == TK_IDENT || type == TK_INT ||
            type == TK_REAL) {
            /* the list of the last data directive goes on */
            int d = a->data_directive;
            if (d == D_ASCII || d == D_ASCIZ || d == D_STRING) rv_store_strings(a, t, count, d);
            else if (d == D_WORD || d == D_HALF || d == D_BYTE || d == D_DWORD) rv_store_numeric(a, t, count, d);
            else if (d == D_FLOAT || d == D_DOUBLE) rv_error(a, a->line_number, "%s is not supported here", RV_DIRECTIVES[d]);
            return;
        }
        if (type == TK_OPERATOR)
            rv_warning(a, a->line_number, "An instruction in .data is ignored, as RARS ignores it: put .text before it");
        return;
    }
    if (t[0].type != TK_OPERATOR) {
        rv_error(a, a->line_number, "'%.*s' is not an instruction", t[0].length > 30 ? 30 : t[0].length, a->line + t[0].start);
        return;
    }
    rv_instruction(a, t, count);
}

/* ------------------------------------------------------------------ the second pass */

static void rv_emit(RvAsm *a, uint32_t word, int line) {
    RvProgram *p = a->program;
    p->text[p->text_words] = word;
    p->text_line[p->text_words] = line;
    p->text_words++;
}

/* Encodes a basic instruction from its fields, in the order its syntax writes them; branches and
 * jumps take a distance. Answers 0 when a distance is out of reach. */
static int rv_encode(RvAsm *a, int format, uint32_t bits, const int32_t *v, int line, uint32_t *word) {
    uint32_t w = bits;
    switch (format) {
    case F_R: w |= (uint32_t)(v[0] & 31) << 7 | (uint32_t)(v[1] & 31) << 15 | (uint32_t)(v[2] & 31) << 20; break;
    case F_I: case F_JALR: w |= (uint32_t)(v[0] & 31) << 7 | (uint32_t)(v[1] & 31) << 15 | ((uint32_t)v[2] & 0xfff) << 20; break;
    case F_SH: w |= (uint32_t)(v[0] & 31) << 7 | (uint32_t)(v[1] & 31) << 15 | ((uint32_t)v[2] & 31) << 20; break;
    case F_LOAD: w |= (uint32_t)(v[0] & 31) << 7 | ((uint32_t)v[1] & 0xfff) << 20 | (uint32_t)(v[2] & 31) << 15; break;
    case F_S: {
        uint32_t imm = (uint32_t)v[1] & 0xfff;
        w |= (uint32_t)(v[0] & 31) << 20 | (uint32_t)(v[2] & 31) << 15 | (imm & 31) << 7 | (imm >> 5) << 25;
        break;
    }
    case F_B: {
        int32_t d = v[2];
        if (d >= 4096 || d < -4096) {
            rv_error(a, line, "The branch target is too far away (more than 4 KiB): use a jump");
            return 0;
        }
        uint32_t imm = (uint32_t)d;
        w |= (uint32_t)(v[0] & 31) << 15 | (uint32_t)(v[1] & 31) << 20 | ((imm >> 12) & 1) << 31 | ((imm >> 5) & 63) << 25 |
             ((imm >> 1) & 15) << 8 | ((imm >> 11) & 1) << 7;
        break;
    }
    case F_U: w |= (uint32_t)(v[0] & 31) << 7 | ((uint32_t)v[1] & 0xfffff) << 12; break;
    case F_J: {
        int32_t d = v[1];
        if (d >= (1 << 20) || d < -(1 << 20)) {
            rv_error(a, line, "The jump target is too far away (more than 1 MiB)");
            return 0;
        }
        uint32_t imm = (uint32_t)d;
        w |= (uint32_t)(v[0] & 31) << 7 | ((imm >> 20) & 1) << 31 | ((imm >> 1) & 0x3ff) << 21 | ((imm >> 11) & 1) << 20 |
             ((imm >> 12) & 0xff) << 12;
        break;
    }
    case F_FENCE: w |= ((uint32_t)v[0] & 15) << 24 | ((uint32_t)v[1] & 15) << 20; break;
    case F_NONE: break;
    }
    *word = w;
    return 1;
}

/* The basic instruction of a template's mnemonic: the first form of its name, as basic forms come
 * first in RV_FORMS. */
static const RvForm *rv_basic(RvAsm *a, const char *name, int length) {
    RvEntry *e = rv_find(a, &a->operators, name, length, 1);
    int form = e ? (e->value & 0xffff) - 1 : -1;
    return form >= 0 && !RV_FORMS[form].expand ? &RV_FORMS[form] : NULL;
}

/* An operand's number: a register's, an integer, or a label's address (-1 when undefined). */
static int rv_operand_values(RvAsm *a, const RvStatement *s, int64_t *values) {
    int ok = 1;
    for (int i = 0; i < s->count; i++) {
        const RvOperand *o = &s->operand[i];
        values[i] = o->type == TK_REG ? o->reg : o->type == TK_INT ? o->value : 0;
        if (o->type == TK_IDENT) {
            values[i] = rv_resolve(a, o->value, s->line);
            if (values[i] < 0) ok = 0;
        }
    }
    return ok;
}

static int32_t rv_low12(int32_t v) { return (int32_t)((uint32_t)v << 20) >> 20; }
static int32_t rv_high20(int32_t v) { return (v >> 12) + ((v >> 11) & 1); }

/* Fills in RARS's templates of a pseudo-instruction and encodes what they say. */
static void rv_expand(RvAsm *a, const RvStatement *s, const RvForm *f, const int64_t *values) {
    const char *t = f->expand;
    uint32_t pc = s->address;
    int last_label = -1;
    for (int i = s->count - 1; i >= 0; i--)
        if (s->operand[i].type == TK_IDENT) {
            last_label = i;
            break;
        }
    while (*t) {
        while (*t == ' ' || *t == ';') t++;
        const char *name = t;
        while (*t && *t != ' ') t++;
        const RvForm *basic = rv_basic(a, name, (int)(t - name));
        int32_t v[4] = { 0, 0, 0, 0 };
        int n = 0;
        while (*t && *t != ';') {
            while (*t == ' ' || *t == ',' || *t == '(' || *t == ')') t++;
            if (!*t || *t == ';') break;
            const char *field = t;
            while (*t && *t != ',' && *t != '(' && *t != ')' && *t != ';' && *t != ' ') t++;
            int length = (int)(t - field);
            int32_t value = 0;
            int operand = field[length - 1] - '0' - 1;    /* RGn and the rest name token n */
            uint32_t here = pc + 4u * (uint32_t)(a->program->text_words - (int)((s->address - RV_TEXT_BASE) / 4));
            if (field[0] == 'x') value = atoi(field + 1);
            else if (!strncmp(field, "RG", 2)) value = (int32_t)values[operand];
            else if (!strncmp(field, "VL", 2)) value = rv_low12((int32_t)values[operand]);
            else if (!strncmp(field, "VH", 2)) value = rv_high20((int32_t)values[operand]);
            else if (!strncmp(field, "PCL", 3)) value = rv_low12((int32_t)((uint32_t)values[operand] - pc));
            else if (!strncmp(field, "PCH", 3)) value = rv_high20((int32_t)((uint32_t)values[operand] - pc));
            else if (!strncmp(field, "LL", 2)) value = rv_low12((int32_t)values[operand]);
            else if (!strncmp(field, "LH", 2)) value = rv_high20((int32_t)values[operand]);
            else if (!strncmp(field, "LAB", 3)) value = (int32_t)((uint32_t)values[last_label] - here);
            else value = atoi(field);
            if (n < 4) v[n++] = value;
        }
        uint32_t word = 0;
        if (basic) rv_encode(a, basic->format, basic->bits, v, s->line, &word);
        rv_emit(a, word, s->line);
    }
}

static void rv_second_pass(RvAsm *a) {
    RvProgram *p = a->program;
    int words = (int)((a->text_address - RV_TEXT_BASE) / 4);
    p->text = RV_ALLOC((size_t)(words ? words : 1) * sizeof(uint32_t));
    p->text_line = RV_ALLOC((size_t)(words ? words : 1) * sizeof(int));
    if (!p->text || !p->text_line) {
        a->out_of_memory = 1;
        return;
    }
    for (int i = 0; i < a->statement_count; i++) {
        const RvStatement *s = &a->statements[i];
        const RvForm *f = &RV_FORMS[s->form];
        int64_t values[RV_MAX_OPERANDS];
        int start = p->text_words, size = rv_form_words(f);
        if (rv_operand_values(a, s, values)) {
            if (f->expand) {
                rv_expand(a, s, f, values);
            } else {
                int32_t v[4] = { 0, 0, 0, 0 };
                int n = 0;
                for (int k = 0; k < s->count && n < 4; k++)
                    if (s->operand[k].type != TK_LPAREN && s->operand[k].type != TK_RPAREN) v[n++] = (int32_t)values[k];
                if (f->format == F_B) v[2] = (int32_t)((uint32_t)v[2] - s->address);
                if (f->format == F_J) v[1] = (int32_t)((uint32_t)v[1] - s->address);
                uint32_t word = 0;
                rv_encode(a, f->format, f->bits, v, s->line, &word);
                rv_emit(a, word, s->line);
            }
        }
        while (p->text_words < start + size) rv_emit(a, 0, s->line);
    }
}

/* ------------------------------------------------------------------ the @screen comment */

static int rv_clamp(int64_t v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : (int)v; }

static void rv_screen(RvAsm *a) {
    RvScreen *screen = &a->program->screen;
    screen->width = 512;
    screen->height = 256;
    screen->unit_w = screen->unit_h = 1;
    screen->base = RV_DATA_BASE;
    const char *s = a->source, *end = a->source + a->source_length;
    for (int line = 1; s < end; line++) {
        const char *eol = memchr(s, '\n', (size_t)(end - s));
        if (!eol) eol = end;
        for (const char *c = s; c < eol; c++) {
            if (*c != '#') continue;
            const char *k = c;
            while (k < eol && *k == '#') k++;
            while (k < eol && (*k == ' ' || *k == '\t')) k++;
            if (eol - k < 7 || !rv_same(k, "@screen", 7, 1) || (k + 7 < eol && rv_ident_char(k[7], 0) && k[7] != '.' && k[7] != '$'))
                continue;
            screen->present = 1;
            screen->line = line;
            k += 7;
            while (k < eol) {
                while (k < eol && (*k == ' ' || *k == '\t' || *k == ',' || *k == '\r')) k++;
                const char *key = k;
                while (k < eol && ((*k >= 'a' && *k <= 'z') || (*k >= 'A' && *k <= 'Z') || (*k >= '0' && *k <= '9') || *k == '_' || *k == '-')) k++;
                int key_length = (int)(k - key);
                while (k < eol && (*k == ' ' || *k == '\t')) k++;
                if (!key_length || k >= eol || *k != '=') break;
                k++;
                while (k < eol && (*k == ' ' || *k == '\t')) k++;
                const char *value = k;
                while (k < eol && *k != ' ' && *k != '\t' && *k != ',' && *k != '\r') k++;
                int value_length = (int)(k - value);
                char folded[24];
                int f = 0;
                for (int i = 0; i < key_length && f < 23; i++)
                    if (key[i] != '-' && key[i] != '_') folded[f++] = (char)(key[i] >= 'A' && key[i] <= 'Z' ? key[i] + 32 : key[i]);
                folded[f] = 0;
                int64_t number = -1;
                int is_number = 0;
                if (value_length > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
                    is_number = 1;
                    number = 0;
                    for (int i = 2; i < value_length; i++) {
                        char h = value[i];
                        int d = h >= '0' && h <= '9' ? h - '0' : h >= 'a' && h <= 'f' ? h - 'a' + 10 : h >= 'A' && h <= 'F' ? h - 'A' + 10 : -1;
                        if (d < 0 || number > 0xffffffffLL) is_number = 0;
                        else number = number * 16 + d;
                    }
                } else if (value_length > 0) {
                    is_number = 1;
                    number = 0;
                    for (int i = 0; i < value_length; i++) {
                        if (value[i] < '0' || value[i] > '9' || number > 0xffffffffLL) is_number = 0;
                        else number = number * 10 + value[i] - '0';
                    }
                }
                if (!strcmp(folded, "base") || !strcmp(folded, "baseaddress")) {
                    if (is_number && number <= 0xffffffffLL) screen->base = (uint32_t)number & ~3u;
                    else if (!is_number && value_length && rv_ident_char(value[0], 1)) {
                        a->screen_label = rv_intern(a, value, value_length);
                        a->screen_label_length = value_length;
                    }
                } else if (is_number) {
                    if (!strcmp(folded, "unit")) screen->unit_w = screen->unit_h = rv_clamp(number, 1, 32);
                    else if (!strcmp(folded, "unitwidth")) screen->unit_w = rv_clamp(number, 1, 32);
                    else if (!strcmp(folded, "unitheight")) screen->unit_h = rv_clamp(number, 1, 32);
                    else if (!strcmp(folded, "width")) screen->width = rv_clamp(number, 1, 1024);
                    else if (!strcmp(folded, "height")) screen->height = rv_clamp(number, 1, 1024);
                }
            }
            return;
        }
        s = eol + 1;
    }
}

/* base=label, once the labels are defined: a label, or a .eqv name standing for a number or one. */
static void rv_screen_label(RvAsm *a) {
    if (a->screen_label <= 0 && !a->screen_label_length) return;
    const char *name = a->arena + a->screen_label;
    int length = a->screen_label_length;
    for (int depth = 0; depth < 8; depth++) {
        RvSymbol *s = rv_symbol(a, name, length);
        if (s) {
            a->program->screen.base = s->address & ~3u;
            return;
        }
        RvEntry *e = rv_find(a, &a->eqv, name, length, 0);
        if (!e) return;
        name = a->arena + e->value;
        length = (int)strlen(name);
        int64_t value;
        if (rv_number(name, length, &value) == 32) {
            a->program->screen.base = (uint32_t)value & ~3u;
            return;
        }
    }
}

/* ------------------------------------------------------------------ assembling */

int rv_assemble(RvProgram *program, const char *source, size_t length) {
    rv_program_free(program);
    RvAsm *a = RV_ALLOC(sizeof *a);
    if (!a) {
        program->error_count = 1;
        program->errors[0].line = 0;
        snprintf(program->errors[0].text, RV_ERROR_TEXT, "Out of memory");
        return 1;
    }
    a->program = program;
    a->source = source;
    a->source_length = length;
    a->text_address = RV_TEXT_BASE;
    a->data_address = RV_DATA_BASE;
    a->auto_align = 1;
    a->data_directive = D_WORD;
    rv_intern(a, "", 0);   /* place 0 is the empty name */
    rv_tables(a);
    rv_screen(a);

    const char *s = source, *end = source + length;
    int line = 0;
    while (s < end || (s == end && line == 0 && length == 0)) {
        if (s == end) break;
        const char *eol = memchr(s, '\n', (size_t)(end - s));
        if (!eol) eol = end;
        int n = (int)(eol - s);
        line++;
        if (rv_grow(a, (void **)&a->line, &a->line_capacity, n + 1, 1)) {
            memcpy(a->line, s, n);
            a->line[n] = 0;
            a->line_number = line;
            rv_line(a, n);
        }
        if (a->out_of_memory || program->error_count >= 200) break;
        s = eol + 1;
    }
    program->line_count = line;

    /* .globl: each name must be defined */
    for (int i = 0; i < a->global_count; i++) {
        const char *name = a->arena + a->globals[i];
        RvSymbol *sym = rv_symbol(a, name, (int)strlen(name));
        if (!sym) rv_error(a, a->global_lines[i], "'%.40s' is declared .globl but never defined", name);
        else sym->global = 1;
    }
    /* data that named a label before it was defined */
    for (int i = 0; i < a->forward_count; i++) {
        RvForward *f = &a->forwards[i];
        const char *name = a->arena + f->name;
        RvSymbol *sym = rv_symbol(a, name, (int)strlen(name));
        if (!sym) {
            rv_error(a, f->line, "No label named '%.40s'", name);
            continue;
        }
        for (int k = 0; k < f->length && k < 4; k++) a->data[f->offset + k] = (uint8_t)(sym->address >> (8 * k));
    }
    rv_screen_label(a);
    if (!a->out_of_memory) rv_second_pass(a);

    /* the data image, from the lowest address placed */
    uint32_t base = a->data_low && a->data_low < RV_DATA_BASE ? a->data_low & ~3u : RV_DATA_BASE;
    uint32_t top = RV_DATA_SEGMENT + a->data_end;
    program->data_base = base;
    program->data_size = top > base ? top - base : 0;
    if (program->data_size && !a->out_of_memory) {
        program->data = RV_ALLOC(program->data_size);
        if (program->data) {
            uint32_t from = base - RV_DATA_SEGMENT, have = a->data_capacity > from ? a->data_capacity - from : 0;
            memcpy(program->data, a->data + from, have < program->data_size ? have : program->data_size);
        } else {
            a->out_of_memory = 1;
        }
    }
    program->entry = RV_TEXT_BASE;
    RvSymbol *main_label = rv_symbol(a, "main", 4);
    if (main_label && main_label->global) program->entry = main_label->address;
    if (a->out_of_memory) rv_error(a, 0, "Out of memory");

    rv_table_free(&a->keywords);
    rv_table_free(&a->operators);
    rv_table_free(&a->symbols);
    rv_table_free(&a->eqv);
    RV_FREE(a->arena);
    RV_FREE(a->statements);
    RV_FREE(a->data);
    RV_FREE(a->forwards);
    RV_FREE(a->line);
    RV_FREE(a->symbol_names);
    RV_FREE(a);
    return program->error_count;
}

void rv_program_free(RvProgram *program) {
    RV_FREE(program->text);
    RV_FREE(program->text_line);
    RV_FREE(program->data);
    RV_FREE(program->symbols);
    memset(program, 0, sizeof *program);
}

/* ------------------------------------------------------------------ looking things up */

int rv_line_of(const RvProgram *program, uint32_t address) {
    uint32_t index = (address - RV_TEXT_BASE) >> 2;
    if ((address & 3) || address < RV_TEXT_BASE || index >= (uint32_t)program->text_words) return 0;
    return program->text_line[index];
}

uint32_t rv_address_of_line(const RvProgram *program, int line) {
    for (int i = 0; i < program->text_words; i++)
        if (program->text_line[i] == line) return RV_TEXT_BASE + 4u * (uint32_t)i;
    return 0;
}

const RvSymbol *rv_symbol_at(const RvProgram *program, uint32_t address) {
    const RvSymbol *found = NULL;
    for (int i = 0; i < program->symbol_count; i++) {
        const RvSymbol *s = &program->symbols[i];
        if (s->address != address) continue;
        if (s->text) return s;
        if (!found) found = s;
    }
    return found;
}
