/*
 * DOOM's x86 assembly, which only asmOS's x86 machine has: DOOM_X86 is 1 there, and 0 on RISC-V and
 * MIPS. Where it is 0, the Files that call these leave the calls out, and DOOM goes without them:
 *
 *   - doom_simd.asm's AVX2 loops: DOOM draws, turns its picture into the window's colours and mixes
 *     its sound in its own C, as on x86 with Game > Fast Drawing off, which is not in the menu;
 *   - asmos_bench.asm's loops: Game > Benchmark CPU, which times x86 instructions, is not in it;
 *   - asmos_fresh.asm and the .part.asm Files around each part, which put DOOM's variables back as
 *     they started before a second game: there is one game each time asmOS starts (i_asmos.c).
 *
 * Game > Profile goes too, though it is C: it names functions from the ELF symbol table of the
 * linked os/main, which only an x86 Build makes.
 */
#ifndef ASMOS_DOOM_X86_H
#define ASMOS_DOOM_X86_H

#if defined(__x86_64__)
#define DOOM_X86 1
#else
#define DOOM_X86 0
#endif

#if DOOM_X86

#include <stdint.h>

/* Game > Fast Drawing: 1 while doom_simd.asm's loops draw rather than DOOM's own C (i_asmos.c) */
extern int doom_simd;

/* the loops of R_DrawColumn and R_DrawSpan, eight pixels at a time (r_draw.c, r_plane.c,
 * asmos_segloop.c), and of R_DrawColumnLow and R_DrawSpanLow, each pixel two wide */
void doom_column(unsigned char *dest, int count, int frac, int step, const unsigned char *source,
                 const unsigned char *colormap);
void doom_span(unsigned char *dest, int count, unsigned int position, unsigned int step,
               const unsigned char *source, const unsigned char *colormap);
void doom_column2(unsigned char *dest, int count, int frac, int step, const unsigned char *source,
                  const unsigned char *colormap);
void doom_span2(unsigned char *dest, int count, unsigned int position, unsigned int step,
                const unsigned char *source, const unsigned char *colormap);

/* rows of DOOM's picture through the palette, once or twice as wide, and whether a row changed
 * since the window last showed it (i_asmos.c) */
void doom_palette_row(uint32_t *to, const unsigned char *from, int count, const uint32_t *palette);
void doom_palette_row2(uint64_t *to, const unsigned char *from, int count, const uint64_t *palette2);
int doom_row_changed(const unsigned char *row, unsigned char *previous, int count);

/* the sound mixer: effects added into a mix, and the mix out as 16-bit samples (i_asmos_sound.c) */
void doom_mix_add(int32_t *mix, const unsigned char *samples, int count, int volume);
void doom_mix_out(int16_t *out, int32_t *mix, int count);

/* asmos_fresh.asm: DOOM's variables as they were when asmOS started (i_asmos.c) */
void doom_fresh(void);

/* asmos_bench.asm: Game > Benchmark CPU's loops, n turns of one kind of instruction (i_asmos.c) */
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

#endif

#endif
