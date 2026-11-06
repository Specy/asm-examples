/* Original short melodies and effects, stored as (frames, Hz) pairs. A zero duration ends a cue. */
static const uint16_t music_bass[] = { 10, 196, 10, 247, 10, 294, 14, 196, 8, 220, 12, 294, 0, 0 };
static const uint16_t music_melody[] = { 6, 587, 6, 740, 8, 880, 4, 740, 8, 659, 8, 784, 12, 988, 12, 880, 0, 0 };
static const uint16_t sound_dot_a[] = { 2, 960, 2, 1120, 0, 0 };
static const uint16_t sound_dot_b[] = { 2, 840, 2, 1040, 0, 0 };
static const uint16_t sound_bonus[] = { 5, 659, 5, 830, 9, 1109, 0, 0 };
static const uint16_t sound_enemy[] = { 4, 392, 4, 587, 7, 784, 0, 0 };
static const uint16_t sound_death[] = { 10, 440, 9, 370, 9, 294, 12, 220, 14, 147, 0, 0 };
static const uint16_t sound_pop[] = { 5, 98, 0, 0 };
static const uint16_t sound_extra_life[] = { 7, 523, 7, 659, 7, 784, 14, 1047, 0, 0 };

/* Two original sixteen-step loops. The melody and bass use separate voices,
 * leaving the third voice free for dots, bonuses and catching enemies. */
static const uint16_t roam_melody[16] = { 392, 0, 494, 587, 440, 0, 523, 659, 392, 494, 587, 740, 659, 523, 440, 0 };
static const uint16_t roam_bass[16] = { 98, 98, 98, 98, 110, 110, 110, 110, 98, 98, 98, 98, 131, 131, 131, 131 };
static const uint16_t power_melody[16] = { 587, 740, 880, 740, 659, 830, 988, 830, 587, 740, 880, 1175, 988, 830, 740, 659 };
static const uint16_t power_bass[16] = { 147, 147, 196, 196, 165, 165, 220, 220, 147, 147, 196, 196, 175, 175, 247, 247 };
static int background_mode, background_step, background_frames;

struct Voice { const uint16_t *next; int frames, hz; };
static struct Voice voices[CHANNEL_COUNT];
static int dot_phase;
static void init_sounds(void) {
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        SIM_SYNTH8(i, SIM_SYNTH_WAVEFORM) = SIM_SYNTH_TRIANGLE;
        SIM_SYNTH8(i, SIM_SYNTH_SUSTAIN) = 127;
        SIM_SYNTH8(i, SIM_SYNTH_VOLUME) = 0;
        SIM_SYNTH8(i, SIM_SYNTH_CONTROL) = SIM_SYNTH_GATE;
    }
}
static void play_sound(int channel, const uint16_t *cue) {
    voices[channel].next = cue;
    voices[channel].frames = 0;
}
static void play_song(int channel, const uint16_t *cue) { play_sound(channel, cue); }
static void stop_sounds(void) {
    background_mode = background_step = background_frames = 0;
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        voices[i].next = 0;
        voices[i].frames = 0;
        SIM_SYNTH8(i, SIM_SYNTH_VOLUME) = 0;
    }
}
static void play_chomp(void) { play_sound(CHANNEL_3, (dot_phase++ & 1) ? sound_dot_b : sound_dot_a); }
static void update_sounds(void) {
    if (!paused) {
        int mode = (game_state == PLAY || game_state == EAT_ENEMY) ? (fright_timer ? 2 : 1) : 0;
        if (mode != background_mode) {
            background_mode = mode;
            background_step = 0;
            background_frames = mode == 2 ? 6 : mode == 1 ? 10 : 0;
        }
    }
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        struct Voice *voice = &voices[i];
        if (paused) { SIM_SYNTH8(i, SIM_SYNTH_VOLUME) = 0; continue; }
        if (voice->next && !voice->frames) {
            if (!voice->next[0]) voice->next = 0;
            else {
                voice->frames = voice->next[0];
                voice->hz = voice->next[1];
                voice->next += 2;
            }
        }
        int hz = 0, volume = 0;
        if (voice->next) { hz = voice->hz; volume = 64; voice->frames--; }
        else if (background_mode && i < CHANNEL_3) {
            const uint16_t *notes = background_mode == 2
                ? (i == CHANNEL_1 ? power_bass : power_melody)
                : (i == CHANNEL_1 ? roam_bass : roam_melody);
            hz = notes[background_step];
            if (hz) volume = background_mode == 2
                ? (i == CHANNEL_1 ? 28 : 44)
                : (i == CHANNEL_1 ? 24 : 32);
        }
        SIM_SYNTH16(i, SIM_SYNTH_FREQUENCY) = (unsigned short)hz;
        SIM_SYNTH8(i, SIM_SYNTH_VOLUME) = (unsigned char)volume;
    }
    if (!paused && background_mode && --background_frames == 0) {
        background_step = (background_step + 1) & 15;
        background_frames = background_mode == 2 ? 6 : 10;
    }
}
