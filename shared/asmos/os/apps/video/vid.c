/*
 * The .vid format: a black-and-white video and its sound, made for asmOS by encode.c beside this
 * File. This File reads it; the Video Player (../video.c) and encode.c both include it, so what
 * encode.c writes is checked against this very code.
 *
 * A picture is rows of pixels that are each black or white, and a row is stored as its changes:
 * the x of every pixel whose colour differs from the one before it, black coming first. Each row
 * is coded the way a fax machine codes a page (ITU-T T.6, "G4"): against a reference row that is
 * nearly the same, so that most changes are "the reference's change", or that change moved a
 * pixel or two. The reference is the same row of the previous frame, maybe a few rows up or down
 * when the picture moves, or the row above in this frame, whichever costs less: the encoder tries
 * them all. A row the same as the previous frame's costs a fraction of a bit, and so does one the
 * same as its reference.
 *
 * Every decision of a picture, a bit at a time, goes through an adaptive binary range coder, the
 * one of LZMA: each kind of decision has a probability that follows what that decision has been,
 * and a bit costs only what its surprise is worth, so the usual cases cost a fraction of a bit.
 *
 * The sound is ADPCM, laid out as the QOA format lays it out: a predictor guesses each sample from
 * the four before it, and only the difference from the guess is stored, as one of 8 steps of a
 * scale that each slice of 20 samples chooses for itself. A slice is 64 bits: 4 for the scale and
 * 3 for each sample, so the sound costs 3.2 bits a sample, and making it takes no coder at all.
 *
 * The video is cut into chunks of a few seconds, each coded on its own, from a black frame and
 * fresh probabilities, and from silence: the player can start at any chunk. A file is,
 * little-endian:
 *
 *   "VID1", u16 width, u16 height, u16 frames a second, u16 frames a chunk, u32 frames,
 *   u32 samples a second (0: no sound), u32 samples, u32 chunks, char title[64]
 *   then (chunks + 1) times u32 picture offset, u32 sound offset: where each chunk's two streams
 *   start in the file, and last where they end
 *   then the streams.
 */

#include <stdint.h>
#include <string.h>
#include "../../os.h"                   /* read_le16 and read_le32 */

#define VID_INLINE static inline __attribute__((always_inline))

enum {
    VID_HEADER = 92,
    VID_MAX_WIDTH = 1024, VID_MAX_HEIGHT = 1024,
    VID_NEAR = 8,           /* a change at most 7 from its reference's is "near" */
    VID_RUN_BITS = 10,      /* bits of the longest run, VID_MAX_WIDTH */
    VID_MOVES = 8,          /* how many rows up or down a reference may be */
    VID_SLICE = 20,         /* the samples of a slice */
};

typedef struct {
    const uint8_t *data;
    long size;
    int width, height, fps, chunk_frames, frames, rate, chunks;
    long samples;
    char title[65];
} VidFile;

static uint32_t vid_u16(const uint8_t *at) { return read_le16(at); }
static uint32_t vid_u32(const uint8_t *at) { return read_le32(at); }

/* Where chunk `chunk`'s picture (sound 0) or sound (sound 1) starts; the chunk after the last is
 * where they end. */
static long vid_offset(const VidFile *v, int chunk, int sound) {
    return vid_u32(v->data + VID_HEADER + chunk * 8 + sound * 4);
}

/* How many samples each chunk has: as many as its frames last. */
static long vid_chunk_samples(const VidFile *v) { return (long)v->chunk_frames * v->rate / v->fps; }

/* Reads the header and checks it against the file's size: 1 when it is a .vid this code plays. */
static int vid_open(VidFile *v, const uint8_t *data, long size) {
    memset(v, 0, sizeof *v);
    if (size < VID_HEADER || memcmp(data, "VID1", 4) != 0) return 0;
    v->data = data;
    v->size = size;
    v->width = vid_u16(data + 4);
    v->height = vid_u16(data + 6);
    v->fps = vid_u16(data + 8);
    v->chunk_frames = vid_u16(data + 10);
    v->frames = (int)vid_u32(data + 12);
    v->rate = (int)vid_u32(data + 16);
    v->samples = vid_u32(data + 20);
    v->chunks = (int)vid_u32(data + 24);
    memcpy(v->title, data + 28, 64);
    v->title[64] = 0;
    if (v->width < 1 || v->width > VID_MAX_WIDTH || v->height < 1 || v->height > VID_MAX_HEIGHT || v->fps < 1 ||
        v->chunk_frames < 1 || v->frames < 1 || v->rate < 0 || v->rate > 96000)
        return 0;
    if (v->chunks != (v->frames + v->chunk_frames - 1) / v->chunk_frames) return 0;
    if (v->samples > (long)v->frames * v->rate / v->fps + v->rate) return 0;
    if (VID_HEADER + (long)(v->chunks + 1) * 8 > size) return 0;
    for (int chunk = 0; chunk <= v->chunks; chunk++)
        for (int sound = 0; sound < 2; sound++) {
            long at = vid_offset(v, chunk, sound);
            if (at > size || (chunk && at < vid_offset(v, chunk - 1, sound))) return 0;
        }
    /* every chunk's sound whole: a slice of 8 bytes for each 20 samples */
    for (int chunk = 0; chunk < v->chunks && v->rate; chunk++) {
        long first = chunk * vid_chunk_samples(v), count = v->samples - first;
        if (count > vid_chunk_samples(v)) count = vid_chunk_samples(v);
        if (count > 0 && vid_offset(v, chunk + 1, 1) - vid_offset(v, chunk, 1) < (count + VID_SLICE - 1) / VID_SLICE * 8)
            return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ the range decoder */

enum { VID_PROBABILITY_BITS = 11, VID_ONE = 1 << VID_PROBABILITY_BITS, VID_ADAPT = 5 };

typedef struct {
    const uint8_t *at, *end;
    uint32_t range, code;
} VidCoder;

static void vid_coder_start(VidCoder *c, const uint8_t *at, const uint8_t *end) {
    c->at = at;
    c->end = end;
    c->range = 0xffffffffu;
    c->code = 0;
    for (int i = 0; i < 5; i++) c->code = c->code << 8 | (c->at < c->end ? *c->at++ : 0);
}

/*
 * A decision whose probability of being 0 is *p out of VID_ONE; *p moves a 32nd of the way
 * towards what the decision turned out to be. A picture takes thousands of these, so they are
 * written into the code that makes them, on a copy of the coder that stays in registers.
 */
VID_INLINE int vid_bit(VidCoder *c, uint16_t *p) {
    uint32_t bound = (c->range >> VID_PROBABILITY_BITS) * *p;
    int bit;
    if (c->code < bound) {
        c->range = bound;
        *p += (VID_ONE - *p) >> VID_ADAPT;
        bit = 0;
    } else {
        c->range -= bound;
        c->code -= bound;
        *p -= *p >> VID_ADAPT;
        bit = 1;
    }
    /* no probability is under 1/64, so one byte always brings the range back */
    if (c->range < 1u << 24) {
        c->range <<= 8;
        c->code = c->code << 8 | (c->at < c->end ? *c->at++ : 0);
    }
    return bit;
}

/* Every probability at an even chance. */
static void vid_fresh(uint16_t *probabilities, size_t bytes) {
    for (size_t i = 0; i < bytes / 2; i++) probabilities[i] = VID_ONE / 2;
}

/* ------------------------------------------------------------------ pictures */

/* The decisions a picture is made of, each kind with its probabilities, split by what came just
 * before: `last` is how the previous change of the row was made (0 at the reference's change, 1
 * near it, 2 by pass or runs), `above` what the row above was (0 the same as before, 1 from the
 * previous frame, 2 from the row above), and `reference` whether the reference is the previous
 * frame (0) or the row above (1). */
typedef struct {
    uint16_t same[3];                          /* [above]: the row is the previous frame's */
    uint16_t from_above[3];                    /* [above]: its reference is the row above */
    uint16_t move_again[2];                    /* [last move was not 0]: moved as the last row was */
    uint16_t move[VID_MOVES];                  /* how many rows, one at a time */
    uint16_t move_up;
    uint16_t own[2][3];                        /* [reference][above]: the row is not its reference */
    uint16_t moved[2][3];                      /* [reference][last]: the change is not b1 */
    uint16_t far[2][3];                        /* [reference][last]: nor near it */
    uint16_t runs[2][3];                       /* [reference][last]: so runs, or else pass */
    uint16_t distance[2][2][VID_NEAR];         /* [reference][last was not 0][past 1, one at a time] */
    uint16_t left[2][3];                       /* [reference][the last distance: 0, right, left] */
    uint16_t run_bits[2][VID_RUN_BITS + 1];    /* [colour]: how many bits the run has, one at a time */
    uint16_t run_value[2][VID_RUN_BITS + 1][4];/* [colour][bits][which: the first three, the rest] */
} VidPictureModel;

typedef struct {
    const VidFile *file;
    VidCoder coder;
    VidPictureModel model;
    int frame;              /* the next frame to make */
    int broken;             /* the file turned out wrong: the picture stops */
    /* Each row has two slots, the row of this frame and that of the previous one: a row that
     * changes is made in the slot the previous frame does not use, so nothing is ever copied. */
    uint16_t *changes[2];   /* [slot]: height rows of width + 1 */
    uint16_t *counts[2];    /* [slot]: each row's number of changes */
    uint8_t *slot;          /* each row's slot in this frame */
    uint8_t *previous_slot; /* and in the previous one */
    uint8_t *changed;       /* rows changed since the player last cleared these */
} VidPicture;

static uint16_t *vid_changes(const VidPicture *p, int slot, int y) {
    return p->changes[slot] + (long)y * (p->file->width + 1);
}

/* Row y of the frame made last: its changes, and how many. */
static const uint16_t *vid_row(const VidPicture *p, int y, int *count) {
    *count = p->counts[p->slot[y]][y];
    return vid_changes(p, p->slot[y], y);
}

/* A run's length: how many bits it has, then those bits below the top one. */
VID_INLINE int vid_run(VidCoder *c, VidPictureModel *m, int colour) {
    int bits = 0;
    while (bits < VID_RUN_BITS && vid_bit(c, &m->run_bits[colour][bits])) bits++;
    int value = 1;
    for (int i = 0; i < bits; i++) value = value << 1 | vid_bit(c, &m->run_value[colour][bits][i < 3 ? i : 3]);
    return value - 1;
}

/*
 * One row, coded against `ref` (`ref_count` changes): its changes into `out`, and how many, or -1
 * for a broken file. a0 is where the row is, the last change made (-1 before the first); b1 is the
 * reference's first change past a0 to the colour a0 does not have, b2 the reference's change after
 * b1. Then the row's next change, a1, is b1, or near b1 (a0 moves to it), or past b2 ("pass": a0
 * moves to b2, the colour unchanged), or none of these: then the two runs to it and to the change
 * after it are given in full.
 */
static int vid_line(VidCoder *coder, VidPictureModel *m, int reference, const uint16_t *ref, int ref_count,
                    uint16_t *out, int width) {
    VidCoder c = *coder;
    int a0 = -1, colour = 0, i = 0, n = 0, last = 0, last_d = 0;
    while (a0 < width) {
        while (i < ref_count && ref[i] <= a0) i++;
        int k = i + ((i & 1) != colour);  /* a change at an even place turns the row white */
        int b1 = k < ref_count ? ref[k] : width, b2 = k + 1 < ref_count ? ref[k + 1] : width;
        if (!vid_bit(&c, &m->moved[reference][last])) {
            if (b1 < width) out[n++] = (uint16_t)b1;
            a0 = b1;
            colour ^= 1;
            last = last_d = 0;
        } else if (!vid_bit(&c, &m->far[reference][last])) {
            int d = 1;
            while (d < VID_NEAR - 1 && vid_bit(&c, &m->distance[reference][last != 0][d - 1])) d++;
            int left = vid_bit(&c, &m->left[reference][last_d]);
            int a1 = left ? b1 - d : b1 + d;
            if (a1 <= a0 || a1 > width) {
                n = -1;
                break;
            }
            if (a1 < width) out[n++] = (uint16_t)a1;
            a0 = a1;
            colour ^= 1;
            last = 1;
            last_d = 1 + left;
        } else if (!vid_bit(&c, &m->runs[reference][last])) {
            a0 = b2;
            last = 2;
        } else {
            int start = a0 < 0 ? 0 : a0;
            int a1 = start + vid_run(&c, m, colour);
            int a2 = a1 + vid_run(&c, m, colour ^ 1);
            if (a1 <= a0 || a2 > width || (a2 == a1 && a1 < width)) {
                n = -1;
                break;
            }
            if (a1 < width) out[n++] = (uint16_t)a1;
            if (a2 < width) out[n++] = (uint16_t)a2;
            a0 = a2;
            last = 2;
        }
    }
    *coder = c;
    return n;
}

/* Makes frame p->frame: 1, or 0 at the end or once the file turned out wrong. */
static int vid_picture_next(VidPicture *p) {
    const VidFile *v = p->file;
    if (p->frame >= v->frames || p->broken) return 0;
    VidCoder *c = &p->coder;
    VidPictureModel *m = &p->model;
    int width = v->width, height = v->height;
    if (p->frame % v->chunk_frames == 0) {
        /* a chunk starts from a black frame: every row in slot 0 with no changes */
        int chunk = p->frame / v->chunk_frames;
        vid_coder_start(c, v->data + vid_offset(v, chunk, 0), v->data + vid_offset(v, chunk + 1, 0));
        vid_fresh((uint16_t *)m, sizeof *m);
        memset(p->slot, 0, height);
        memset(p->counts[0], 0, height * sizeof(uint16_t));
        memset(p->changed, 1, height);
    }
    memcpy(p->previous_slot, p->slot, height);
    int above = 0, last_move = 0;
    for (int y = 0; y < height; y++) {
        if (!vid_bit(c, &m->same[above])) {
            above = 0;
            continue;
        }
        const uint16_t *ref;
        int ref_count;
        int from_above = vid_bit(c, &m->from_above[above]);
        if (from_above) {
            ref = y ? vid_changes(p, p->slot[y - 1], y - 1) : NULL;
            ref_count = y ? p->counts[p->slot[y - 1]][y - 1] : 0;
        } else {
            int move = last_move;
            if (!vid_bit(c, &m->move_again[last_move != 0])) {
                move = 0;
                while (move < VID_MOVES && vid_bit(c, &m->move[move])) move++;
                if (move && vid_bit(c, &m->move_up)) move = -move;
            }
            int from = y + move;
            if (from < 0 || from >= height) {
                p->broken = 1;
                return 0;
            }
            ref = vid_changes(p, p->previous_slot[from], from);
            ref_count = p->counts[p->previous_slot[from]][from];
            last_move = move;
        }
        int slot = p->previous_slot[y] ^ 1, count = ref_count;
        uint16_t *out = vid_changes(p, slot, y);
        if (vid_bit(c, &m->own[from_above][above])) count = vid_line(c, m, from_above, ref, ref_count, out, width);
        else memcpy(out, ref, ref_count * sizeof *ref);
        if (count < 0) {
            p->broken = 1;
            return 0;
        }
        p->counts[slot][y] = (uint16_t)count;
        p->slot[y] = (uint8_t)slot;
        p->changed[y] = 1;
        above = 1 + from_above;
    }
    p->frame++;
    return 1;
}

/* The next frame made will be the first of chunk `chunk`. */
static void vid_picture_seek(VidPicture *p, int chunk) {
    p->frame = chunk * p->file->chunk_frames;
}

/* ------------------------------------------------------------------ sound */

/* What each 3-bit code of a slice stands for, at each of the 16 scales: the scale, (s + 1) to the
 * power 2.75, times 0.75, 2.5, 4.5 and 7, either way. */
static const int16_t VID_STEPS[16][8] = {
    { 1, -1, 3, -3, 5, -5, 7, -7 },
    { 5, -5, 18, -18, 32, -32, 49, -49 },
    { 16, -16, 53, -53, 95, -95, 147, -147 },
    { 34, -34, 113, -113, 203, -203, 315, -315 },
    { 63, -63, 210, -210, 378, -378, 588, -588 },
    { 104, -104, 345, -345, 621, -621, 966, -966 },
    { 158, -158, 528, -528, 950, -950, 1477, -1477 },
    { 228, -228, 760, -760, 1368, -1368, 2128, -2128 },
    { 316, -316, 1053, -1053, 1895, -1895, 2947, -2947 },
    { 422, -422, 1405, -1405, 2529, -2529, 3934, -3934 },
    { 548, -548, 1828, -1828, 3290, -3290, 5117, -5117 },
    { 696, -696, 2320, -2320, 4176, -4176, 6496, -6496 },
    { 868, -868, 2893, -2893, 5207, -5207, 8099, -8099 },
    { 1064, -1064, 3548, -3548, 6386, -6386, 9933, -9933 },
    { 1286, -1286, 4288, -4288, 7718, -7718, 12005, -12005 },
    { 1536, -1536, 5120, -5120, 9216, -9216, 14336, -14336 },
};

/* The predictor: the guess is the four samples before, weighted. */
typedef struct {
    int history[4], weights[4];
} VidGuess;

/* A chunk starts afresh: the guess is the line through the last two samples, until it learns. */
static void vid_guess_start(VidGuess *g) {
    memset(g, 0, sizeof *g);
    g->weights[2] = -(1 << 13);
    g->weights[3] = 1 << 14;
}

VID_INLINE int vid_clamp16(int value) { return value < -32768 ? -32768 : value > 32767 ? 32767 : value; }

VID_INLINE int vid_guess(const VidGuess *g) {
    int64_t sum = (int64_t)g->history[0] * g->weights[0] + (int64_t)g->history[1] * g->weights[1] +
                  (int64_t)g->history[2] * g->weights[2] + (int64_t)g->history[3] * g->weights[3];
    return (int)(sum >> 13);
}

/* Each weight moves to make the guess better: by a sixteenth of the error, up if its sample had
 * the error's sign and down if not. */
VID_INLINE void vid_learn(VidGuess *g, int sample, int error) {
    int delta = error >> 4;
    for (int i = 0; i < 4; i++) g->weights[i] += g->history[i] < 0 ? -delta : delta;
    g->history[0] = g->history[1];
    g->history[1] = g->history[2];
    g->history[2] = g->history[3];
    g->history[3] = sample;
}

typedef struct {
    const VidFile *file;
    VidGuess guess;
    long sample;            /* the next sample to make */
} VidSound;

/* The next sample made will be the first of chunk `chunk`. */
static void vid_sound_seek(VidSound *s, int chunk) {
    s->sample = (long)chunk * vid_chunk_samples(s->file);
    vid_guess_start(&s->guess);
}

/* Makes up to `count` samples into `out`; answers how many, 0 at the end. */
static int vid_sound_read(VidSound *s, int16_t *out, int count) {
    const VidFile *v = s->file;
    long per_chunk = vid_chunk_samples(v);
    int made = 0;
    while (made < count && s->sample < v->samples) {
        long in_chunk = s->sample % per_chunk;
        if (in_chunk == 0) vid_guess_start(&s->guess);
        const uint8_t *at = v->data + vid_offset(v, (int)(s->sample / per_chunk), 1) + in_chunk / VID_SLICE * 8;
        uint64_t slice = (uint64_t)vid_u32(at + 4) << 32 | vid_u32(at);
        const int16_t *steps = VID_STEPS[slice >> 60];
        /* the codes from the top, after the scale; a slice may be entered part way, after a seek */
        int first = (int)(in_chunk % VID_SLICE);
        slice <<= 4 + 3 * first;
        int end = VID_SLICE;
        if (end - first > count - made) end = first + count - made;
        if (end - first > v->samples - s->sample) end = first + (int)(v->samples - s->sample);
        if (end - first > per_chunk - in_chunk) end = first + (int)(per_chunk - in_chunk);
        /* vid_guess and vid_learn written out, on variables the compiler keeps in registers: this
         * runs for every sample */
        int64_t h0 = s->guess.history[0], h1 = s->guess.history[1], h2 = s->guess.history[2],
                h3 = s->guess.history[3];
        int64_t w0 = s->guess.weights[0], w1 = s->guess.weights[1], w2 = s->guess.weights[2],
                w3 = s->guess.weights[3];
        int16_t *to = out + made;
        for (int i = first; i < end; i++) {
            int error = steps[slice >> 61];
            int64_t up = error >> 4, down = -up;
            slice <<= 3;
            int sample = vid_clamp16((int)((h0 * w0 + h1 * w1 + h2 * w2 + h3 * w3) >> 13) + error);
            w0 += h0 < 0 ? down : up;
            w1 += h1 < 0 ? down : up;
            w2 += h2 < 0 ? down : up;
            w3 += h3 < 0 ? down : up;
            h0 = h1;
            h1 = h2;
            h2 = h3;
            h3 = sample;
            *to++ = (int16_t)sample;
        }
        s->guess.history[0] = (int)h0, s->guess.history[1] = (int)h1;
        s->guess.history[2] = (int)h2, s->guess.history[3] = (int)h3;
        s->guess.weights[0] = (int)w0, s->guess.weights[1] = (int)w1;
        s->guess.weights[2] = (int)w2, s->guess.weights[3] = (int)w3;
        made += end - first;
        s->sample += end - first;
    }
    return made;
}
