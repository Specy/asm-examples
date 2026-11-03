/*
 * Sound on x86 is two Linux devices. /dev/dsp, the OSS interface, plays sampled sound: the format
 * and the rate are set with ioctl, then the samples are written to it, and SNDCTL_DSP_GETOSPACE
 * says how many fit before a write would wait. /dev/snd/midiC1D0 is the raw MIDI port of the
 * machine's General MIDI synthesizer: three bytes written there are a note, played at once.
 */
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

static int pcm_fd = -1, midi_fd = -1;

int platform_pcm_open(int rate) {
    pcm_fd = open("/dev/dsp", O_WRONLY);
    if (pcm_fd < 0) return -1;
    int format = AFMT_S16_LE, channels = 1, speed = rate;
    /* six fragments, asked for in the device's starting format, which the 16-bit format and the
     * rate below then scale to 2 KB each: 1,024 samples, under a twentieth of a second at 22 kHz,
     * since /dev/dsp plays whole fragments and a larger one only makes the sound later */
    int fragments = 6 << 16 | 10;
    ioctl(pcm_fd, SNDCTL_DSP_SETFRAGMENT, &fragments);
    ioctl(pcm_fd, SNDCTL_DSP_SETFMT, &format);
    ioctl(pcm_fd, SNDCTL_DSP_CHANNELS, &channels);
    ioctl(pcm_fd, SNDCTL_DSP_SPEED, &speed);
    return 0;
}

int platform_pcm_space(void) {
    audio_buf_info room = { 0 };
    return ioctl(pcm_fd, SNDCTL_DSP_GETOSPACE, &room) == 0 ? room.bytes / 2 : 0;
}

int platform_pcm_write(const int16_t *samples, int count) {
    ssize_t written = write(pcm_fd, samples, (size_t)count * 2);
    return written > 0 ? (int)(written / 2) : 0;
}

void platform_pcm_close(void) {
    /* what is still buffered is dropped: close would wait for it to play */
    ioctl(pcm_fd, SNDCTL_DSP_RESET, NULL);
    close(pcm_fd);
    pcm_fd = -1;
}

int platform_midi_open(void) {
    midi_fd = open("/dev/snd/midiC1D0", O_WRONLY);
    return midi_fd < 0 ? -1 : 0;
}

void platform_midi_write(const unsigned char *bytes, int length) {
    if (midi_fd >= 0) write(midi_fd, bytes, length);
}
