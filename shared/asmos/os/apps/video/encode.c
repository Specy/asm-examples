/*
 * The .vid encoder: a program for your own computer, not a part of asmOS. It made
 * videos/bad-apple.vid, and it makes a .vid of any video, from its frames and its sound as raw
 * numbers, which ffmpeg writes:
 *
 *   ffmpeg -i video.mp4 -vf scale=480:360:flags=area -f rawvideo -pix_fmt gray frames.raw
 *   ffmpeg -i video.mp4 -vn -ac 1 -ar 16000 -f s16le sound.raw
 *   cc -O2 -o encode encode.c -lm
 *   ./encode frames.raw 480 360 30 sound.raw 16000 "Bad Apple!!" bad-apple.vid
 *
 * A pixel is white from 192 up and black under 64; one between keeps the colour it had in the
 * previous frame, so that a blurred edge does not flicker. A run shorter than 3 pixels that the
 * previous frame did not have takes the colour around it, and a change at most 2 pixels from its
 * reference's is moved onto it: the edges keep still unless they really move. Each slice of the
 * sound takes the scale of the 16 that makes its error least, its guess's weights counted in. A
 * chunk is 150 frames, five seconds at 30 a second. The file is then read back with vid.c, the code
 * asmOS plays it with, and every frame and sample compared with what the encoder meant.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "vid.c"

enum { CHUNK_FRAMES = 150, BLACK_UNDER = 64, WHITE_FROM = 192, SPECK = 3, SNAP = 2, MOVES = 6 };

/* ------------------------------------------------------------------ the range encoder, LZMA's */

typedef struct {
    uint64_t low;
    uint32_t range;
    uint8_t cache;
    uint64_t pending;       /* bytes waiting on a carry: the cache, then 0xff ones */
    uint64_t shifts;        /* bytes made in all, what a trial measures */
    int trial;              /* only counting */
    uint8_t *out;
    size_t size, capacity;
} Encoder;

static void put_byte(Encoder *e, uint8_t byte) {
    if (e->size == e->capacity) {
        e->capacity = e->capacity ? e->capacity * 2 : 1 << 16;
        e->out = realloc(e->out, e->capacity);
        if (!e->out) exit(1);
    }
    e->out[e->size++] = byte;
}

static void shift_low(Encoder *e) {
    e->shifts++;
    if (!e->trial && ((uint32_t)e->low < 0xff000000u || e->low >> 32)) {
        uint8_t carry = (uint8_t)(e->low >> 32), byte = e->cache;
        do {
            put_byte(e, (uint8_t)(byte + carry));
            byte = 0xff;
        } while (--e->pending);
        e->cache = (uint8_t)(e->low >> 24);
    }
    e->pending++;
    e->low = (e->low & 0x00ffffffu) << 8;
}

static void encoder_start(Encoder *e) {
    e->low = 0;
    e->range = 0xffffffffu;
    e->cache = 0;
    e->pending = 1;
}

static void encoder_end(Encoder *e) {
    for (int i = 0; i < 5; i++) shift_low(e);
}

/* The same decision as vid_bit, the same probability moved the same way. */
static void put_bit(Encoder *e, uint16_t *p, int bit) {
    uint32_t bound = (e->range >> VID_PROBABILITY_BITS) * *p;
    if (!bit) {
        e->range = bound;
        *p += (VID_ONE - *p) >> VID_ADAPT;
    } else {
        e->low += bound;
        e->range -= bound;
        *p -= *p >> VID_ADAPT;
    }
    while (e->range < 1u << 24) {
        e->range <<= 8;
        shift_low(e);
    }
}

/* What the decisions so far cost, in bits. */
static double cost(const Encoder *e) { return (double)e->shifts * 8 - log2((double)e->range); }

/* An encoder that only counts, starting where `e` is. */
static Encoder trial_of(const Encoder *e) {
    Encoder t = *e;
    t.trial = 1;
    t.out = NULL;
    t.shifts = 0;
    return t;
}

/* ------------------------------------------------------------------ pictures */

static int width, height;

typedef struct {
    int count;
    int *x;
} Row;

static void put_run(Encoder *e, VidPictureModel *m, int colour, int run) {
    int value = run + 1, bits = 0;
    while (value >> (bits + 1)) bits++;
    for (int i = 0; i < bits; i++) put_bit(e, &m->run_bits[colour][i], 1);
    if (bits < VID_RUN_BITS) put_bit(e, &m->run_bits[colour][bits], 0);
    for (int i = 0; i < bits; i++) put_bit(e, &m->run_value[colour][bits][i < 3 ? i : 3], value >> (bits - 1 - i) & 1);
}

/* vid_line's other side: codes `want` against `ref`, making `out`, which is `want` with the
 * changes near their reference's moved onto it. */
static void put_line(Encoder *e, VidPictureModel *m, int reference, const Row *ref, const Row *want, Row *out) {
    int a0 = -1, colour = 0, i = 0, j = 0, n = 0, last = 0, last_d = 0;
    while (a0 < width) {
        while (i < ref->count && ref->x[i] <= a0) i++;
        int k = i + ((i & 1) != colour);
        int b1 = k < ref->count ? ref->x[k] : width, b2 = k + 1 < ref->count ? ref->x[k + 1] : width;
        int a1 = j < want->count ? want->x[j] : width, a2 = j + 1 < want->count ? want->x[j + 1] : width;
        if (b2 >= a1 && abs(a1 - b1) < VID_NEAR) {
            int d = a1 - b1;
            if (d && abs(d) <= SNAP && b1 > a0 && b1 < a2 && a1 < width) d = 0;
            put_bit(e, &m->moved[reference][last], d != 0);
            if (d) {
                int distance = abs(d);
                put_bit(e, &m->far[reference][last], 0);
                for (int s = 1; s < distance; s++) put_bit(e, &m->distance[reference][last != 0][s - 1], 1);
                if (distance < VID_NEAR - 1) put_bit(e, &m->distance[reference][last != 0][distance - 1], 0);
                put_bit(e, &m->left[reference][last_d], d < 0);
            }
            a1 = b1 + d;
            if (a1 < width) out->x[n++] = a1;
            a0 = a1;
            colour ^= 1;
            last = d != 0;
            last_d = d ? 1 + (d < 0) : 0;
            j++;
        } else if (b2 < a1) {
            put_bit(e, &m->moved[reference][last], 1);
            put_bit(e, &m->far[reference][last], 1);
            put_bit(e, &m->runs[reference][last], 0);
            a0 = b2;
            last = 2;
        } else {
            put_bit(e, &m->moved[reference][last], 1);
            put_bit(e, &m->far[reference][last], 1);
            put_bit(e, &m->runs[reference][last], 1);
            int start = a0 < 0 ? 0 : a0;
            put_run(e, m, colour, a1 - start);
            put_run(e, m, colour ^ 1, a2 - a1);
            if (a1 < width) out->x[n++] = a1;
            if (a2 < width) out->x[n++] = a2;
            a0 = a2;
            j += 2;
            last = 2;
        }
    }
    out->count = n;
}

/* The choice of reference: the row above, or the previous frame's row `move` rows down. */
static void put_reference(Encoder *e, VidPictureModel *m, int above, int last_move, int from_above, int move) {
    put_bit(e, &m->from_above[above], from_above);
    if (from_above) return;
    put_bit(e, &m->move_again[last_move != 0], move == last_move);
    if (move == last_move) return;
    int distance = abs(move);
    for (int s = 0; s < distance; s++) put_bit(e, &m->move[s], 1);
    if (distance < VID_MOVES) put_bit(e, &m->move[distance], 0);
    if (distance) put_bit(e, &m->move_up, move < 0);
}

static int same_row(const Row *a, const Row *b);
static void copy_row(Row *to, const Row *from);

/* A row that is not the previous frame's: its reference, then the row against it, or one
 * decision when it comes out as the reference itself. */
static void put_row(Encoder *e, VidPictureModel *m, int above, int last_move, int from_above, int move,
                    const Row *ref, const Row *want, Row *out) {
    put_reference(e, m, above, last_move, from_above, move);
    Encoder t = trial_of(e);
    VidPictureModel trial_model = *m;
    put_line(&t, &trial_model, from_above, ref, want, out);
    int own = !same_row(out, ref);
    put_bit(e, &m->own[from_above][above], own);
    if (own) put_line(e, m, from_above, ref, want, out);
    else copy_row(out, ref);
}

static void row_of(const uint8_t *pixels, Row *row) {
    int n = 0, colour = 0;
    for (int x = 0; x < width; x++)
        if (pixels[x] != colour) {
            row->x[n++] = x;
            colour ^= 1;
        }
    row->count = n;
}

static int same_row(const Row *a, const Row *b) {
    return a->count == b->count && !memcmp(a->x, b->x, a->count * sizeof(int));
}

static void copy_row(Row *to, const Row *from) {
    to->count = from->count;
    memcpy(to->x, from->x, from->count * sizeof(int));
}

/* A row of pixels, 0 or 1, eight to a byte. */
static void pack(const uint8_t *pixels, uint8_t *bits) {
    memset(bits, 0, (width + 7) / 8);
    for (int x = 0; x < width; x++) bits[x >> 3] |= pixels[x] << (x & 7);
}

static Row *new_rows(int count) {
    Row *rows = calloc(count, sizeof(Row));
    for (int i = 0; i < count; i++) rows[i].x = calloc(width + 1, sizeof(int));
    return rows;
}

/* ------------------------------------------------------------------ sound */

/*
 * The code of `steps` that brings a guess nearest the sample, the sample it makes, and its error.
 * The error is measured before the sample is clamped to 16 bits: on sound that is clipped, a guess
 * far past the top would make the same clamped sample with any code, and only the largest step
 * down teaches the weights to guess lower again.
 */
static int nearest(const int16_t *steps, int guess, int want, int *made, double *error) {
    int best = 0;
    for (int q = 1; q < 8; q++)
        if (abs(want - (guess + steps[q])) < abs(want - (guess + steps[best]))) best = q;
    *made = vid_clamp16(guess + steps[best]);
    *error = (double)(want - (guess + steps[best])) * (want - (guess + steps[best]));
    return best;
}

/* What a guess's weights add to a slice's error: nothing while they stay small, and more and more
 * as they grow, since a scale that lets them grow loses the guess on loud sound, which then swings
 * from one end to the other. QOA's encoder weighs its choices the same way. */
static double weights_penalty(const VidGuess *g) {
    int64_t sum = 0;
    for (int i = 0; i < 4; i++) sum += (int64_t)g->weights[i] * g->weights[i];
    double penalty = (double)((sum >> 18) - 0x8ff);
    return penalty > 0 ? penalty * penalty : 0;
}

/* One chunk of sound, `count` samples from `in`, as slices into `e`'s bytes; what the decoder will
 * make goes to `out`. */
static void put_sound_chunk(Encoder *e, const int16_t *in, long count, int16_t *out) {
    VidGuess guess;
    vid_guess_start(&guess);
    for (long at = 0; at < count; at += VID_SLICE) {
        int length = count - at < VID_SLICE ? (int)(count - at) : VID_SLICE;
        double best = INFINITY;
        int best_scale = 0;
        for (int scale = 0; scale < 16; scale++) {
            VidGuess g = guess;
            double error = 0;
            for (int i = 0; i < length && error < best; i++) {
                int sample;
                double off;
                int q = nearest(VID_STEPS[scale], vid_guess(&g), in[at + i], &sample, &off);
                vid_learn(&g, sample, VID_STEPS[scale][q]);
                error += off + weights_penalty(&g);
            }
            if (error < best) best = error, best_scale = scale;
        }
        uint64_t slice = (uint64_t)best_scale << 60;
        for (int i = 0; i < length; i++) {
            int sample;
            double off;
            int q = nearest(VID_STEPS[best_scale], vid_guess(&guess), in[at + i], &sample, &off);
            slice |= (uint64_t)q << (57 - 3 * i);
            vid_learn(&guess, sample, VID_STEPS[best_scale][q]);
            out[at + i] = (int16_t)sample;
        }
        for (int i = 0; i < 8; i++) put_byte(e, (uint8_t)(slice >> 8 * i));
    }
}

/* ------------------------------------------------------------------ the file */

static void put16(uint8_t *at, uint32_t value) {
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
}
static void put32(uint8_t *at, uint32_t value) {
    for (int i = 0; i < 4; i++) at[i] = (uint8_t)(value >> 8 * i);
}

static uint8_t *read_all(const char *path, long *size) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    uint8_t *data = malloc(*size ? *size : 1);
    if (fread(data, 1, *size, file) != (size_t)*size) *size = 0;
    fclose(file);
    return data;
}

int main(int argc, char **argv) {
    if (argc != 9) {
        fprintf(stderr, "encode FRAMES.raw WIDTH HEIGHT FPS SOUND.raw RATE TITLE OUT.vid\n"
                        "  FRAMES.raw: gray8 frames; SOUND.raw: s16le mono, or - for none\n");
        return 2;
    }
    width = atoi(argv[2]);
    height = atoi(argv[3]);
    int fps = atoi(argv[4]), rate = strcmp(argv[5], "-") ? atoi(argv[6]) : 0;
    long sound_size = 0;
    FILE *gray_file = fopen(argv[1], "rb");
    int16_t *sound = rate ? (int16_t *)read_all(argv[5], &sound_size) : NULL;
    if (!gray_file || width < 1 || width > VID_MAX_WIDTH || height < 1 || height > VID_MAX_HEIGHT || fps < 1 ||
        (rate && !sound)) {
        fprintf(stderr, "encode: cannot read the frames or the sound, or a size is wrong\n");
        return 1;
    }
    fseek(gray_file, 0, SEEK_END);
    int frames = (int)(ftell(gray_file) / ((long)width * height));
    fseek(gray_file, 0, SEEK_SET);
    int chunks = (frames + CHUNK_FRAMES - 1) / CHUNK_FRAMES;
    long chunk_samples = (long)CHUNK_FRAMES * rate / fps;
    /* the sound lasts as long as the frames */
    long samples = rate ? (long)frames * rate / fps : 0;
    int16_t *sound_in = calloc(samples + 1, 2), *sound_out = calloc(samples + 1, 2);
    if (rate) memcpy(sound_in, sound, 2 * (sound_size / 2 < samples ? sound_size / 2 : samples));

    long table_bytes = (long)(chunks + 1) * 8;
    Encoder pictures = { 0 }, sounds = { 0 };
    long *picture_at = calloc(chunks + 1, sizeof(long)), *sound_at = calloc(chunks + 1, sizeof(long));

    /* every frame as the decoder will make it, a bit a pixel */
    int packed = (width + 7) / 8;
    uint8_t *made = calloc((size_t)frames * height, packed);
    uint8_t *previous = calloc(width * height, 1), *pixels = malloc(width * height), *g = malloc(width * height);
    Row *before = new_rows(height), *now = new_rows(height), *want = new_rows(1), *trial = new_rows(1);
    Row empty = { 0, NULL };
    VidPictureModel model;
    long same_rows = 0, above_rows = 0;
    for (int frame = 0; frame < frames; frame++) {
        if (frame % CHUNK_FRAMES == 0) {
            if (frame) encoder_end(&pictures);
            picture_at[frame / CHUNK_FRAMES] = (long)pictures.size;
            encoder_start(&pictures);
            vid_fresh((uint16_t *)&model, sizeof model);
            for (int y = 0; y < height; y++) before[y].count = 0;
            memset(previous, 0, width * height);
        }
        if (fread(g, 1, (size_t)width * height, gray_file) != (size_t)width * height) return 1;
        int above = 0, last_move = 0;
        for (int y = 0; y < height; y++) {
            uint8_t *px = pixels + y * width;
            const uint8_t *was = previous + y * width, *in = g + y * width;
            for (int x = 0; x < width; x++)
                px[x] = in[x] >= WHITE_FROM ? 1 : in[x] < BLACK_UNDER ? 0 : frame % CHUNK_FRAMES ? was[x] : in[x] >= 128;
            for (int x = 0; x < width;) {
                int end = x;
                while (end < width && px[end] == px[x]) end++;
                if (end - x < SPECK && x > 0 && end < width && memcmp(px + x, was + x, end - x))
                    memset(px + x, px[x - 1], end - x);
                x = end;
            }
            row_of(px, want);
            if (same_row(want, &before[y])) {
                put_bit(&pictures, &model.same[above], 0);
                copy_row(&now[y], &before[y]);
                above = 0;
                same_rows++;
                continue;
            }
            put_bit(&pictures, &model.same[above], 1);
            const Row *up = y ? &now[y - 1] : &empty;
            /* every reference, on copies, for the one that costs least */
            double best = INFINITY;
            int best_above = 1, best_move = 0;
            for (int choice = -1; choice <= 2 * MOVES; choice++) {
                int from_above = choice < 0, move = choice <= 0 ? 0 : choice & 1 ? (choice + 1) / 2 : -(choice / 2);
                if (!from_above && (y + move < 0 || y + move >= height)) continue;
                Encoder t = trial_of(&pictures);
                VidPictureModel m = model;
                put_row(&t, &m, above, last_move, from_above, move, from_above ? up : &before[y + move], want, trial);
                if (cost(&t) < best) best = cost(&t), best_above = from_above, best_move = move;
            }
            put_row(&pictures, &model, above, last_move, best_above, best_move,
                    best_above ? up : &before[y + best_move], want, &now[y]);
            if (!best_above) last_move = best_move;
            above = 1 + best_above;
            above_rows += best_above;
        }
        for (int y = 0; y < height; y++) {
            uint8_t *out = previous + y * width;
            int colour = 0, x = 0;
            for (int i = 0; i <= now[y].count; i++) {
                int end = i < now[y].count ? now[y].x[i] : width;
                memset(out + x, colour, end - x);
                x = end;
                colour ^= 1;
            }
            pack(out, made + ((size_t)frame * height + y) * packed);
        }
        Row *swap = before;
        before = now;
        now = swap;
    }
    encoder_end(&pictures);
    picture_at[chunks] = (long)pictures.size;

    for (int chunk = 0; chunk < chunks && rate; chunk++) {
        long from = chunk * chunk_samples, count = samples - from < chunk_samples ? samples - from : chunk_samples;
        sound_at[chunk] = (long)sounds.size;
        put_sound_chunk(&sounds, sound_in + from, count, sound_out + from);
    }
    sound_at[chunks] = (long)sounds.size;

    long size = VID_HEADER + table_bytes + (long)pictures.size + (long)sounds.size;
    uint8_t *file = calloc(size, 1);
    memcpy(file, "VID1", 4);
    put16(file + 4, width);
    put16(file + 6, height);
    put16(file + 8, fps);
    put16(file + 10, CHUNK_FRAMES);
    put32(file + 12, frames);
    put32(file + 16, rate);
    put32(file + 20, samples);
    put32(file + 24, chunks);
    strncpy((char *)file + 28, argv[7], 64);
    long pictures_from = VID_HEADER + table_bytes, sounds_from = pictures_from + (long)pictures.size;
    for (int chunk = 0; chunk <= chunks; chunk++) {
        put32(file + VID_HEADER + chunk * 8, pictures_from + picture_at[chunk]);
        put32(file + VID_HEADER + chunk * 8 + 4, sounds_from + sound_at[chunk]);
    }
    memcpy(file + pictures_from, pictures.out, pictures.size);
    if (sounds.size) memcpy(file + sounds_from, sounds.out, sounds.size);

    /* read back with vid.c */
    VidFile v;
    if (!vid_open(&v, file, size)) {
        fprintf(stderr, "encode: vid.c does not open what was made\n");
        return 1;
    }
    VidPicture p = { &v };
    p.changes[0] = calloc((size_t)height * (width + 1), 2);
    p.changes[1] = calloc((size_t)height * (width + 1), 2);
    p.counts[0] = calloc(height, 2);
    p.counts[1] = calloc(height, 2);
    p.slot = calloc(height, 1);
    p.previous_slot = calloc(height, 1);
    p.changed = calloc(height, 1);
    uint8_t *row = malloc(width);
    for (int frame = 0; frame < frames; frame++) {
        if (!vid_picture_next(&p)) {
            fprintf(stderr, "encode: vid.c stops at frame %d\n", frame);
            return 1;
        }
        for (int y = 0; y < height; y++) {
            int count, colour = 0, x = 0;
            const uint16_t *changes = vid_row(&p, y, &count);
            for (int i = 0; i <= count; i++) {
                int end = i < count ? changes[i] : width;
                memset(row + x, colour, end - x);
                x = end;
                colour ^= 1;
            }
            uint8_t bits[VID_MAX_WIDTH / 8];
            pack(row, bits);
            if (memcmp(bits, made + ((size_t)frame * height + y) * packed, packed)) {
                fprintf(stderr, "encode: frame %d row %d differs when read back\n", frame, y);
                return 1;
            }
        }
    }
    if (rate) {
        VidSound s = { &v };
        vid_sound_seek(&s, 0);
        int16_t block[1000];
        long at = 0;
        for (int got; (got = vid_sound_read(&s, block, 1000)) > 0; at += got)
            if (memcmp(block, sound_out + at, got * 2)) {
                fprintf(stderr, "encode: the sound differs when read back, near sample %ld\n", at);
                return 1;
            }
        if (at != samples) {
            fprintf(stderr, "encode: %ld samples read back of %ld\n", at, samples);
            return 1;
        }
    }

    FILE *out = fopen(argv[8], "wb");
    if (!out || fwrite(file, 1, size, out) != (size_t)size || fclose(out)) {
        fprintf(stderr, "encode: cannot write %s\n", argv[8]);
        return 1;
    }
    double signal = 0, noise = 0;
    for (long i = 0; i < samples; i++) {
        signal += (double)sound_in[i] * sound_in[i];
        noise += (double)(sound_in[i] - sound_out[i]) * (sound_in[i] - sound_out[i]);
    }
    printf("%s: %d frames of %dx%d at %d a second in %d chunks, %ld bytes: pictures %zu, sound %zu\n", argv[8],
           frames, width, height, fps, chunks, size, pictures.size, sounds.size);
    printf("rows the same as before %.1f%%, from the row above %.1f%%; sound %.1f dB under, read back the same\n",
           100.0 * same_rows / ((double)frames * height), 100.0 * above_rows / ((double)frames * height),
           noise ? 10 * log10(signal / noise) : 0.0);
    return 0;
}
