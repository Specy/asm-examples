/*
 * Sound, through the raw MIDI port of the machine's General MIDI synthesizer. A note is three
 * bytes written to the port; the synthesizer plays it at once. Notes that should start or stop
 * later wait in a queue, which the system process empties when their time comes, so a jingle
 * never holds anything up. Any process may queue notes: the queue is the kernel's, changed under
 * its lock.
 */

static int midi_fd = -1;

typedef struct {
    long due;
    unsigned char bytes[3];
    int length;
    int pid;                  /* the process that queued it */
} MidiLater;

enum { MIDI_QUEUE = 256 };
static MidiLater midi_queue[MIDI_QUEUE];
static int midi_queued;

static void sound_init(void) { midi_fd = open("/dev/snd/midiC1D0", O_WRONLY); }

static void midi_write(const unsigned char *bytes, int length) {
    if (midi_fd >= 0) write(midi_fd, bytes, length);
}

static void midi_later(long delay, int status, int data1, int data2, int length) {
    unsigned char bytes[3] = { (unsigned char)status, (unsigned char)data1, (unsigned char)data2 };
    if (delay <= 0) {
        midi_write(bytes, length);
        return;
    }
    if (midi_queued == MIDI_QUEUE || midi_fd < 0) return;
    MidiLater *later = &midi_queue[midi_queued++];
    later->pid = current->pid;
    later->due = os_now_ms() + delay;
    memcpy(later->bytes, bytes, 3);
    later->length = length;
}

void os_midi_send(const unsigned char *bytes, int length) {
    lock();
    current->midi_used = 1;
    midi_write(bytes, length);
    unlock();
}

void os_midi_program(int channel, int program) {
    lock();
    current->midi_used = 1;
    midi_later(0, 0xc0 | (channel & 15), program & 127, 0, 2);
    unlock();
}

/* A note that starts after `start` milliseconds and lasts `length`. */
void os_midi_note(int channel, int note, int velocity, int start_ms, int length_ms) {
    lock();
    current->midi_used = 1;
    midi_later(start_ms, 0x90 | (channel & 15), note & 127, velocity & 127, 3);
    midi_later(start_ms + length_ms, 0x80 | (channel & 15), note & 127, 0, 3);
    unlock();
}

/* Sends what is due; answers how long until the next queued message, or -1 for none. */
static long sound_run(void) {
    long now = os_now_ms(), next = -1;
    for (int i = 0; i < midi_queued;) {
        if (midi_queue[i].due <= now) {
            midi_write(midi_queue[i].bytes, midi_queue[i].length);
            midi_queue[i] = midi_queue[--midi_queued];
        } else {
            long wait = midi_queue[i].due - now;
            if (next < 0 || wait < next) next = wait;
            i++;
        }
    }
    return next;
}

/* A process that played music ended, perhaps killed in the middle of a note: what it queued is
 * dropped, and every note of the applications' channels, 0 to 12, is let go, the sustain pedal
 * too. */
static void sound_process_ended(int pid) {
    for (int i = 0; i < midi_queued;)
        if (midi_queue[i].pid == pid) midi_queue[i] = midi_queue[--midi_queued];
        else i++;
    for (int channel = 0; channel <= 12; channel++) {
        unsigned char off[6] = { (unsigned char)(0xb0 | channel), 123, 0, (unsigned char)(0xb0 | channel), 64, 0 };
        midi_write(off, 6);
    }
}

/* Every note off on every channel, as asmOS shuts down. */
static void sound_all_off(void) {
    midi_queued = 0;
    for (int channel = 0; channel < 16; channel++) {
        unsigned char off[3] = { (unsigned char)(0xb0 | channel), 123, 0 };
        midi_write(off, 3);
    }
}

/* The start-up chime: a warm pad under a rising vibraphone arpeggio; falling at shut down. */
static void sound_chime(int shutdown) {
    static const int rising[] = { 60, 64, 67, 71, 74, 79 };
    if (!os_settings()->sounds) return;
    os_midi_program(14, 89);     /* Pad 2 (warm) */
    os_midi_program(15, 11);     /* Vibraphone */
    int root = shutdown ? 53 : 48;
    os_midi_note(14, root, 70, 0, 2200);
    os_midi_note(14, root + 7, 60, 0, 2200);
    os_midi_note(14, root + 16, 55, 0, 2200);
    for (int i = 0; i < 6; i++) os_midi_note(15, shutdown ? rising[5 - i] - 2 : rising[i], 90, i * 150, 1200);
}

/* A short two-tone ding for something that cannot be done. */
void os_beep(void) {
    if (!os_settings()->sounds) return;
    os_midi_program(13, 14);     /* Tubular bells */
    os_midi_note(13, 76, 70, 0, 300);
    os_midi_note(13, 72, 70, 120, 400);
}
