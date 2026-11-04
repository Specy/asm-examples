/*
 * DOOM's sound on asmOS, in place of doomgeneric's i_sdlsound.c and i_sdlmusic.c.
 *
 * The effects are the IWAD's DS lumps, 8-bit samples at 11,025 a second, mixed here into 16-bit
 * samples for /dev/dsp through os_pcm_write. The kernel's buffer holds about a quarter of a second
 * and a frame of DOOM takes longer, so the buffer is topped up whenever DOOM looks at the clock,
 * I_StartTic between the renderer's phases among them: I_SoundPump. When nothing plays nothing is
 * written, so that the buffer runs empty and the next effect is heard at once.
 *
 * The music is the IWAD's MUS lumps, read once into notes with their times and lengths. The kernel
 * plays a note queued with os_midi_note at its time, whatever DOOM is doing, so each pump queues
 * the notes of the next moments, DOOM_MUSIC_AHEAD milliseconds of them, and the frame rate does
 * not shake the rhythm.
 */
#include "doomgeneric/doomtype.h"
#include "doomgeneric/i_sound.h"
#include "doomgeneric/w_wad.h"
#include "doomgeneric/z_zone.h"
#include "asmos_x86.h"

/* DOOM_MIX: samples mixed at a time, more than the buffer holds, so one write usually fills it;
 * DOOM_PUMP_MS: the least time between two top-ups, each a few system calls, which cost time */
enum { DOOM_RATE = 11025, DOOM_CHANNELS = 16, DOOM_MIX = 4096, DOOM_PUMP_MS = 20 };

void I_SoundPump(void);
static void doom_pump_effects(void);

/* doom_simd.asm's mixer is used while Game > Fast Drawing is on, on x86 (asmos_x86.h) */
extern long doom_now_ms;    /* i_asmos.c: os_now_ms as DOOM last read it */

/* ------------------------------------------------------------------ the effects */

typedef struct {
    const byte *samples;        /* NULL: silent */
    unsigned length;            /* samples */
    unsigned position, step;    /* 16.16: a step of 1.0 when the effect is at DOOM_RATE */
    int volume;                 /* 0 to 127 */
} DoomVoice;

static DoomVoice doom_voices[DOOM_CHANNELS];
static boolean doom_pcm_open, doom_sfx_prefix;
static int doom_pcm_capacity;   /* samples the buffer holds, empty */
static int doom_silence_owed;   /* samples of silence after the last effect, to flush the buffer */

/* An effect's samples in its lump: a DMX header of 8 bytes, then 16 bytes DMX skips at either end. */
static boolean doom_sfx_samples(sfxinfo_t *sfx, const byte **samples, unsigned *length,
                                unsigned *rate) {
    if (sfx->lumpnum < 0) return false;
    int size = W_LumpLength(sfx->lumpnum);
    const byte *data = W_CacheLumpNum(sfx->lumpnum, PU_STATIC);
    if (size < 8 || data[0] != 3 || data[1] != 0) return false;
    unsigned count = read_le32(data + 4);
    if (count > (unsigned)size - 8 || count <= 48) return false;
    *rate = read_le16(data + 2);
    *samples = data + 8 + 16;
    *length = count - 32;
    return *rate > 0;
}

static boolean doom_sound_init(boolean use_sfx_prefix) {
    doom_sfx_prefix = use_sfx_prefix;
    /* another process, Pac-Man say, may have the device: DOOM then plays without effects */
    doom_pcm_open = os_pcm_open(DOOM_RATE) == 0;
    doom_pcm_capacity = doom_pcm_open ? os_pcm_space() : 0;
    return doom_pcm_open;
}

static void doom_sound_shutdown(void) {
    if (doom_pcm_open) os_pcm_close();
    doom_pcm_open = false;
}

static int doom_sfx_lump(sfxinfo_t *sfx) {
    char name[9];
    if (sfx->link) sfx = sfx->link;
    snprintf(name, sizeof name, doom_sfx_prefix ? "ds%s" : "%s", sfx->name);
    return W_CheckNumForName(name);
}

static void doom_sound_params(int channel, int volume, int separation) {
    (void)separation;           /* the device is mono */
    if (channel >= 0 && channel < DOOM_CHANNELS) doom_voices[channel].volume = volume;
}

static int doom_start_sound(sfxinfo_t *sfx, int channel, int volume, int separation) {
    const byte *samples;
    unsigned length, rate;
    if (!doom_pcm_open || channel < 0 || channel >= DOOM_CHANNELS) return -1;
    if (!doom_sfx_samples(sfx, &samples, &length, &rate)) return -1;
    DoomVoice *voice = &doom_voices[channel];
    voice->samples = samples;
    voice->length = length;
    voice->position = 0;
    voice->step = (unsigned)(((unsigned long)rate << 16) / DOOM_RATE);
    doom_sound_params(channel, volume, separation);
    doom_pump_effects();        /* heard at once, whenever the last top-up was */
    return channel;
}

static void doom_stop_sound(int channel) {
    if (channel >= 0 && channel < DOOM_CHANNELS) doom_voices[channel].samples = NULL;
}

static boolean doom_sound_playing(int channel) {
    return channel >= 0 && channel < DOOM_CHANNELS && doom_voices[channel].samples != NULL;
}

/* `count` samples of every voice playing, added up into `mix`, which is all zero before: answers
 * whether any played. With Game > Fast Drawing on, on x86, doom_simd.asm adds eight samples at a
 * time. */
static boolean doom_mix(int *mix, int count) {
    boolean any = false;
    for (int c = 0; c < DOOM_CHANNELS; c++) {
        DoomVoice *voice = &doom_voices[c];
        if (!voice->samples) continue;
        any = true;
        const byte *samples = voice->samples;
        int volume = voice->volume * 3;     /* a full sample at full volume: 3/4 of the range */
        unsigned position = voice->position, step = voice->step, end = voice->length << 16;
        int i = 0;
        if (step == 1 << 16) {              /* the IWAD's own rate, nearly every effect */
            unsigned at = position >> 16, left = voice->length - at;
            int n = (unsigned)count < left ? count : (int)left;
#if DOOM_X86
            if (doom_simd) doom_mix_add(mix, samples + at, n, volume);
            else
#endif
            for (; i < n; i++) mix[i] += (samples[at + i] - 128) * volume;
            position += (unsigned)n << 16;
        } else {
            for (; i < count && position < end; i++, position += step)
                mix[i] += (samples[position >> 16] - 128) * volume;
        }
        voice->position = position;
        if (position >= end) voice->samples = NULL;
    }
    return any;
}

/* As much sound as the buffer has room for, while an effect plays and a moment after. */
static void doom_pump_effects(void) {
    static int mix[DOOM_MIX];
    static int16_t out[DOOM_MIX];
    if (!doom_pcm_open) return;
    for (;;) {
        boolean playing = false;
        for (int c = 0; c < DOOM_CHANNELS && !playing; c++) playing = doom_voices[c].samples != NULL;
        if (!playing && doom_silence_owed <= 0) return;
        int count = os_pcm_space();
        if (count > DOOM_MIX) count = DOOM_MIX;
        if (count <= 0) return;
        if (doom_mix(mix, count)) {
            /* a whole buffer of silence after the last effect, which pushes its end out of
             * /dev/dsp's last fragment, kept until a fragment is full */
            doom_silence_owed = doom_pcm_capacity;
        } else {
            doom_silence_owed -= count;
        }
        /* the mix as 16-bit samples, and back to zero for the next */
#if DOOM_X86
        if (doom_simd) doom_mix_out(out, mix, count);
        else
#endif
        for (int i = 0; i < count; i++) {
            int s = mix[i];
            out[i] = (int16_t)(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
            mix[i] = 0;
        }
        os_pcm_write(out, count);
    }
}

static snddevice_t doom_sound_devices[] = {
    SNDDEVICE_SB, SNDDEVICE_PAS, SNDDEVICE_GUS, SNDDEVICE_WAVEBLASTER, SNDDEVICE_SOUNDCANVAS,
    SNDDEVICE_AWE32, SNDDEVICE_ADLIB, SNDDEVICE_GENMIDI, SNDDEVICE_PCSPEAKER,
};

sound_module_t asmos_sound_module = {
    doom_sound_devices, sizeof doom_sound_devices / sizeof *doom_sound_devices,
    doom_sound_init, doom_sound_shutdown, doom_sfx_lump, I_SoundPump, doom_sound_params,
    doom_start_sound, doom_stop_sound, doom_sound_playing, NULL,
};

/* ------------------------------------------------------------------ the music */

/* A note, or another MIDI message. */
enum { DOOM_UNENDED = 0x7fffffff };
typedef struct {
    int time;                   /* ms from the song's start */
    int length;                 /* a note's ms; -1 a message of two data bytes, -2 of one */
    byte status, data1, data2;
} DoomEvent;

typedef struct {
    DoomEvent *events;
    int count;
    int duration;               /* ms, where it loops */
} DoomSong;

enum { DOOM_MUSIC_AHEAD = 700, DOOM_QUEUE_MAX = 180 };

static DoomSong *doom_song;     /* playing, or paused */
static int doom_next;           /* its next event to queue */
static long doom_song_start;    /* os_now_ms of its time 0, this time round */
static boolean doom_looping, doom_paused;
static int doom_music_volume = 127;
/* when each MIDI message the kernel holds for DOOM goes out, so as not to overflow its queue,
 * which would lose a note's end */
static long doom_queued[DOOM_QUEUE_MAX];
static int doom_queued_count;

/* MUS's channel 15 is the drums, MIDI's 9; the ones from 9 on move up one. */
static int doom_mus_channel(int channel) {
    return channel == 15 ? 9 : channel < 9 ? channel : channel + 1;
}

/* MUS's controllers 1 to 9 as MIDI's: bank, modulation, volume, pan, expression, reverb, chorus,
 * sustain, soft; and its system events 10 to 14. */
static const byte doom_mus_controllers[15] = {
    0, 0, 1, 7, 10, 11, 91, 93, 64, 67, 120, 123, 126, 127, 121,
};

static int doom_mus_byte(const byte *data, int length, int *at) {
    return *at < length ? data[(*at)++] : -1;
}

/* A MUS lump read into notes and messages at their times: NULL for anything else, MIDI too. */
static DoomSong *doom_read_mus(const byte *data, int length) {
    if (length < 16 || memcmp(data, "MUS\x1a", 4) != 0) return NULL;
    int at = (int)read_le16(data + 6), capacity = 256, count = 0;
    DoomEvent *events = malloc(capacity * sizeof *events);
    int open[16][128];          /* each sounding note's event, or -1 */
    byte velocity[16];
    long ticks = 0;             /* MUS counts 140 a second */
    memset(open, -1, sizeof open);
    memset(velocity, 100, sizeof velocity);
    for (;;) {
        int event = doom_mus_byte(data, length, &at);
        if (event < 0 || (event >> 4 & 7) == 6) break;   /* the end of the score */
        int channel = doom_mus_channel(event & 15), type = event >> 4 & 7;
        int time = (int)(ticks * 1000 / 140);
        if (count + 1 >= capacity) {
            capacity *= 2;
            events = realloc(events, capacity * sizeof *events);
        }
        DoomEvent *e = &events[count];
        e->time = time;
        e->length = -1;
        e->data2 = 0;
        int a = 0, b = 0, keep = 1;
        switch (type) {
        case 0:                 /* a note's end */
            a = doom_mus_byte(data, length, &at) & 127;
            if (open[event & 15][a] >= 0)
                events[open[event & 15][a]].length = time - events[open[event & 15][a]].time;
            open[event & 15][a] = -1;
            keep = 0;
            break;
        case 1:                 /* a note, perhaps with its velocity */
            a = doom_mus_byte(data, length, &at);
            if (a & 128) velocity[event & 15] = doom_mus_byte(data, length, &at) & 127;
            a &= 127;
            e->status = (byte)channel;
            e->data1 = (byte)a;
            e->data2 = velocity[event & 15];
            e->length = DOOM_UNENDED;
            open[event & 15][a] = count;
            break;
        case 2:                 /* the pitch bend, 128 the middle */
            b = doom_mus_byte(data, length, &at) << 6;
            e->status = (byte)(0xe0 | channel);
            e->data1 = b & 127;
            e->data2 = b >> 7 & 127;
            break;
        case 3:                 /* a system event: all notes off and the like */
            a = doom_mus_byte(data, length, &at);
            keep = a >= 10 && a <= 14;
            e->status = (byte)(0xb0 | channel);
            e->data1 = keep ? doom_mus_controllers[a] : 0;
            break;
        case 4:                 /* a controller, 0 the instrument */
            a = doom_mus_byte(data, length, &at);
            b = doom_mus_byte(data, length, &at) & 127;
            if (a == 0) {
                e->status = (byte)(0xc0 | channel);
                e->data1 = (byte)b;
                e->length = -2;  /* a message of one data byte */
            } else if (a > 0 && a < 10) {
                e->status = (byte)(0xb0 | channel);
                e->data1 = doom_mus_controllers[a];
                e->data2 = (byte)b;
            } else {
                keep = 0;
            }
            break;
        default:                /* 5, the end of a measure, and 7, nothing */
            keep = 0;
        }
        if (keep) count++;
        if (event & 128) {      /* a delay follows, 7 bits a byte */
            long delay = 0;
            int d;
            do {
                d = doom_mus_byte(data, length, &at);
                delay = delay << 7 | (d & 127);
            } while (d >= 128);
            ticks += delay;
        }
    }
    int end = (int)(ticks * 1000 / 140);
    for (int i = 0; i < count; i++)     /* a note never ended lasts to the song's end */
        if (events[i].length == DOOM_UNENDED) events[i].length = end - events[i].time;
    DoomSong *song = malloc(sizeof *song);
    song->events = events;
    song->count = count;
    song->duration = end;
    return song;
}

static void doom_notes_off(void) {
    for (int channel = 0; channel < 16; channel++) {
        byte off[6] = { (byte)(0xb0 | channel), 123, 0, (byte)(0xb0 | channel), 64, 0 };
        os_midi_send(off, 6);
    }
}

/* The notes of the next DOOM_MUSIC_AHEAD ms into the kernel's queue, the other messages at once. */
static void doom_pump_music(void) {
    if (!doom_song || doom_paused || doom_song->count == 0) return;
    long now = doom_now_ms;
    int kept = 0;
    for (int i = 0; i < doom_queued_count; i++)
        if (doom_queued[i] > now) doom_queued[kept++] = doom_queued[i];
    doom_queued_count = kept;
    for (;;) {
        if (doom_next == doom_song->count) {
            if (!doom_looping || doom_song->duration <= 0) return;
            doom_next = 0;
            doom_song_start += doom_song->duration;
        }
        DoomEvent *e = &doom_song->events[doom_next];
        long due = doom_song_start + e->time;
        if (due > now + DOOM_MUSIC_AHEAD) return;
        if (e->length >= 0) {
            if (doom_queued_count + 2 > DOOM_QUEUE_MAX) return;
            int start = (int)(due - now), velocity = e->data2 * doom_music_volume / 127;
            if (start < 0) start = 0;  /* late: DOOM was busy */
            if (velocity > 0) {
                os_midi_note(e->status, e->data1, velocity, start, e->length > 0 ? e->length : 1);
                doom_queued[doom_queued_count++] = now + start;
                doom_queued[doom_queued_count++] = now + start + e->length;
            }
        } else {
            byte message[3] = { e->status, e->data1, e->data2 };
            os_midi_send(message, e->length == -2 ? 2 : 3);
        }
        doom_next++;
    }
}

static boolean doom_music_init(void) {
    return true;
}

static void doom_music_shutdown(void) {
    doom_song = NULL;
    doom_notes_off();
}

static void doom_set_music_volume(int volume) {
    doom_music_volume = volume < 0 ? 0 : volume > 127 ? 127 : volume;
}

static void doom_pause_music(void) {
    if (doom_paused || !doom_song) return;
    doom_paused = true;
    doom_song_start -= os_now_ms();     /* kept as the time already played, negated */
    doom_notes_off();
}

static void doom_resume_music(void) {
    if (!doom_paused) return;
    doom_paused = false;
    doom_song_start += os_now_ms();
}

static void *doom_register_song(void *data, int length) {
    return doom_read_mus(data, length);
}

static void doom_unregister_song(void *handle) {
    DoomSong *song = handle;
    if (!song) return;
    if (song == doom_song) doom_song = NULL;
    free(song->events);
    free(song);
}

static void doom_play_song(void *handle, boolean looping) {
    doom_notes_off();
    doom_song = handle;
    doom_next = 0;
    doom_looping = looping;
    doom_paused = false;
    doom_song_start = os_now_ms() + 50;
    for (int channel = 0; channel < 16; channel++) {      /* the controllers as they start */
        byte reset[3] = { (byte)(0xb0 | channel), 121, 0 };
        os_midi_send(reset, 3);
    }
    doom_pump_music();
}

static void doom_stop_song(void) {
    doom_song = NULL;
    doom_notes_off();
}

static boolean doom_music_playing(void) {
    return doom_song != NULL && !doom_paused;
}

music_module_t asmos_music_module = {
    NULL, 0, doom_music_init, doom_music_shutdown, doom_set_music_volume, doom_pause_music,
    doom_resume_music, doom_register_song, doom_unregister_song, doom_play_song, doom_stop_song,
    doom_music_playing, doom_pump_music,
};

/* ------------------------------------------------------------------ both */

/* DOOM calls it many times a frame; it does its work at most once every DOOM_PUMP_MS. */
void I_SoundPump(void) {
    static long last;
    if (doom_now_ms - last < DOOM_PUMP_MS && doom_now_ms >= last) return;
    last = doom_now_ms;
    doom_pump_effects();
    doom_pump_music();
}
