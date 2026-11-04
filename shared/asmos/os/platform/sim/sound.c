/*
 * Sound is two devices the editor adds to MARS and RARS, as x86's is two Linux devices. The MIDI
 * port at 0xFFFF0340 takes raw MIDI bytes for a General MIDI synthesizer, as x86's
 * /dev/snd/midiC1D0 does, and sends them on at MIDI's speed from a buffer of 4 KB. The
 * sampled-audio output at 0xFFFF0350 plays 16-bit samples at a rate it is given, as x86's
 * /dev/dsp does, from a queue of 8,192 that its Free register says how much of is empty. Both are
 * memory: <sim.h> names their registers (SIM_MIDI_..., SIM_PCM_...). MARS and RARS themselves have
 * neither, and their blocks read 0 there, which is how asmOS tells.
 */
#include <sim.h>

/* In mono a word of the output's Data register is two samples, the first in its low half. A
 * sample asmOS was given alone waits here for the next one. */
static int pcm_held, pcm_held_sample;

int platform_pcm_open(int rate) {
    if (!sim_pcm_present()) return -1;
    SIM_PCM_CONTROL = 0;              /* off, which empties the queue */
    SIM_PCM_RATE = (unsigned)rate;
    SIM_PCM_CONTROL = SIM_PCM_ON;     /* mono */
    pcm_held = 0;
    return 0;
}

/* Free counts frames, a sample each in mono, and one of them is the held sample's. */
int platform_pcm_space(void) { return (int)SIM_PCM_FREE - pcm_held; }

int platform_pcm_write(const int16_t *samples, int count) {
    int at = 0;
    if (pcm_held && count > 0) {
        SIM_PCM_DATA = ((unsigned)pcm_held_sample & 0xffffu) | (unsigned)(uint16_t)samples[at++] << 16;
        pcm_held = 0;
    }
    for (; at + 1 < count; at += 2)
        SIM_PCM_DATA = ((unsigned)samples[at] & 0xffffu) | (unsigned)(uint16_t)samples[at + 1] << 16;
    if (at < count) {
        pcm_held_sample = samples[at];
        pcm_held = 1;
    }
    return count;
}

void platform_pcm_close(void) {
    /* what is still queued is dropped, as off does */
    SIM_PCM_CONTROL = 0;
    pcm_held = 0;
}

int platform_midi_open(void) { return sim_midi_present() ? 0 : -1; }

/* sim_midi_write waits while the port's buffer is full, as a write to /dev/snd/midiC1D0 does. */
void platform_midi_write(const unsigned char *bytes, int length) {
    for (int i = 0; i < length; i++) sim_midi_write(bytes[i]);
}
