/*
 * No sound. MARS and RARS play MIDI notes of a length given in advance (syscall 31) and the editor
 * adds a Synth, but neither is the MIDI port nor the sampled sound asmOS's sound is written for, so
 * asmOS is silent here: the music, Pac-Man and Blocks play without it, as on a machine without a
 * sound card.
 */

int platform_pcm_open(int rate) {
    (void)rate;
    return -1;
}
int platform_pcm_space(void) { return 0; }
int platform_pcm_write(const int16_t *samples, int count) {
    (void)samples, (void)count;
    return 0;
}
void platform_pcm_close(void) {}

int platform_midi_open(void) { return -1; }
void platform_midi_write(const unsigned char *bytes, int length) { (void)bytes, (void)length; }
