/*
 * The sound: the arcade's own sound program, played on the wavetable channels of synth.c.
 *
 * The arcade made its sound with a Namco wavetable chip of three voices, each a frequency, a
 * volume from 0 to 15 and one of eight waveforms of 32 four-bit samples. Its program sets the
 * three anew every 1/60 s from two kinds of table, and this file is that program, table for table:
 *
 * An effect is 8 bytes, played on one voice:
 *
 *   0  bits 4-6: the octave, a shift of the frequency; bits 0-2: the waveform
 *   1  the starting frequency
 *   2  added to the frequency every frame
 *   3  bit 7: turn back at the end of each run, a round trip; bits 0-6: frames a run lasts
 *   4  added to the starting frequency after each round, for the next one
 *   5  how many runs before the effect ends; 0 never ends
 *   6  bits 4-7: how the volume fades; bits 0-3: the volume
 *   7  added to the volume after each round
 *
 * A song is bytes on two voices at once. Most are a note, bits 5-7 its length in frames as a power
 * of two and bits 0-3 its pitch in PM_NOTE_FREQUENCIES, 0 a rest; bit 4 plays it an octave up.
 * Before the notes, F1 sets the waveform, F2 the octave, F3 the volume and F4 how it fades; FF ends
 * it.
 *
 * The tables are the arcade's bytes, as Frédéric Vecoven's commented listing of its sound code
 * gives them (vecoven.com/elec/pacman). A channel of synth.c plays a voice the way the chip did, as
 * the editor's Synth did for the RISC-V version: its wavetable, its frequency and volume set every
 * frame. Its wavetable holds 32 samples of 4 bits, the chip's own format. The chip's eight
 * waveforms were in a PROM; the ones below are drawn with the overtones measured from it, which
 * set their color and their pitch: some hold several cycles in their 32 samples, and sound that
 * many times the frequency the program sets.
 */

/* The arcade's eight waveforms, 32 samples of 0 to 15, the first of each pair in the high half. */
static const uint8_t pm_waveforms[8][16] = {
    /* 0: a sine, for the start music's tune, the eyes and the pop */
    { 0x89, 0xac, 0xde, 0xef, 0xff, 0xee, 0xdc, 0xa9, 0x86, 0x53, 0x21, 0x10, 0x00, 0x11, 0x23, 0x56 },
    /* 1: a sine with its octave and double octave, for the death */
    { 0x8c, 0xef, 0xec, 0xaa, 0xbc, 0xb9, 0x74, 0x35, 0x7a, 0xcb, 0x86, 0x43, 0x45, 0x53, 0x10, 0x13 },
    /* 2: bright, sounding an octave up, for the start music's bass and the dots */
    { 0x8f, 0xec, 0xca, 0x98, 0x76, 0x66, 0x66, 0x66, 0x89, 0x99, 0x99, 0x99, 0x87, 0x65, 0x33, 0x10 },
    /* 3: a sine ringing three octaves up, for the extra life */
    { 0x8d, 0xb8, 0xbd, 0x97, 0xbf, 0xc8, 0x9a, 0x63, 0x7c, 0x95, 0x67, 0x30, 0x48, 0x62, 0x47, 0x42 },
    /* 4: a buzz eight times the frequency, for the frightened ghosts */
    { 0x8f, 0x82, 0x8b, 0x85, 0x7a, 0x85, 0x7a, 0x86, 0x89, 0x85, 0x7a, 0x85, 0x7a, 0x74, 0x7d, 0x80 },
    /* 5: a whine sixteen times the frequency, for a ghost eaten */
    { 0xd3, 0xc3, 0xb4, 0xb4, 0xb4, 0xb4, 0xb3, 0xc3, 0xd2, 0xe1, 0xe0, 0xf0, 0xf0, 0xf0, 0xe1, 0xe2 },
    /* 6: nearly a sine, for the siren and the fruit */
    { 0x8a, 0xcd, 0xee, 0xef, 0xff, 0xee, 0xed, 0xca, 0x85, 0x32, 0x11, 0x10, 0x00, 0x11, 0x12, 0x35 },
    /* 7: a sawtooth at twice the frequency, which Pac-Man does not use */
    { 0x8f, 0xbc, 0xaa, 0x98, 0x87, 0x65, 0x53, 0x40, 0x7f, 0xbc, 0xaa, 0x98, 0x87, 0x65, 0x53, 0x40 },
};

/* A note's pitch in a song, before its octave. */
static const uint8_t PM_NOTE_FREQUENCIES[16] = {
    0x00, 0x57, 0x5c, 0x61, 0x67, 0x6d, 0x74, 0x7b, 0x82, 0x8a, 0x92, 0x9a, 0xa3, 0xad, 0xb8, 0xc3
};

/* The start music: the bass on channel 1 and the tune on channel 2, 256 frames each. */
static const uint8_t pm_music_bass[] = {
    0xf1, 0x02, 0xf2, 0x03, 0xf3, 0x0f, 0xf4, 0x01, 0x82, 0x70, 0x69, 0x82, 0x70, 0x69, 0x83, 0x70,
    0x6a, 0x83, 0x70, 0x6a, 0x82, 0x70, 0x69, 0x82, 0x70, 0x69, 0x89, 0x8b, 0x8d, 0x8e, 0xff
};
static const uint8_t pm_music_melody[] = {
    0xf1, 0x00, 0xf2, 0x02, 0xf3, 0x0f, 0xf4, 0x00, 0x42, 0x50, 0x4e, 0x50, 0x49, 0x50, 0x46, 0x50,
    0x4e, 0x49, 0x70, 0x66, 0x70, 0x43, 0x50, 0x4f, 0x50, 0x4a, 0x50, 0x47, 0x50, 0x4f, 0x4a, 0x70,
    0x67, 0x70, 0x42, 0x50, 0x4e, 0x50, 0x49, 0x50, 0x46, 0x50, 0x4e, 0x49, 0x70, 0x66, 0x70, 0x45,
    0x46, 0x47, 0x50, 0x47, 0x48, 0x49, 0x50, 0x49, 0x4a, 0x4b, 0x50, 0x6e, 0xff
};

/* Channel 1's effect: ten beeps for the extra life. */
static const uint8_t pm_sound_extra_life[8] = { 0x73, 0x20, 0x00, 0x0c, 0x00, 0x0a, 0x1f, 0x00 };

/* Channel 2's: the five sirens, each higher and faster, then the frightened ghosts and the eyes. */
static const uint8_t pm_sound_siren_1[8] = { 0x36, 0x20, 0x04, 0x8c, 0x00, 0x00, 0x06, 0x00 };
static const uint8_t pm_sound_siren_2[8] = { 0x36, 0x28, 0x05, 0x8b, 0x00, 0x00, 0x06, 0x00 };
static const uint8_t pm_sound_siren_3[8] = { 0x36, 0x30, 0x06, 0x8a, 0x00, 0x00, 0x06, 0x00 };
static const uint8_t pm_sound_siren_4[8] = { 0x36, 0x3c, 0x07, 0x89, 0x00, 0x00, 0x06, 0x00 };
static const uint8_t pm_sound_siren_5[8] = { 0x36, 0x48, 0x08, 0x88, 0x00, 0x00, 0x06, 0x00 };
static const uint8_t pm_sound_frightened[8] = { 0x24, 0x00, 0x06, 0x08, 0x00, 0x00, 0x0a, 0x00 };
static const uint8_t pm_sound_eyes[8] = { 0x40, 0x70, 0xfa, 0x10, 0x00, 0x00, 0x0a, 0x00 };

/* Channel 3's: a dot, falling then rising, the fruit, a ghost eaten, the death and the pop. */
static const uint8_t pm_sound_wa[8] = { 0x42, 0x18, 0xfd, 0x06, 0x00, 0x01, 0x0c, 0x00 };
static const uint8_t pm_sound_ka[8] = { 0x42, 0x04, 0x03, 0x06, 0x00, 0x01, 0x0c, 0x00 };
static const uint8_t pm_sound_fruit[8] = { 0x56, 0x0c, 0xff, 0x8c, 0x00, 0x02, 0x0f, 0x00 };
static const uint8_t pm_sound_ghost[8] = { 0x05, 0x00, 0x02, 0x20, 0x00, 0x01, 0x0c, 0x00 };
static const uint8_t pm_sound_death[8] = { 0x41, 0x20, 0xff, 0x86, 0xfe, 0x1c, 0x0f, 0xff };
static const uint8_t pm_sound_pop[8] = { 0x70, 0x00, 0x01, 0x0c, 0x00, 0x01, 0x08, 0x00 };

enum { PM_SILENT, PM_EFFECT, PM_SONG_START, PM_SONG };

/* What a voice is playing, as the arcade keeps it. */
struct PmVoice {
    int playing;
    uint8_t effect[8];   /* the effect's bytes, which it changes as it plays */
    const uint8_t *song; /* the song's next byte */
    uint8_t duration;    /* frames left of the run or the note */
    uint8_t frequency;   /* before the octave */
    uint8_t direction;   /* an effect's half of the round trip; a song's last note */
    uint8_t volume;
    uint8_t fade;        /* how the volume fades: 0 not, 1 to 4 down one every 1, 2, 4 or 8 frames */
    uint8_t waveform, octave, song_volume; /* a song's, from its F1, F2 and F3 */
    int loaded;          /* the waveform in its channel's wavetable, -1 none */
};
static struct PmVoice pm_voices[PM_CHANNEL_COUNT];
static const uint8_t *pm_background_sound; /* what pm_choose_background last started on channel 2 */
static int pm_chomp_turn;                  /* wa or ka next */
static unsigned pm_sound_frame;            /* frames of sound, which the fades count */

/* The sirens, and how many dots must be eaten for each after the first: the arcade's numbers. */
static const uint8_t *const pm_sirens[5] = {
    pm_sound_siren_1, pm_sound_siren_2, pm_sound_siren_3, pm_sound_siren_4, pm_sound_siren_5
};
static const int pm_siren_dots[4] = { 116, 180, 212, 228 };

/*
 * The three channels of synth.c as the chip's three voices: each plays its wavetable at its volume
 * alone, as the chip's did. None has a wavetable yet.
 */
static void pm_init_sounds(void) {
    for (int i = 0; i < PM_CHANNEL_COUNT; i++) pm_voices[i].loaded = -1;
}

/* The channel drops what it was playing and plays `effect` from this frame on. */
static void pm_play_sound(int channel, const uint8_t *effect) {
    struct PmVoice *voice = &pm_voices[channel];
    for (int i = 0; i < 8; i++) voice->effect[i] = effect[i];
    voice->playing = PM_EFFECT;
    voice->duration = effect[3] & 0x7f;
    voice->frequency = effect[1];
    voice->fade = effect[6] >> 4;
    voice->volume = effect[6];
    voice->direction = 0;
}

/* The channel drops what it was playing and plays `song` from this frame on. */
static void pm_play_song(int channel, const uint8_t *song) {
    struct PmVoice *voice = &pm_voices[channel];
    voice->playing = PM_SONG_START;
    voice->song = song;
    voice->waveform = voice->octave = voice->song_volume = voice->fade = 0;
}

/* Every channel falls silent at once. */
static void pm_stop_sounds(void) {
    for (int i = 0; i < PM_CHANNEL_COUNT; i++) pm_voices[i].playing = PM_SILENT;
    pm_background_sound = 0;
}

/* A dot or an energizer eaten. */
static void pm_play_chomp(void) {
    pm_chomp_turn = !pm_chomp_turn;
    pm_play_sound(PM_CHANNEL_3, pm_chomp_turn ? pm_sound_wa : pm_sound_ka);
}

/*
 * What plays on channel 2 under the game: the eyes while a ghost goes home, or the frightened sound
 * while one is blue, or the siren of the dots eaten. Nothing outside play, and what was playing
 * plays on while the game stands still after a ghost is eaten. A change starts the new effect.
 */
static void pm_choose_background(void) {
    if (pm_game_state == PM_EAT_GHOST) return;
    const uint8_t *effect = 0;
    if (pm_game_state == PM_PLAY) {
        int siren = 0;
        while (siren < 4 && pm_dots_eaten >= pm_siren_dots[siren]) siren++;
        effect = pm_sirens[siren];
        for (int i = 0; i < 4; i++) {
            if (pm_ghosts[i].state >= PM_EYES) {
                effect = pm_sound_eyes;
                break;
            }
            if (pm_ghosts[i].fright) effect = pm_sound_frightened;
        }
    }
    if (effect == pm_background_sound) return;
    pm_background_sound = effect;
    if (effect) pm_play_sound(PM_CHANNEL_2, effect);
    else pm_voices[PM_CHANNEL_2].playing = PM_SILENT;
}

/* The volume this frame, which fades by one every 1, 2, 4 or 8 frames as `fade` says. */
static int pm_fade_volume(struct PmVoice *voice) {
    static const uint8_t every[5] = { 0, 0, 1, 3, 7 };
    if (voice->fade == 0 || voice->fade > 4 || (pm_sound_frame & every[voice->fade])) {
        return voice->volume;
    }
    if (voice->volume & 15) voice->volume = (voice->volume & 15) - 1;
    return voice->volume & 15;
}

/* One frame of an effect: the arcade's routine at 0x2e6e. Answers the frequency, 0 once it ends. */
static int pm_step_effect(struct PmVoice *voice) {
    uint8_t *e = voice->effect;
    if (--voice->duration == 0) {
        if (e[5] && --e[5] == 0) {
            voice->playing = PM_SILENT;
            return 0;
        }
        voice->duration = e[3] & 0x7f;
        int next_round = 1;
        if (e[3] & 0x80) {
            e[2] = -e[2]; /* back the other way */
            next_round = voice->direction;
            voice->direction ^= 1;
        }
        if (next_round) {
            e[1] += e[4];
            voice->frequency = e[1];
            e[6] += e[7];
            voice->volume = e[6];
        }
    }
    voice->frequency += e[2];
    return voice->frequency << ((e[0] >> 4) & 7);
}

/* One frame of a song: the arcade's routine at 0x2d44. Answers the frequency, 0 for a rest. */
static int pm_step_song(struct PmVoice *voice) {
    if (voice->playing == PM_SONG_START || --voice->duration == 0) {
        voice->playing = PM_SONG;
        uint8_t note;
        for (;;) {
            note = *voice->song++;
            if (note < 0xf0) break;
            switch (note) {
            case 0xf1: voice->waveform = *voice->song++; break;
            case 0xf2: voice->octave = *voice->song++; break;
            case 0xf3: voice->song_volume = *voice->song++; break;
            case 0xf4: voice->fade = *voice->song++; break;
            default: /* 0xff: the song is over */
                voice->playing = PM_SILENT;
                return 0;
            }
        }
        if (note & 0x1f) {
            voice->direction = note;
            voice->frequency = PM_NOTE_FREQUENCIES[note & 0x0f];
        }
        voice->volume = voice->song_volume;
        voice->duration = 1 << (note >> 5);
    }
    int octave = voice->octave + ((voice->direction & 0x10) ? 1 : 0);
    return voice->frequency << octave;
}

/*
 * One frame of one voice on its channel. The chip ran at 96,000 samples a second with a
 * 20-bit phase counter, so a frequency f sounds at f * 96000 / 2^20 Hz on voice 1. Voices 2 and 3
 * hold theirs 4 bits higher up the counter, 16 times f.
 */
static void pm_play_voice(int channel) {
    struct PmVoice *voice = &pm_voices[channel];
    unsigned frequency = 0;
    int volume = 0, waveform = voice->loaded;
    if (voice->playing == PM_EFFECT) {
        waveform = voice->effect[0] & 7;
        frequency = pm_step_effect(voice);
    } else if (voice->playing != PM_SILENT) {
        frequency = pm_step_song(voice);
        waveform = voice->waveform & 7; /* after the step: a song sets it in its first bytes */
    }
    if (voice->playing != PM_SILENT) volume = pm_fade_volume(voice);
    unsigned hz = channel == PM_CHANNEL_1 ? (frequency * 375 + 2048) >> 12 /* * 96000 / 2^20 */
                                       : (frequency * 375 + 128) >> 8;  /* * 96000 / 2^16 */
    pm_channels[channel].frequency = (unsigned short)hz;
    pm_channels[channel].volume = (volume & 15) * 8;
    if (waveform != voice->loaded && waveform >= 0) {
        memcpy(pm_channels[channel].table, pm_waveforms[waveform], 16);
        voice->loaded = waveform;
    }
}

/*
 * One frame of sound on every channel, then its samples. A paused game is silent, and picks up
 * where it was.
 */
static void pm_update_sounds(void) {
    if (pm_paused) {
        for (int i = 0; i < PM_CHANNEL_COUNT; i++) pm_channels[i].volume = 0;
    } else {
        pm_sound_frame++;
        pm_choose_background();
        for (int i = 0; i < PM_CHANNEL_COUNT; i++) pm_play_voice(i);
    }
    pm_synth_render();
}
