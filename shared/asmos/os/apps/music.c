/*
 * Music Player: Standard MIDI Files, played on the General MIDI synthesizer through the kernel's
 * MIDI port. A file's tracks are read into one list of messages in time order, each with the
 * microsecond it is due at, and while the music plays a timer every 10 ms sends those whose time
 * has come. The playlist is the .mid files of the Project's music folder; Open, a file dropped on
 * the window, or `open` in the Terminal plays any other.
 *
 * A file's tempo changes, running status, notes, programs, controllers and pitch bends are kept;
 * SysEx and the other meta events are skipped. Channels 13 to 15 are the system's, so a file's
 * music on them moves to channels it leaves free.
 */

enum { MUSIC_MAX_SONGS = 64, MUSIC_MAX_EVENTS = 200000, MUSIC_TICK_MS = 10, MUSIC_ROW_H = 16,
       MUSIC_SCROLL_W = 16, MUSIC_CHANNELS = 13, MUSIC_BARS_EVERY = 5 };
enum { MUSIC_OPEN = 1, MUSIC_EXIT, MUSIC_PLAY_PAUSE, MUSIC_STOP, MUSIC_PREVIOUS, MUSIC_NEXT, MUSIC_RELOAD,
       MUSIC_ABOUT };
enum { MUSIC_TIMER = 1 };
enum { MUSIC_STOPPED, MUSIC_PLAYING, MUSIC_PAUSED };
enum { MUSIC_BUTTON_PREVIOUS, MUSIC_BUTTON_PLAY, MUSIC_BUTTON_PAUSE, MUSIC_BUTTON_STOP, MUSIC_BUTTON_NEXT,
       MUSIC_BUTTONS };
enum { MUSIC_LCD = 0x102018, MUSIC_LCD_TEXT = 0x60f060, MUSIC_LCD_DIM = 0x3c8c3c, MUSIC_LCD_BAR = 0x204a2a };

static MenuItem music_file_menu[] = {
    { "Open...", MUSIC_OPEN, 0, "Ctrl+O", NULL },
    { "Reload Playlist", MUSIC_RELOAD, 0, "F5", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", MUSIC_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem music_play_menu[] = {
    { "Play/Pause", MUSIC_PLAY_PAUSE, 0, "Space", NULL },
    { "Stop", MUSIC_STOP, 0, NULL, NULL },
    { "-", 0, 0, NULL, NULL },
    { "Previous", MUSIC_PREVIOUS, 0, NULL, NULL },
    { "Next", MUSIC_NEXT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static MenuItem music_help_menu[] = {
    { "About Music Player", MUSIC_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const Menu music_menus[] = {
    { "File", music_file_menu }, { "Play", music_play_menu }, { "Help", music_help_menu }, { NULL, NULL },
};

/* A message of the piece and when it is due, from the start. */
typedef struct {
    long us;
    unsigned char bytes[3];
    unsigned char length;
} MusicEvent;

typedef struct {
    Window *win;
    int w, h;                       /* the client area */
    /* the piece */
    MusicEvent *events;
    int count;
    long length_us;
    char path[128];
    char title[64], text[64], info[64];
    /* playing it */
    int state;
    long started;                   /* os_now_ms() at the piece's start, while it plays */
    long position;                  /* where it is in ms, while it does not */
    int next;                       /* the next event to send */
    int level[16];                  /* each channel's last loudness, for the bars */
    int ticks;
    int shown_second, shown_fill;   /* what the display shows, to draw only changes */
    int seeking;
    /* the playlist */
    DirEntry songs[MUSIC_MAX_SONGS];
    int song_count, current, selected, top;
    ScrollBar scroll;
    ButtonTracker buttons;
} MusicPlayer;

/* ------------------------------------------------------------------ reading a MIDI file */

/* What reading a file collects: every message and tempo change of every track, with its tick. */
typedef struct {
    long tick;
    int order;                      /* the order read, which sorting keeps for equal ticks */
    long tempo;                     /* a tempo change's microseconds a quarter note, or 0 */
    unsigned char bytes[3];
    unsigned char length;
} MusicRaw;

typedef struct {
    MusicRaw *raw;
    int count, capacity;
    long end_tick;
    char title[64], text[64];
} MusicReader;


/* A variable-length number: seven bits a byte, the high bit set on all but the last. */
static long music_number(const unsigned char *data, long end, long *at) {
    long value = 0;
    for (int i = 0; i < 4 && *at < end; i++) {
        unsigned char byte = data[(*at)++];
        value = value << 7 | (byte & 0x7f);
        if (!(byte & 0x80)) break;
    }
    return value;
}

static int music_push(MusicReader *r, long tick, long tempo, const unsigned char *bytes, int length) {
    if (r->count == r->capacity) {
        if (r->capacity >= MUSIC_MAX_EVENTS) return 0;
        int capacity = r->capacity ? r->capacity * 2 : 1024;
        MusicRaw *grown = os_realloc(r->raw, capacity * sizeof *grown);
        if (!grown) return 0;
        r->raw = grown;
        r->capacity = capacity;
    }
    MusicRaw *raw = &r->raw[r->count];
    raw->tick = tick;
    raw->order = r->count++;
    raw->tempo = tempo;
    raw->length = (unsigned char)length;
    for (int i = 0; i < 3; i++) raw->bytes[i] = i < length ? bytes[i] : 0;
    return 1;
}

/* A text meta event as the font can show it. */
static void music_meta_text(char *to, size_t size, const unsigned char *text, long length) {
    size_t kept = 0;
    for (long i = 0; i < length && kept + 1 < size; i++)
        to[kept++] = text[i] >= 32 && text[i] < 127 ? (char)text[i] : ' ';
    to[kept] = 0;
}

/* One MTrk chunk's events, from `at` to `end`. */
static void music_read_track(MusicReader *r, const unsigned char *data, long at, long end, int track) {
    long tick = 0;
    unsigned char status = 0;
    while (at < end) {
        os_yield_point();       /* an event at a time: a long song takes a while to read */
        tick += music_number(data, end, &at);
        if (at >= end) break;
        unsigned char byte = data[at];
        if (byte == 0xff) {
            /* a meta event: the tempo and the names are kept */
            if (at + 2 > end) break;
            unsigned char type = data[at + 1];
            at += 2;
            long length = music_number(data, end, &at);
            if (length < 0 || at + length > end) break;
            const unsigned char *meta = data + at;
            at += length;
            if (type == 0x2f) break;
            /* the tempo: 3 bytes, big-endian */
            long tempo = type == 0x51 && length >= 3 ? (long)read_be16(meta) << 8 | meta[2] : 0;
            if (tempo > 0) music_push(r, tick, tempo, NULL, 0);
            else if (type == 0x03 && track == 0 && !r->title[0])
                music_meta_text(r->title, sizeof r->title, meta, length);
            else if (type == 0x01 && !r->text[0])
                music_meta_text(r->text, sizeof r->text, meta, length);
            continue;
        }
        if (byte == 0xf0 || byte == 0xf7) {
            /* SysEx: its length, then the bytes skipped */
            at++;
            at += music_number(data, end, &at);
            continue;
        }
        if (byte >= 0xf1) {
            /* not something a file holds: skipped, and running status with it */
            at++;
            status = 0;
            continue;
        }
        if (byte & 0x80) {
            status = byte;
            at++;
        } else if (!status) {
            at++;
            continue;
        }
        /* running status: a message without a status byte has the last one's */
        int kind = status & 0xf0, length = kind == 0xc0 || kind == 0xd0 ? 2 : 3;
        if (at + length - 1 > end) break;
        unsigned char bytes[3] = { status, (unsigned char)(data[at] & 0x7f), 0 };
        if (length == 3) bytes[2] = data[at + 1] & 0x7f;
        at += length - 1;
        if (!music_push(r, tick, 0, bytes, length)) break;
    }
    if (tick > r->end_tick) r->end_tick = tick;
}

static int music_raw_order(const void *a, const void *b) {
    const MusicRaw *x = a, *y = b;
    if (x->tick != y->tick) return x->tick < y->tick ? -1 : 1;
    return (x->order > y->order) - (x->order < y->order);
}

/* Reads a whole file into `p`'s piece; answers 0, leaving the piece as it was, if it is not one. */
static int music_parse(MusicPlayer *p, const unsigned char *data, long size, const char *path) {
    if (size < 14 || memcmp(data, "MThd", 4) != 0) return 0;
    long header = (long)read_be32(data + 4);
    int format = (int)read_be16(data + 8), tracks = (int)read_be16(data + 10);
    int division = (int)read_be16(data + 12);
    if (header < 6 || format > 2 || division == 0) return 0;
    MusicReader reader = { 0 };
    long at = 8 + header;
    int track = 0;
    while (at + 8 <= size && track < tracks) {
        long length = (long)read_be32(data + at + 4), end = at + 8 + length;
        if (end > size) end = size;
        if (!memcmp(data + at, "MTrk", 4)) music_read_track(&reader, data, at + 8, end, track++);
        at += 8 + length;
    }
    if (!track) {
        os_free(reader.raw);
        return 0;
    }
    qsort(reader.raw, reader.count, sizeof *reader.raw, music_raw_order);
    /* channels 13 to 15 are the system's: the file's music on them moves to channels it leaves free */
    int used[16] = { 0 }, map[16];
    for (int i = 0; i < reader.count; i++)
        if (reader.raw[i].length) used[reader.raw[i].bytes[0] & 15] = 1;
    for (int c = 0; c < 16; c++) map[c] = c;
    for (int c = MUSIC_CHANNELS; c < 16; c++) {
        if (!used[c]) continue;
        map[c] = MUSIC_CHANNELS - 1;
        for (int free = 0; free < MUSIC_CHANNELS; free++)
            if (free != 9 && !used[free]) {
                used[free] = 1;
                map[c] = free;
                break;
            }
    }
    MusicEvent *events = os_alloc((reader.count ? reader.count : 1) * sizeof *events);
    if (!events) {
        os_free(reader.raw);
        return 0;
    }
    /* ticks to microseconds: from the last tempo change, at its tempo; SMPTE time has no tempo. The
     * products are 64-bit: a few thousand ticks times a tempo of half a million is over what a long
     * holds on a 32-bit machine. */
    long tempo = 500000, base_tick = 0, base_us = 0, per_second = 0, count = 0;
    if (division & 0x8000) per_second = (long)(256 - (division >> 8 & 0xff)) * (division & 0xff);
    if (per_second <= 0 && (division & 0x8000)) per_second = 1000;
    for (int i = 0; i < reader.count; i++) {
        MusicRaw *raw = &reader.raw[i];
        long us = per_second ? (long)((int64_t)raw->tick * 1000000 / per_second)
                             : base_us + (long)((int64_t)(raw->tick - base_tick) * tempo / division);
        if (!raw->length) {
            base_us = us;
            base_tick = raw->tick;
            tempo = raw->tempo;
            continue;
        }
        MusicEvent *event = &events[count++];
        event->us = us;
        memcpy(event->bytes, raw->bytes, 3);
        event->bytes[0] = (unsigned char)((raw->bytes[0] & 0xf0) | map[raw->bytes[0] & 15]);
        event->length = raw->length;
    }
    long length_us = per_second ? (long)((int64_t)reader.end_tick * 1000000 / per_second)
                                : base_us + (long)((int64_t)(reader.end_tick - base_tick) * tempo / division);
    if (count && events[count - 1].us > length_us) length_us = events[count - 1].us;
    os_free(reader.raw);
    os_free(p->events);
    p->events = events;
    p->count = (int)count;
    p->length_us = length_us;
    copy_text(p->path, sizeof p->path, path);
    if (reader.title[0]) {
        copy_text(p->title, sizeof p->title, reader.title);
    } else {
        /* no name in the file: its own, without the extension */
        copy_text(p->title, sizeof p->title, os_base_name(path));
        char *dot = strrchr(p->title, '.');
        if (dot && dot != p->title) *dot = 0;
    }
    copy_text(p->text, sizeof p->text, reader.text[0] ? reader.text : os_base_name(path));
    long seconds = length_us / 1000000;
    snprintf(p->info, sizeof p->info, "Format %d, %d track%s, %ld:%02ld", format, track, track == 1 ? "" : "s",
             seconds / 60, seconds % 60);
    return 1;
}

/* ------------------------------------------------------------------ the synthesizer */

/* Every note off on the channels the player uses, and the sustain pedal up, as music stops. */
static void music_all_off(MusicPlayer *p) {
    unsigned char bytes[MUSIC_CHANNELS * 5];
    int used = 0;
    for (int channel = 0; channel < MUSIC_CHANNELS; channel++) {
        bytes[used++] = (unsigned char)(0xb0 | channel);
        bytes[used++] = 123;
        bytes[used++] = 0;
        /* running status: the pedal's controller needs no second status byte */
        bytes[used++] = 64;
        bytes[used++] = 0;
    }
    os_midi_send(bytes, used);
    memset(p->level, 0, sizeof p->level);
}

/* Before a piece: the channels as a new one expects them, a piano at full volume in the middle. */
static void music_reset_channels(void) {
    unsigned char bytes[MUSIC_CHANNELS * 9];
    int used = 0;
    for (int channel = 0; channel < MUSIC_CHANNELS; channel++) {
        unsigned char reset[9] = { (unsigned char)(0xb0 | channel), 121, 0, 7, 100, 10, 64,
                                   (unsigned char)(0xc0 | channel), 0 };
        memcpy(bytes + used, reset, sizeof reset);
        used += sizeof reset;
    }
    os_midi_send(bytes, used);
}

/* Sends the events due by `us`, in as few writes as fit, with running status within each. */
static void music_send_due(MusicPlayer *p, long us) {
    unsigned char batch[192];
    int used = 0;
    unsigned char status = 0;
    while (p->next < p->count && p->events[p->next].us <= us) {
        MusicEvent *event = &p->events[p->next++];
        if (used + 3 > (int)sizeof batch) {
            os_midi_send(batch, used);
            used = 0;
            status = 0;
        }
        if (event->bytes[0] != status) batch[used++] = status = event->bytes[0];
        for (int i = 1; i < event->length; i++) batch[used++] = event->bytes[i];
        if ((event->bytes[0] & 0xf0) == 0x90 && event->bytes[2]) {
            int channel = event->bytes[0] & 15;
            p->level[channel] = max_int(p->level[channel], event->bytes[2]);
        }
    }
    if (used) os_midi_send(batch, used);
}

static long music_now(MusicPlayer *p) {
    return p->state == MUSIC_PLAYING ? os_now_ms() - p->started : p->position;
}

/* Moves to `ms` into the piece: the notes stop, and each channel gets the program, controllers and
 * bend it would have had there. */
static void music_seek(MusicPlayer *p, long ms) {
    ms = clamp_int((int)ms, 0, (int)(p->length_us / 1000));
    music_all_off(p);
    unsigned char programs[16], controllers[16][128], bends[16][2];
    memset(programs, 0xff, sizeof programs);
    memset(controllers, 0xff, sizeof controllers);
    memset(bends, 0xff, sizeof bends);
    int index = 0;
    for (; index < p->count && p->events[index].us < ms * 1000; index++) {
        const unsigned char *bytes = p->events[index].bytes;
        int channel = bytes[0] & 15;
        switch (bytes[0] & 0xf0) {
        case 0xc0: programs[channel] = bytes[1]; break;
        case 0xb0: if (bytes[1] < 120) controllers[channel][bytes[1]] = bytes[2]; break;
        case 0xe0: bends[channel][0] = bytes[1], bends[channel][1] = bytes[2]; break;
        }
    }
    unsigned char batch[96];
    int used = 0;
    for (int channel = 0; channel < 16; channel++) {
        for (int c = -2; c < 128; c++) {
            unsigned char message[3];
            int length = 0;
            if (c == -2 && programs[channel] != 0xff) {
                message[0] = (unsigned char)(0xc0 | channel), message[1] = programs[channel], length = 2;
            } else if (c == -1 && bends[channel][0] != 0xff) {
                message[0] = (unsigned char)(0xe0 | channel), message[1] = bends[channel][0];
                message[2] = bends[channel][1], length = 3;
            } else if (c >= 0 && controllers[channel][c] != 0xff) {
                message[0] = (unsigned char)(0xb0 | channel), message[1] = (unsigned char)c;
                message[2] = controllers[channel][c], length = 3;
            }
            if (!length) continue;
            if (used + length > (int)sizeof batch) {
                os_midi_send(batch, used);
                used = 0;
            }
            memcpy(batch + used, message, length);
            used += length;
        }
    }
    if (used) os_midi_send(batch, used);
    p->next = index;
    if (p->state == MUSIC_PLAYING) {
        p->started = os_now_ms() - ms;
    } else {
        p->position = ms;
        if (p->state == MUSIC_STOPPED) p->state = MUSIC_PAUSED;
    }
}

/* ------------------------------------------------------------------ layout and drawing */

static Rect music_display_rect(MusicPlayer *p) { return rect(6, 6, p->w - 12, 66); }
static Rect music_progress_rect(MusicPlayer *p) { return rect(6, 78, p->w - 12, 16); }
static Rect music_button_rect(int which) { return rect(6 + which * 40, 100, 38, 26); }
static Rect music_bars_rect(MusicPlayer *p) {
    Rect display = music_display_rect(p);
    return rect(display.x + display.w - 8 - MUSIC_CHANNELS * 6, display.y + 8, MUSIC_CHANNELS * 6, display.h - 16);
}
static Rect music_list_rect(MusicPlayer *p) { return rect(6, 150, p->w - 12, max_int(40, p->h - 156)); }
static int music_rows(MusicPlayer *p) { return max_int(1, (music_list_rect(p).h - 4) / MUSIC_ROW_H); }

static void music_time_text(char *text, size_t size, long ms) {
    long seconds = ms / 1000;
    snprintf(text, size, "%ld:%02ld", seconds / 60, seconds % 60);
}

/* A triangle `size` pixels wide pointing right (or left), its middle row at `cy`. */
static void music_triangle(int x, int cy, int size, int right) {
    for (int i = 0; i < size; i++) {
        int half = right ? size - 1 - i : i;
        vline(x + i, cy - half, 2 * half + 1, BLACK);
    }
}

static void music_draw_button(MusicPlayer *p, int which) {
    Rect r = music_button_rect(which);
    int down = button_down(&p->buttons, which) || (which == MUSIC_BUTTON_PLAY && p->state == MUSIC_PLAYING) ||
               (which == MUSIC_BUTTON_PAUSE && p->state == MUSIC_PAUSED);
    draw_button(r.x, r.y, r.w, r.h, "", down);
    int cx = r.x + r.w / 2 + down, cy = r.y + r.h / 2 + down;
    switch (which) {
    case MUSIC_BUTTON_PREVIOUS:
        fill_rect(cx - 7, cy - 5, 2, 11, BLACK);
        music_triangle(cx - 5, cy, 6, 0);
        music_triangle(cx + 1, cy, 6, 0);
        break;
    case MUSIC_BUTTON_PLAY: music_triangle(cx - 3, cy, 7, 1); break;
    case MUSIC_BUTTON_PAUSE:
        fill_rect(cx - 5, cy - 6, 4, 13, BLACK);
        fill_rect(cx + 2, cy - 6, 4, 13, BLACK);
        break;
    case MUSIC_BUTTON_STOP: fill_rect(cx - 5, cy - 5, 11, 11, BLACK); break;
    case MUSIC_BUTTON_NEXT:
        music_triangle(cx - 6, cy, 6, 1);
        music_triangle(cx, cy, 6, 1);
        fill_rect(cx + 6, cy - 5, 2, 11, BLACK);
        break;
    }
}

static void music_draw_buttons(MusicPlayer *p) {
    for (int i = 0; i < MUSIC_BUTTONS; i++) music_draw_button(p, i);
}

/* The bars: how loud each channel's last note was, fading. */
static void music_draw_bars(MusicPlayer *p) {
    Rect bars = music_bars_rect(p);
    fill_rect(bars.x, bars.y, bars.w, bars.h, MUSIC_LCD);
    for (int channel = 0; channel < MUSIC_CHANNELS; channel++) {
        int height = p->level[channel] * bars.h / 127, x = bars.x + channel * 6;
        fill_rect(x, bars.y, 5, bars.h - height, MUSIC_LCD_BAR);
        if (height) fill_rect(x, bars.y + bars.h - height, 5, height, channel == 9 ? 0xe0c040 : MUSIC_LCD_TEXT);
    }
}

/* The display's last line: what the player does, and the time. */
static void music_draw_state(MusicPlayer *p) {
    Rect display = music_display_rect(p), bars = music_bars_rect(p);
    int y = display.y + 50;
    fill_rect(display.x + 2, y, bars.x - display.x - 4, FONT_HEIGHT, MUSIC_LCD);
    static const char *const states[] = { "Stopped", "Playing", "Paused" };
    draw_text(display.x + 8, y, p->count ? states[p->state] : "No music", MUSIC_LCD_TEXT);
    if (!p->count) return;
    char now[24], total[24], text[56];
    long ms = music_now(p);
    music_time_text(now, sizeof now, ms);
    music_time_text(total, sizeof total, p->length_us / 1000);
    snprintf(text, sizeof text, "%s / %s", now, total);
    draw_text(bars.x - 12 - text_width(text), y, text, MUSIC_LCD_TEXT);
    p->shown_second = (int)(ms / 1000);
}

static void music_draw_display(MusicPlayer *p) {
    Rect display = music_display_rect(p), bars = music_bars_rect(p);
    draw_field(display.x, display.y, display.w, display.h, MUSIC_LCD);
    gfx_clip(rect(display.x + 2, display.y + 2, bars.x - display.x - 10, display.h - 4));
    if (p->count) {
        draw_text_bold(display.x + 8, display.y + 6, p->title, WHITE);
        draw_text(display.x + 8, display.y + 21, p->text, MUSIC_LCD_TEXT);
        draw_text(display.x + 8, display.y + 35, p->info, MUSIC_LCD_DIM);
    } else {
        draw_text(display.x + 8, display.y + 6, "Choose a piece in the playlist,", MUSIC_LCD_TEXT);
        draw_text(display.x + 8, display.y + 21, "or open a .mid file.", MUSIC_LCD_TEXT);
    }
    gfx_clip(rect(0, 0, p->w, p->h));
    music_draw_state(p);
    music_draw_bars(p);
}

static void music_draw_progress(MusicPlayer *p) {
    Rect bar = music_progress_rect(p);
    draw_field(bar.x, bar.y, bar.w, bar.h, WHITE);
    int inside = bar.w - 4;
    long total = p->length_us / 1000;
    int fill = total > 0 ? (int)(clamp_int((int)music_now(p), 0, (int)total) * (long)inside / total) : 0;
    if (fill) fill_rect(bar.x + 2, bar.y + 2, fill, bar.h - 4, os_theme_colour(THEME_SELECTION));
    p->shown_fill = fill;
}

static void music_draw_list(MusicPlayer *p) {
    Rect list = music_list_rect(p);
    fill_rect(0, 128, p->w, 22, FACE);
    char label[48];
    snprintf(label, sizeof label, "Playlist: %d piece%s in music", p->song_count, p->song_count == 1 ? "" : "s");
    draw_text(8, 133, label, BLACK);
    draw_field(list.x, list.y, list.w, list.h, WHITE);
    int right = list.x + list.w - 2 - MUSIC_SCROLL_W;
    gfx_clip(rect(list.x + 2, list.y + 2, right - list.x - 2, list.h - 4));
    for (int row = 0; row < music_rows(p); row++) {
        int index = p->top + row, y = list.y + 2 + row * MUSIC_ROW_H;
        if (index >= p->song_count) break;
        int chosen = index == p->selected;
        uint32_t ink = chosen ? WHITE : BLACK;
        if (chosen) fill_rect(list.x + 2, y, right - list.x - 2, MUSIC_ROW_H, os_theme_colour(THEME_SELECTION));
        char name[80];
        snprintf(name, sizeof name, "%d. %s", index + 1, p->songs[index].name);
        if (index == p->current) {
            for (int i = 0; i < 5; i++) vline(list.x + 6 + i, y + 8 - (4 - i), 2 * (4 - i) + 1, ink);
            draw_text_bold(list.x + 16, y + 2, name, ink);
        } else {
            draw_text(list.x + 16, y + 2, name, ink);
        }
        char size[24];
        snprintf(size, sizeof size, "%ld KB", (p->songs[index].size + 1023) / 1024);
        draw_text(right - 6 - text_width(size), y + 2, size, chosen ? WHITE : SHADOW);
    }
    if (!p->song_count) draw_text(list.x + 16, list.y + 6, "(no .mid files in music)", SHADOW);
    gfx_clip(rect(0, 0, p->w, p->h));
    p->scroll.r = rect(right, list.y + 2, MUSIC_SCROLL_W, list.h - 4);
    p->scroll.total = p->song_count;
    p->scroll.visible = music_rows(p);
    p->scroll.top = p->top;
    draw_scroll_bar(&p->scroll);
}

static void music_draw_all(MusicPlayer *p) {
    os_window_draw(p->win);
    fill_rect(0, 0, p->w, p->h, FACE);
    music_draw_display(p);
    music_draw_progress(p);
    music_draw_buttons(p);
    music_draw_list(p);
    os_window_update_all(p->win);
}

/* The parts that change as the music plays, drawn again where they changed. */
static void music_draw_playing(MusicPlayer *p, int bars) {
    long ms = music_now(p), total = p->length_us / 1000;
    Rect progress = music_progress_rect(p), display = music_display_rect(p);
    int fill = total > 0 ? (int)(clamp_int((int)ms, 0, (int)total) * (long)(progress.w - 4) / total) : 0;
    os_window_draw(p->win);
    if (ms / 1000 != p->shown_second) {
        music_draw_state(p);
        os_window_update(p->win, display.x, display.y + 50, display.w, FONT_HEIGHT);
    }
    if (fill != p->shown_fill) {
        music_draw_progress(p);
        os_window_update(p->win, progress.x, progress.y, progress.w, progress.h);
    }
    if (bars) {
        Rect r = music_bars_rect(p);
        music_draw_bars(p);
        os_window_update(p->win, r.x, r.y, r.w, r.h);
    }
}

/* After the state changed: the display, the progress and the buttons. */
static void music_draw_controls(MusicPlayer *p) {
    os_window_draw(p->win);
    music_draw_display(p);
    music_draw_progress(p);
    music_draw_buttons(p);
    os_window_update(p->win, 0, 0, p->w, 128);
}

static void music_title(MusicPlayer *p) {
    char title[96];
    if (p->count) snprintf(title, sizeof title, "%.60s - Music Player", p->title);
    else snprintf(title, sizeof title, "Music Player");
    os_window_set_title(p->win, title);
}

/* ------------------------------------------------------------------ the playlist */

static int music_is_midi(const char *name) {
    size_t length = strlen(name);
    const char *extensions[2] = { ".mid", ".midi" };
    for (int e = 0; e < 2; e++) {
        size_t n = strlen(extensions[e]);
        if (length <= n) continue;
        int same = 1;
        for (size_t i = 0; i < n && same; i++)
            same = tolower((unsigned char)name[length - n + i]) == extensions[e][i];
        if (same) return 1;
    }
    return 0;
}

static void music_read_playlist(MusicPlayer *p) {
    DirEntry *entries = os_alloc(MUSIC_MAX_SONGS * sizeof *entries);
    int count = entries ? os_list_dir("music", entries, MUSIC_MAX_SONGS) : -1;
    p->song_count = 0;
    for (int i = 0; i < count; i++)
        if (!entries[i].is_dir && music_is_midi(entries[i].name)) p->songs[p->song_count++] = entries[i];
    os_free(entries);
    p->current = -1;
    for (int i = 0; i < p->song_count; i++) {
        char path[128];
        snprintf(path, sizeof path, "music/%s", p->songs[i].name);
        if (!strcmp(path, p->path)) p->current = i;
    }
    p->selected = clamp_int(p->selected, p->song_count ? 0 : -1, p->song_count - 1);
    p->top = clamp_int(p->top, 0, max_int(0, p->song_count - music_rows(p)));
}

static void music_follow(MusicPlayer *p) {
    if (p->selected < 0) return;
    if (p->selected < p->top) p->top = p->selected;
    if (p->selected >= p->top + music_rows(p)) p->top = p->selected - music_rows(p) + 1;
}

/* ------------------------------------------------------------------ playing */

static void music_play(MusicPlayer *p) {
    if (!p->count || p->state == MUSIC_PLAYING) return;
    if (p->state == MUSIC_STOPPED) {
        p->position = 0;
        p->next = 0;
    }
    p->state = MUSIC_PLAYING;
    p->started = os_now_ms() - p->position;
    p->ticks = 0;
    os_set_timer(p->win, MUSIC_TIMER, MUSIC_TICK_MS);
    music_draw_controls(p);
}

static void music_pause(MusicPlayer *p) {
    if (p->state != MUSIC_PLAYING) return;
    p->position = music_now(p);
    p->state = MUSIC_PAUSED;
    os_set_timer(p->win, MUSIC_TIMER, 0);
    music_all_off(p);
    music_draw_controls(p);
}

static void music_stop(MusicPlayer *p) {
    if (p->state != MUSIC_STOPPED) music_all_off(p);
    p->state = MUSIC_STOPPED;
    p->position = 0;
    p->next = 0;
    os_set_timer(p->win, MUSIC_TIMER, 0);
}

/* Loads a piece, and plays it if asked; answers 1 when it could be read. */
static int music_open(MusicPlayer *p, const char *path, int play) {
    long size;
    char *data = os_read_file(path, &size);
    if (!data) {
        char message[180];
        snprintf(message, sizeof message, "Music Player cannot open %.100s.", path);
        os_message_box(p->win, "Music Player", message, MB_OK, MB_WARNING);
        return 0;
    }
    music_stop(p);
    int ok = music_parse(p, (const unsigned char *)data, size, path);
    os_free(data);
    if (!ok) {
        char message[180];
        snprintf(message, sizeof message, "%.80s is not a MIDI file Music Player can play.", os_base_name(path));
        os_message_box(p->win, "Music Player", message, MB_OK, MB_WARNING);
        return 0;
    }
    music_reset_channels();
    p->current = -1;
    for (int i = 0; i < p->song_count; i++) {
        char song[128];
        snprintf(song, sizeof song, "music/%s", p->songs[i].name);
        if (!strcmp(song, path)) p->current = p->selected = i;
    }
    music_follow(p);
    music_title(p);
    music_draw_all(p);
    if (play) music_play(p);
    return 1;
}

static void music_open_song(MusicPlayer *p, int index, int play) {
    if (index < 0 || index >= p->song_count) return;
    char path[128];
    snprintf(path, sizeof path, "music/%s", p->songs[index].name);
    p->selected = index;
    music_open(p, path, play);
}

static void music_previous(MusicPlayer *p) {
    if (p->count && (music_now(p) > 3000 || p->current < 0 || p->song_count < 2)) {
        music_seek(p, 0);
        music_draw_controls(p);
        return;
    }
    if (p->song_count) music_open_song(p, (p->current - 1 + p->song_count) % p->song_count, 1);
}

static void music_next(MusicPlayer *p) {
    if (p->song_count) music_open_song(p, (p->current + 1) % p->song_count, 1);
}

/* The 10 ms tick: what is due goes out, the bars fade, and the piece's end moves on to the next. */
static void music_tick(MusicPlayer *p) {
    if (p->state != MUSIC_PLAYING) return;
    long ms = music_now(p);
    music_send_due(p, ms * 1000);
    if (p->next >= p->count && ms * 1000 >= p->length_us) {
        if (p->current >= 0 && p->current + 1 < p->song_count) {
            music_open_song(p, p->current + 1, 1);
        } else {
            music_stop(p);
            music_draw_controls(p);
        }
        return;
    }
    int bars = ++p->ticks % MUSIC_BARS_EVERY == 0, any = 0;
    if (bars) {
        for (int channel = 0; channel < 16; channel++) {
            any |= p->level[channel] > 0;
            p->level[channel] = max_int(0, p->level[channel] - MUSIC_BARS_EVERY);
        }
    }
    music_draw_playing(p, bars && any);
}

static void music_quit(MusicPlayer *p) {
    if (p->state != MUSIC_STOPPED) music_all_off(p);
    os_exit(0);
}

/* ------------------------------------------------------------------ commands, keys and the mouse */

static void music_command(MusicPlayer *p, int id) {
    switch (id) {
    case MUSIC_OPEN: {
        char path[128];
        copy_text(path, sizeof path, p->path[0] ? p->path : "music");
        if (os_file_dialog(p->win, 0, "Open", ".mid", path, sizeof path)) music_open(p, path, 1);
        break;
    }
    case MUSIC_RELOAD:
        music_read_playlist(p);
        music_draw_all(p);
        break;
    case MUSIC_EXIT: music_quit(p); break;
    case MUSIC_PLAY_PAUSE:
        if (p->state == MUSIC_PLAYING) music_pause(p);
        else if (p->count) music_play(p);
        else music_open_song(p, max_int(0, p->selected), 1);
        break;
    case MUSIC_STOP:
        music_stop(p);
        music_draw_controls(p);
        break;
    case MUSIC_PREVIOUS: music_previous(p); break;
    case MUSIC_NEXT: music_next(p); break;
    case MUSIC_ABOUT:
        os_message_box(p->win, "About Music Player",
                       "Music Player for asmOS.\nIt plays Standard MIDI Files on the General MIDI synthesizer, "
                       "sending each note as its time comes.",
                       MB_OK, MB_INFO);
        break;
    }
}

static void music_key(MusicPlayer *p, const Message *m) {
    if (m->mods & MOD_CTRL) {
        if (m->key == KEY_O) music_command(p, MUSIC_OPEN);
        return;
    }
    switch (m->key) {
    case KEY_SPACE: music_command(p, MUSIC_PLAY_PAUSE); return;
    case KEY_F5: music_command(p, MUSIC_RELOAD); return;
    case KEY_ENTER: case KEY_KPENTER: music_open_song(p, p->selected, 1); return;
    case KEY_LEFT: case KEY_RIGHT:
        if (p->count) {
            music_seek(p, music_now(p) + (m->key == KEY_LEFT ? -5000 : 5000));
            music_draw_controls(p);
        }
        return;
    case KEY_UP: case KEY_DOWN: case KEY_HOME: case KEY_END:
        if (!p->song_count) return;
        p->selected = m->key == KEY_HOME ? 0 : m->key == KEY_END ? p->song_count - 1
                      : clamp_int(p->selected + (m->key == KEY_UP ? -1 : 1), 0, p->song_count - 1);
        music_follow(p);
        os_window_draw(p->win);
        music_draw_list(p);
        os_window_update(p->win, 0, 128, p->w, p->h - 128);
        return;
    }
}

static void music_mouse(MusicPlayer *p, const Message *m) {
    if (scroll_bar_message(&p->scroll, m)) {
        p->top = p->scroll.top;
        os_window_draw(p->win);
        music_draw_list(p);
        os_window_update(p->win, 0, 128, p->w, p->h - 128);
        return;
    }
    if (m->type == MSG_MOUSE_WHEEL) {
        p->top = clamp_int(p->top - m->wheel * 3, 0, max_int(0, p->song_count - music_rows(p)));
        os_window_draw(p->win);
        music_draw_list(p);
        os_window_update(p->win, 0, 128, p->w, p->h - 128);
        return;
    }
    Rect buttons[MUSIC_BUTTONS];
    for (int i = 0; i < MUSIC_BUTTONS; i++) buttons[i] = music_button_rect(i);
    int changed, clicked = track_buttons(&p->buttons, buttons, MUSIC_BUTTONS, m, &changed);
    if (changed) {
        os_window_draw(p->win);
        music_draw_buttons(p);
        os_window_update(p->win, 0, buttons[0].y, p->w, buttons[0].h);
    }
    switch (clicked) {
    case MUSIC_BUTTON_PREVIOUS: music_previous(p); return;
    case MUSIC_BUTTON_PLAY:
        /* Play plays, from where it paused; Pause pauses and plays on */
        if (p->state != MUSIC_PLAYING) music_command(p, MUSIC_PLAY_PAUSE);
        return;
    case MUSIC_BUTTON_PAUSE:
        if (p->state != MUSIC_STOPPED) music_command(p, MUSIC_PLAY_PAUSE);
        return;
    case MUSIC_BUTTON_STOP: music_command(p, MUSIC_STOP); return;
    case MUSIC_BUTTON_NEXT: music_next(p); return;
    }
    Rect progress = music_progress_rect(p);
    if (m->type == MSG_MOUSE_UP) p->seeking = 0;
    if ((m->type == MSG_MOUSE_DOWN && m->button == BUTTON_LEFT && rect_contains(progress, m->x, m->y)) ||
        (m->type == MSG_MOUSE_MOVE && p->seeking)) {
        if (!p->count) return;
        p->seeking = 1;
        long total = p->length_us / 1000;
        long x = clamp_int(m->x - progress.x - 2, 0, progress.w - 4);
        music_seek(p, x * total / max_int(1, progress.w - 4));
        music_draw_controls(p);
        return;
    }
    Rect list = music_list_rect(p);
    if (m->type != MSG_MOUSE_DOWN || m->button != BUTTON_LEFT || !rect_contains(list, m->x, m->y)) return;
    int index = p->top + (m->y - list.y - 2) / MUSIC_ROW_H;
    if (index < 0 || index >= p->song_count) return;
    p->selected = index;
    if (m->clicks == 2) {
        music_open_song(p, index, 1);
        return;
    }
    os_window_draw(p->win);
    music_draw_list(p);
    os_window_update(p->win, 0, 128, p->w, p->h - 128);
}

static void music_relayout(MusicPlayer *p) {
    os_window_pixels(p->win, &p->w, &p->h);
    p->top = clamp_int(p->top, 0, max_int(0, p->song_count - music_rows(p)));
    music_follow(p);
    music_draw_all(p);
}

void music_main(const char *argument) {
    MusicPlayer *p = os_alloc(sizeof *p);
    if (!p) os_exit(1);
    p->buttons.pressed = -1;
    p->selected = -1;
    p->win = os_window_create("Music Player", &ICON_MUSIC, 380, 320, WIN_RESIZABLE);
    if (!p->win) os_exit(1);
    os_window_set_menu(p->win, music_menus);
    os_window_min_size(p->win, 300, 220);
    os_window_pixels(p->win, &p->w, &p->h);
    music_read_playlist(p);
    if (argument && argument[0]) music_open(p, argument, 1);
    else if (p->song_count) music_open_song(p, 0, 0);
    music_relayout(p);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_TIMER: music_tick(p); break;
        case MSG_KEY: music_key(p, &m); break;
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: case MSG_MOUSE_WHEEL:
            music_mouse(p, &m);
            break;
        case MSG_COMMAND: music_command(p, m.id); break;
        case MSG_RESIZE: music_relayout(p); break;
        case MSG_SETTINGS: music_draw_all(p); break;
        case MSG_DROP:
            if (music_is_midi(m.text)) {
                music_open(p, m.text, 1);
            } else {
                os_message_box(p->win, "Music Player", "Music Player plays MIDI files, whose names end in .mid.",
                               MB_OK, MB_INFO);
            }
            break;
        case MSG_CLOSE: case MSG_QUIT: music_quit(p); break;
        default: os_default_message(&m);
        }
    }
}
