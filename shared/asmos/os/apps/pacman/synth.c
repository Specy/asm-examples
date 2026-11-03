/*
 * The part of the editor's Synth that the RISC-V version played its sound on, made here: three
 * wavetable channels, each a frequency in Hz, a volume from 0 to 127 and a wavetable of 32
 * four-bit samples, which sound.c sets every frame as it set the Synth's registers. After each
 * frame pm_synth_render turns them into that frame's 1/60 s of 16-bit samples, 22,050 a second,
 * and pm_pcm_flush writes what was made to /dev/dsp once a tick.
 *
 * As on the Synth, a channel's phase counter runs through its 32 samples once a period, and its
 * frequency changes without restarting it; here the counter is 32 bits and its top five bits are
 * the sample. Nothing smooths the steps between samples, on the Synth or here.
 */

enum {
    PM_RATE = 22050,
    PM_FRAME_SAMPLES = 368,   /* a frame's samples, at most */
    PM_QUEUE = 4096,          /* samples made and not yet written */
    PM_FRAGMENTS = 6,         /* how many fragments os_pcm_open asks /dev/dsp for */
    PM_QUIET_FRAMES = 30      /* silent frames after which /dev/dsp has played everything */
};

struct PmChannel {
    unsigned frequency;       /* Hz, 0 to 65,535 */
    int volume;               /* 0 to 127 */
    uint8_t table[16];        /* 32 samples of 0 to 15, the first of each pair in the high half */
    uint32_t phase;
};
static struct PmChannel pm_channels[PM_CHANNEL_COUNT];

static int pm_sound_on = 1;           /* the Sound item of the Game menu */
static int pm_pcm_ready;              /* /dev/dsp is open for this process */
static int pm_pcm_capacity;           /* samples its buffer holds: all its space when it opened */
static int16_t pm_samples[PM_QUEUE];
static int pm_sample_count;
static int pm_sample_rest;            /* 1/60ths of a sample carried to the next frame */
static int pm_quiet_frames;           /* frames in a row with every channel silent */
static long pm_samples_written;       /* since os_pcm_open */
static const int16_t pm_silence[512];

/*
 * One frame of the three channels, 367 or 368 samples, onto the end of pm_samples; with the Sound
 * off, a frame of silence. Silence after half a second of it is not made at all: the device has
 * played everything by then and stays quiet without it.
 */
static void pm_synth_render(void) {
    pm_sample_rest += PM_RATE;
    int count = pm_sample_rest / 60;
    pm_sample_rest -= count * 60;
    int loud = 0;
    for (int c = 0; c < PM_CHANNEL_COUNT; c++) loud |= pm_channels[c].volume && pm_channels[c].frequency;
    loud &= pm_sound_on;
    pm_quiet_frames = loud ? 0 : min_int(pm_quiet_frames + 1, PM_QUIET_FRAMES + 1);
    if (!pm_pcm_ready || pm_quiet_frames > PM_QUIET_FRAMES) return;
    if (pm_sample_count + count > PM_QUEUE) {
        /* nothing was written for a long while: the oldest samples go */
        int drop = pm_sample_count + count - PM_QUEUE;
        memmove(pm_samples, pm_samples + drop, (pm_sample_count - drop) * sizeof *pm_samples);
        pm_sample_count -= drop;
    }
    int16_t *out = pm_samples + pm_sample_count;
    memset(out, 0, count * sizeof *out);
    pm_sample_count += count;
    for (int c = 0; c < PM_CHANNEL_COUNT && loud; c++) {
        struct PmChannel *channel = &pm_channels[c];
        if (!channel->volume || !channel->frequency) continue;
        /* the 32 samples as levels about 0, at this volume: three channels at most add to 22,860 */
        int16_t level[32];
        for (int i = 0; i < 32; i++) {
            int sample = i & 1 ? channel->table[i / 2] & 15 : channel->table[i / 2] >> 4;
            level[i] = (int16_t)((2 * sample - 15) * channel->volume * 4);
        }
        uint32_t phase = channel->phase, step = (uint32_t)(((uint64_t)channel->frequency << 32) / PM_RATE);
        for (int i = 0; i < count; i++) {
            out[i] += level[phase >> 27];
            phase += step;
        }
        channel->phase = phase;
    }
}

static void pm_pcm_drop(int count) {
    count = min_int(count, pm_sample_count);
    memmove(pm_samples, pm_samples + count, (pm_sample_count - count) * sizeof *pm_samples);
    pm_sample_count -= count;
}

static void pm_pcm_write(const int16_t *samples, int count) {
    pm_samples_written += os_pcm_write(samples, count);
}

/*
 * The samples made since the last tick into /dev/dsp. It keeps what is written until a whole
 * fragment, about a tenth of a second, has come, and plays fragment after fragment; so that the
 * next is there when one ends, however late the next tick is, a fragment and a tick's most frames
 * more are kept waiting in it, which is how far the sound is behind the picture. When it has run
 * dry, as when a sound starts after a silence, silence goes first to make them up; samples that
 * would put the sound a fragment further behind are dropped.
 */
static void pm_pcm_flush(void) {
    if (!pm_pcm_ready || !pm_sample_count) return;
    int fragment = max_int(pm_pcm_capacity / PM_FRAGMENTS, 1);
    int cushion = fragment + PM_MOST_FRAMES * PM_FRAME_SAMPLES;
    int waiting = pm_pcm_capacity - os_pcm_space();
    if (waiting == 0) {
        /* nothing playing: what waits for its fragment to fill is what was written past the last */
        int padding = cushion - (int)(pm_samples_written % fragment) - pm_sample_count;
        while (padding > 0) {
            int count = min_int(padding, (int)(sizeof pm_silence / sizeof *pm_silence));
            pm_pcm_write(pm_silence, count);
            padding -= count;
        }
    } else if (waiting + pm_sample_count > cushion + fragment) {
        pm_pcm_drop(waiting + pm_sample_count - cushion);
    }
    long before = pm_samples_written;
    pm_pcm_write(pm_samples, pm_sample_count);
    pm_pcm_drop((int)(pm_samples_written - before));
}
