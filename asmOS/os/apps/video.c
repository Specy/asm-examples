/*
 * Video Player: .vid files, black-and-white video with its sound, which video/vid.c reads and
 * video/encode.c makes. The file is read whole into memory, and while it plays, a timer every 10 ms
 * keeps /dev/dsp full of sound and makes the frames the sound has reached: the sound is the clock,
 * so the picture keeps with it however the processor is shared. Without the sound, turned off or
 * taken by another program, os_now_ms is the clock. Only the rows of a frame that changed are drawn
 * again, each a few runs of black and white, so a still picture costs nothing.
 *
 * The picture fits the window, which resizes, and a click on it pauses or plays; the bar under it
 * has Play and Pause, the time, and the progress, which a click or a drag moves. A .vid is cut into chunks of five seconds that
 * each start afresh, so that is where a jump lands: Left and Right go back or on by a chunk. Started
 * with nothing to open, the player plays the first video of the Project's `videos` folder.
 */

#include "video/vid.c"

enum { VIDEO_BAR_H = 30, VIDEO_TICK_MS = 10, VIDEO_MOST_FRAMES = 8, VIDEO_RING = 1 << 16, VIDEO_BLOCK = 1024,
       VIDEO_MAX_FILES = 64 };
enum { VIDEO_OPEN = 1, VIDEO_EXIT, VIDEO_PLAY_PAUSE, VIDEO_START, VIDEO_BACK, VIDEO_FORWARD, VIDEO_SOUND,
       VIDEO_ABOUT };
enum { VIDEO_TIMER = 1 };
enum { VIDEO_BACKGROUND = 0x101010 };

static const MenuItem VIDEO_FILE_MENU[] = {
    { "Open...", VIDEO_OPEN, 0, "Ctrl+O", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Exit", VIDEO_EXIT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};
/* Each window has a copy of the Play menu, whose Sound item it checks. */
static const MenuItem VIDEO_PLAY_MENU[] = {
    { "Play/Pause", VIDEO_PLAY_PAUSE, 0, "Space", NULL },
    { "From the Start", VIDEO_START, 0, "Home", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Back 5 Seconds", VIDEO_BACK, 0, "Left", NULL },
    { "On 5 Seconds", VIDEO_FORWARD, 0, "Right", NULL },
    { "-", 0, 0, NULL, NULL },
    { "Sound", VIDEO_SOUND, MENU_CHECKED, "S", NULL },
    { NULL, 0, 0, NULL, NULL },
};
static const MenuItem VIDEO_HELP_MENU[] = {
    { "About Video Player", VIDEO_ABOUT, 0, NULL, NULL },
    { NULL, 0, 0, NULL, NULL },
};

typedef struct {
    Window *win;
    int w, h;                       /* the client area */
    MenuItem play_menu[sizeof VIDEO_PLAY_MENU / sizeof VIDEO_PLAY_MENU[0]];
    Menu menus[4];
    /* the video */
    char *data;                     /* the file, or NULL */
    VidFile file;
    VidPicture picture;
    VidSound sound;
    char path[128];
    /* playing it */
    int playing;
    int ended;                      /* it played to the end: Play starts it again */
    long clock_frame;               /* without sound: the frame at clock_ms */
    long clock_ms;
    /* the sound, while /dev/dsp is this window's */
    int sound_on;                   /* the Play menu's Sound */
    int pcm;
    int capacity;                   /* the samples /dev/dsp holds */
    int16_t *ring;                  /* the last VIDEO_RING samples made, by their place in the video */
    long made;                      /* the place of the next sample vid_sound_read makes */
    long written;                   /* and of the next written to /dev/dsp: at most `made` */
    /* drawing */
    Rect view;                      /* where the picture is in the client area */
    int all_rows;                   /* every row is drawn next time, not only those that changed */
    int shown_second, shown_fill;
    int seeking;
    ButtonTracker buttons;
} VideoPlayer;

static long video_samples_per_chunk(VideoPlayer *v) { return vid_chunk_samples(&v->file); }

/* ------------------------------------------------------------------ the clock */

/* The samples /dev/dsp has played of what was written: all but those it still holds. */
static long video_heard(VideoPlayer *v) {
    return v->written - (v->capacity - os_pcm_space());
}

/* The frame the video is at now. */
static long video_now(VideoPlayer *v) {
    if (!v->data) return 0;
    if (!v->playing) return v->picture.frame > 0 ? v->picture.frame - 1 : 0;
    if (v->pcm) return video_heard(v) * v->file.fps / v->file.rate;
    return v->clock_frame + (os_now_ms() - v->clock_ms) * v->file.fps / 1000;
}

static void video_time_text(char *text, size_t size, long frames, int fps) {
    long seconds = frames / fps;
    snprintf(text, size, "%ld:%02ld", seconds / 60, seconds % 60);
}

/* ------------------------------------------------------------------ the sound */

static void video_sound_close(VideoPlayer *v) {
    if (v->pcm) os_pcm_close();
    v->pcm = 0;
}

/*
 * /dev/dsp at the video's rate, from sample `from` on, unless the sound is off, there is none, or
 * another program has it. The ring has the samples made last; a sample before them, or one past
 * them since the sound was off, is made again from the start of its chunk.
 */
static void video_sound_open(VideoPlayer *v, long from) {
    video_sound_close(v);
    if (!v->sound_on || !v->file.rate || from >= v->file.samples) return;
    if (from < v->made - VIDEO_RING || from > v->made) {
        int chunk = (int)(from / video_samples_per_chunk(v));
        vid_sound_seek(&v->sound, chunk);
        v->made = chunk * video_samples_per_chunk(v);
        while (v->made < from) {
            int at = (int)(v->made % VIDEO_RING);
            int count = min_int(min_int((int)(from - v->made), VIDEO_BLOCK), VIDEO_RING - at);
            int got = vid_sound_read(&v->sound, v->ring + at, count);
            if (!got) break;
            v->made += got;
        }
    }
    if (os_pcm_open(v->file.rate) != 0) return;
    v->pcm = 1;
    v->capacity = min_int(os_pcm_space(), VIDEO_RING);
    v->written = from;
}

/*
 * Fills /dev/dsp. What was written and dropped since (by a pause) is written again from the ring;
 * then new samples are made into the ring and written from it. Past the end of the sound comes
 * silence, so that the last of it is played out.
 */
static void video_sound_fill(VideoPlayer *v) {
    if (!v->pcm) return;
    int space = os_pcm_space();
    while (space > 0) {
        if (v->written == v->made) {
            int at = (int)(v->made % VIDEO_RING);
            int count = min_int(min_int(space, VIDEO_BLOCK), VIDEO_RING - at);
            int got = vid_sound_read(&v->sound, v->ring + at, count);
            if (!got) {
                memset(v->ring + at, 0, count * sizeof *v->ring);
                got = count;
            }
            v->made += got;
        }
        int at = (int)(v->written % VIDEO_RING);
        int count = min_int((int)(v->made - v->written), min_int(space, VIDEO_RING - at));
        int put = os_pcm_write(v->ring + at, count);
        if (put <= 0) break;
        v->written += put;
        space -= put;
    }
}

/* ------------------------------------------------------------------ drawing */

static Rect video_bar_rect(VideoPlayer *v) { return rect(0, v->h - VIDEO_BAR_H, v->w, VIDEO_BAR_H); }
static Rect video_button_rect(VideoPlayer *v) { return rect(4, v->h - VIDEO_BAR_H + 4, 30, 22); }
static Rect video_progress_rect(VideoPlayer *v) {
    return rect(42, v->h - VIDEO_BAR_H + 8, max_int(20, v->w - 42 - 104), 14);
}

/* Where the picture goes: as large as the space over the bar allows, its shape kept. */
static void video_place(VideoPlayer *v) {
    int room_w = v->w, room_h = max_int(1, v->h - VIDEO_BAR_H);
    if (!v->data) {
        v->view = rect(0, 0, room_w, room_h);
        return;
    }
    int w = room_w, h = (int)((long)room_w * v->file.height / v->file.width);
    if (h > room_h) {
        h = room_h;
        w = (int)((long)room_h * v->file.width / v->file.height);
    }
    v->view = rect((room_w - w) / 2, (room_h - h) / 2, max_int(1, w), max_int(1, h));
}

/*
 * The rows of the picture that changed, or all of them: each row of the window is the picture's row
 * it falls on, drawn as its runs, a fill each, straight into the window's pixels.
 */
static void video_draw_rows(VideoPlayer *v) {
    if (!v->data || v->picture.frame == 0) return;
    int stride;
    uint32_t *pixels = os_window_pixels(v->win, &stride, NULL);
    Rect view = v->view;
    int width = v->file.width, height = v->file.height, top = -1, bottom = -1;
    for (int y = 0; y < view.h; y++) {
        int row = (int)((long)y * height / view.h);
        if (!v->all_rows && !v->picture.changed[row]) continue;
        int count;
        const uint16_t *changes = vid_row(&v->picture, row, &count);
        uint32_t *line = pixels + (long)(view.y + y) * stride + view.x;
        int x = 0;
        for (int i = 0; i <= count; i++) {
            int end = i < count ? changes[i] * view.w / width : view.w;
            if (end > x) fill_rect32(line + x, stride * 4, end - x, 1, i & 1 ? WHITE : BLACK);
            x = end;
        }
        if (top < 0) top = y;
        bottom = y;
    }
    memset(v->picture.changed, 0, height);
    v->all_rows = 0;
    if (top >= 0) os_window_update(v->win, view.x, view.y + top, view.w, bottom - top + 1);
}

/* The space around the picture, and the picture, or what there is instead. */
static void video_draw_view(VideoPlayer *v) {
    os_window_draw(v->win);
    int room_h = v->h - VIDEO_BAR_H;
    Rect view = v->view;
    fill_rect(0, 0, v->w, view.y, VIDEO_BACKGROUND);
    fill_rect(0, view.y + view.h, v->w, room_h - view.y - view.h, VIDEO_BACKGROUND);
    fill_rect(0, view.y, view.x, view.h, VIDEO_BACKGROUND);
    fill_rect(view.x + view.w, view.y, v->w - view.x - view.w, view.h, VIDEO_BACKGROUND);
    if (!v->data || v->picture.frame == 0) {
        fill_rect(view.x, view.y, view.w, view.h, v->data ? BLACK : VIDEO_BACKGROUND);
        if (!v->data) {
            const char *text = "Open a .vid file with File > Open.";
            draw_text(view.x + (view.w - text_width(text)) / 2, view.y + view.h / 2 - 6, text, LIGHT);
        }
        os_window_update(v->win, 0, 0, v->w, room_h);
        return;
    }
    os_window_update(v->win, 0, 0, v->w, room_h);
    v->all_rows = 1;
    video_draw_rows(v);
}

static void video_draw_button(VideoPlayer *v) {
    Rect r = video_button_rect(v);
    int down = button_down(&v->buttons, 0);
    draw_button(r.x, r.y, r.w, r.h, "", down);
    int cx = r.x + r.w / 2 + down, cy = r.y + r.h / 2 + down;
    if (v->playing) {
        fill_rect(cx - 5, cy - 6, 4, 13, BLACK);
        fill_rect(cx + 2, cy - 6, 4, 13, BLACK);
    } else {
        for (int i = 0; i < 7; i++) vline(cx - 3 + i, cy - (6 - i), 2 * (6 - i) + 1, BLACK);
    }
}

/* The progress and the time, drawn when they changed or when `always`. */
static void video_draw_progress(VideoPlayer *v, int always) {
    Rect bar = video_progress_rect(v), whole = video_bar_rect(v);
    long frame = video_now(v), frames = v->data ? v->file.frames : 1;
    int inside = bar.w - 4;
    int fill = (int)(clamp_int((int)frame, 0, (int)frames) * (long)inside / frames);
    int second = v->data ? (int)(frame / v->file.fps) : 0;
    if (!always && fill == v->shown_fill && second == v->shown_second) return;
    os_window_draw(v->win);
    draw_field(bar.x, bar.y, bar.w, bar.h, WHITE);
    if (fill) fill_rect(bar.x + 2, bar.y + 2, fill, bar.h - 4, os_theme_colour(THEME_SELECTION));
    int text_x = bar.x + bar.w + 8;
    fill_rect(text_x, whole.y + 2, whole.w - text_x, whole.h - 2, FACE);
    if (v->data) {
        char now[16], total[16], text[40];
        video_time_text(now, sizeof now, frame, v->file.fps);
        video_time_text(total, sizeof total, v->file.frames, v->file.fps);
        snprintf(text, sizeof text, "%s / %s", now, total);
        draw_text(text_x, whole.y + 9, text, BLACK);
    }
    v->shown_fill = fill;
    v->shown_second = second;
    os_window_update(v->win, bar.x, whole.y, whole.w - bar.x, whole.h);
}

static void video_draw_bar(VideoPlayer *v) {
    Rect bar = video_bar_rect(v);
    os_window_draw(v->win);
    fill_rect(bar.x, bar.y, bar.w, bar.h, FACE);
    hline(bar.x, bar.y, bar.w, LIGHT);
    video_draw_button(v);
    video_draw_progress(v, 1);
    os_window_update(v->win, bar.x, bar.y, bar.w, bar.h);
}

static void video_draw_all(VideoPlayer *v) {
    video_draw_view(v);
    video_draw_bar(v);
}

static void video_title(VideoPlayer *v) {
    char title[112];
    const char *name = v->file.title[0] ? v->file.title : os_base_name(v->path);
    if (v->data) snprintf(title, sizeof title, "%.60s - Video Player", name);
    else snprintf(title, sizeof title, "Video Player");
    os_window_set_title(v->win, title);
}

/* ------------------------------------------------------------------ playing */

/* The frames up to the one the clock is at, drawing only the last; at the end, the video stops. */
static void video_catch_up(VideoPlayer *v) {
    long target = video_now(v);
    int chunk_frames = v->file.chunk_frames;
    /* too far behind, as when the processor was busy elsewhere: on from the chunk the clock is in */
    if (target / chunk_frames > v->picture.frame / chunk_frames && target < v->file.frames) {
        vid_picture_seek(&v->picture, (int)(target / chunk_frames));
        v->all_rows = 1;
    }
    int made = 0;
    while (v->picture.frame <= target && made < VIDEO_MOST_FRAMES && vid_picture_next(&v->picture)) made++;
    if (made) video_draw_rows(v);
}

static void video_set_clock(VideoPlayer *v, long frame) {
    v->clock_frame = frame;
    v->clock_ms = os_now_ms();
}

/* The video from the start of chunk `chunk`, playing or not: its first frame is shown. */
static void video_seek(VideoPlayer *v, int chunk) {
    if (!v->data) return;
    chunk = clamp_int(chunk, 0, v->file.chunks - 1);
    vid_picture_seek(&v->picture, chunk);
    vid_sound_seek(&v->sound, chunk);
    v->made = v->written = chunk * video_samples_per_chunk(v);
    v->ended = 0;
    if (v->playing) video_sound_open(v, v->made);
    video_set_clock(v, (long)chunk * v->file.chunk_frames);
    if (vid_picture_next(&v->picture)) {
        v->all_rows = 1;
        video_draw_rows(v);
    }
    video_sound_fill(v);
    video_draw_progress(v, 0);
}

static void video_play(VideoPlayer *v) {
    if (!v->data || v->playing) return;
    if (v->ended) {
        v->playing = 0;
        video_seek(v, 0);
    }
    v->playing = 1;
    /* on from the frame shown, the sound from the same moment */
    long frame = v->picture.frame > 0 ? v->picture.frame - 1 : 0;
    video_sound_open(v, frame * v->file.rate / v->file.fps);
    video_set_clock(v, frame);
    video_sound_fill(v);
    os_set_timer(v->win, VIDEO_TIMER, VIDEO_TICK_MS);
    os_window_draw(v->win);
    video_draw_button(v);
    Rect r = video_button_rect(v);
    os_window_update(v->win, r.x, r.y, r.w, r.h);
}

static void video_pause(VideoPlayer *v) {
    if (!v->playing) return;
    /* the frame the clock is at is the one shown, and the sound stops at once: what /dev/dsp still
     * held is dropped, to be written again from the ring */
    video_catch_up(v);
    v->playing = 0;
    video_sound_close(v);
    os_set_timer(v->win, VIDEO_TIMER, 0);
    os_window_draw(v->win);
    video_draw_button(v);
    Rect r = video_button_rect(v);
    os_window_update(v->win, r.x, r.y, r.w, r.h);
}

/* The 10 ms tick: the sound topped up, the picture brought to it, the bar moved on. */
static void video_tick(VideoPlayer *v) {
    if (!v->playing) return;
    video_sound_fill(v);
    video_catch_up(v);
    video_draw_progress(v, 0);
    if (video_now(v) >= v->file.frames) {
        video_pause(v);
        v->ended = 1;
        video_draw_progress(v, 1);
    }
}

static void video_close_file(VideoPlayer *v) {
    video_sound_close(v);
    os_set_timer(v->win, VIDEO_TIMER, 0);
    v->playing = v->ended = 0;
    os_free(v->data);
    for (int i = 0; i < 2; i++) {
        os_free(v->picture.changes[i]);
        os_free(v->picture.counts[i]);
    }
    os_free(v->picture.slot);
    os_free(v->picture.previous_slot);
    os_free(v->picture.changed);
    memset(&v->picture, 0, sizeof v->picture);
    v->data = NULL;
}

/* Opens a .vid and plays it; answers 1 when it could be read. */
static int video_open(VideoPlayer *v, const char *path) {
    long size;
    char *data = os_read_file(path, &size);
    VidFile file;
    if (!data || !vid_open(&file, (const uint8_t *)data, size)) {
        char message[180];
        if (data) snprintf(message, sizeof message, "%.80s is not a video Video Player can play.", os_base_name(path));
        else snprintf(message, sizeof message, "Video Player cannot open %.100s.", path);
        os_free(data);
        os_message_box(v->win, "Video Player", message, MB_OK, MB_WARNING);
        return 0;
    }
    video_close_file(v);
    v->data = data;
    v->file = file;
    v->picture.file = v->sound.file = &v->file;
    long rows = (long)file.height * (file.width + 1);
    for (int i = 0; i < 2; i++) {
        v->picture.changes[i] = os_alloc(rows * sizeof(uint16_t));
        v->picture.counts[i] = os_alloc(file.height * sizeof(uint16_t));
    }
    v->picture.slot = os_alloc(file.height);
    v->picture.previous_slot = os_alloc(file.height);
    v->picture.changed = os_alloc(file.height);
    if (!v->picture.changes[1] || !v->picture.counts[1] || !v->picture.changed) {
        video_close_file(v);
        os_message_box(v->win, "Video Player", "There is not enough memory for this video.", MB_OK, MB_WARNING);
        return 0;
    }
    copy_text(v->path, sizeof v->path, path);
    video_title(v);
    video_place(v);
    video_draw_all(v);
    video_seek(v, 0);
    video_play(v);
    return 1;
}

/* The first .vid of the `videos` folder, or "". */
static void video_first_in_folder(char *path, int size) {
    DirEntry *entries = os_alloc(VIDEO_MAX_FILES * sizeof *entries);
    int count = entries ? os_list_dir("videos", entries, VIDEO_MAX_FILES) : -1;
    path[0] = 0;
    for (int i = 0; i < count && !path[0]; i++)
        if (!entries[i].is_dir && os_app_for_file(entries[i].name) == APP_VIDEO)
            snprintf(path, size, "videos/%s", entries[i].name);
    os_free(entries);
}

/* ------------------------------------------------------------------ commands, keys and the mouse */

static void video_check_sound(VideoPlayer *v) {
    for (int i = 0; v->play_menu[i].label; i++)
        if (v->play_menu[i].id == VIDEO_SOUND) v->play_menu[i].flags = v->sound_on ? MENU_CHECKED : 0;
}

static void video_quit(VideoPlayer *v) {
    video_sound_close(v);
    os_exit(0);
}

static void video_command(VideoPlayer *v, int id) {
    switch (id) {
    case VIDEO_OPEN: {
        char path[128];
        copy_text(path, sizeof path, v->path[0] ? v->path : "videos");
        if (os_file_dialog(v->win, 0, "Open", ".vid", path, sizeof path)) video_open(v, path);
        break;
    }
    case VIDEO_EXIT: video_quit(v); break;
    case VIDEO_PLAY_PAUSE:
        if (v->playing) video_pause(v);
        else video_play(v);
        break;
    case VIDEO_START: video_seek(v, 0); break;
    case VIDEO_BACK: case VIDEO_FORWARD: {
        if (!v->data) break;
        /* a jump back from a chunk's first seconds goes to the chunk before */
        long frame = video_now(v);
        int chunk = (int)(frame / v->file.chunk_frames);
        if (id == VIDEO_BACK && frame % v->file.chunk_frames < v->file.fps) chunk--;
        video_seek(v, id == VIDEO_BACK ? chunk : chunk + 1);
        break;
    }
    case VIDEO_SOUND: {
        v->sound_on = !v->sound_on;
        video_check_sound(v);
        if (!v->playing) break;
        /* the clock passes from the sound to os_now_ms, or back, at the frame shown */
        long frame = v->picture.frame > 0 ? v->picture.frame - 1 : 0;
        video_set_clock(v, frame);
        if (v->sound_on) {
            video_sound_open(v, frame * v->file.rate / v->file.fps);
            video_sound_fill(v);
        } else {
            video_sound_close(v);
        }
        break;
    }
    case VIDEO_ABOUT:
        os_message_box(v->win, "About Video Player",
                       "Video Player for asmOS.\nIt plays .vid files: black and white pictures, each row "
                       "kept as where it changes colour, coded as a fax machine codes a page, and ADPCM "
                       "sound, which is its clock.",
                       MB_OK, MB_INFO);
        break;
    }
}

static void video_key(VideoPlayer *v, const Message *m) {
    if (m->mods & MOD_CTRL) {
        if (m->key == KEY_O) video_command(v, VIDEO_OPEN);
        return;
    }
    switch (m->key) {
    case KEY_SPACE: case KEY_ENTER: case KEY_KPENTER: case KEY_P: video_command(v, VIDEO_PLAY_PAUSE); break;
    case KEY_HOME: video_command(v, VIDEO_START); break;
    case KEY_LEFT: video_command(v, VIDEO_BACK); break;
    case KEY_RIGHT: video_command(v, VIDEO_FORWARD); break;
    case KEY_S: video_command(v, VIDEO_SOUND); break;
    }
}

static void video_mouse(VideoPlayer *v, const Message *m) {
    Rect button = video_button_rect(v);
    int changed, clicked = track_buttons(&v->buttons, &button, 1, m, &changed);
    if (changed) {
        os_window_draw(v->win);
        video_draw_button(v);
        os_window_update(v->win, button.x, button.y, button.w, button.h);
    }
    if (clicked == 0) {
        video_command(v, VIDEO_PLAY_PAUSE);
        return;
    }
    Rect progress = video_progress_rect(v);
    if (m->type == MSG_MOUSE_UP) v->seeking = 0;
    if ((m->type == MSG_MOUSE_DOWN && m->button == BUTTON_LEFT && rect_contains(progress, m->x, m->y)) ||
        (m->type == MSG_MOUSE_MOVE && v->seeking)) {
        if (!v->data) return;
        v->seeking = 1;
        long x = clamp_int(m->x - progress.x - 2, 0, progress.w - 4);
        long frame = x * v->file.frames / max_int(1, progress.w - 4);
        int chunk = (int)(frame / v->file.chunk_frames);
        /* a drag within the chunk being played leaves it playing */
        if (m->type == MSG_MOUSE_DOWN || chunk != (int)(video_now(v) / v->file.chunk_frames)) video_seek(v, chunk);
        return;
    }
    Rect room = rect(0, 0, v->w, v->h - VIDEO_BAR_H);
    if (m->type == MSG_MOUSE_DOWN && m->button == BUTTON_LEFT && rect_contains(room, m->x, m->y))
        video_command(v, VIDEO_PLAY_PAUSE);
}

static void video_relayout(VideoPlayer *v) {
    os_window_pixels(v->win, &v->w, &v->h);
    video_place(v);
    video_draw_all(v);
}

void video_main(const char *argument) {
    VideoPlayer *v = os_alloc(sizeof *v);
    if (!v) os_exit(1);
    v->ring = os_alloc(VIDEO_RING * sizeof *v->ring);
    if (!v->ring) os_exit(1);
    v->buttons.pressed = -1;
    v->sound_on = 1;
    memcpy(v->play_menu, VIDEO_PLAY_MENU, sizeof VIDEO_PLAY_MENU);
    v->menus[0] = (Menu){ "File", (MenuItem *)VIDEO_FILE_MENU };
    v->menus[1] = (Menu){ "Play", v->play_menu };
    v->menus[2] = (Menu){ "Help", (MenuItem *)VIDEO_HELP_MENU };
    v->menus[3] = (Menu){ NULL, NULL };
    v->win = os_window_create("Video Player", &ICON_VIDEO, 480, 360 + VIDEO_BAR_H, WIN_RESIZABLE);
    if (!v->win) os_exit(1);
    os_window_set_menu(v->win, v->menus);
    os_window_min_size(v->win, 240, 120 + VIDEO_BAR_H);
    os_window_pixels(v->win, &v->w, &v->h);
    video_place(v);
    video_draw_all(v);
    char path[128];
    if (argument && argument[0]) copy_text(path, sizeof path, argument);
    else video_first_in_folder(path, sizeof path);
    if (path[0]) video_open(v, path);
    for (;;) {
        Message m;
        os_get_message(&m);
        switch (m.type) {
        case MSG_TIMER: video_tick(v); break;
        case MSG_KEY: video_key(v, &m); break;
        case MSG_MOUSE_DOWN: case MSG_MOUSE_UP: case MSG_MOUSE_MOVE: video_mouse(v, &m); break;
        case MSG_COMMAND: video_command(v, m.id); break;
        case MSG_RESIZE: video_relayout(v); break;
        case MSG_SETTINGS: video_draw_all(v); break;
        case MSG_DROP:
            if (os_app_for_file(m.text) == APP_VIDEO) {
                video_open(v, m.text);
            } else {
                os_message_box(v->win, "Video Player", "Video Player plays videos whose names end in .vid.",
                               MB_OK, MB_INFO);
            }
            break;
        case MSG_CLOSE: case MSG_QUIT: video_quit(v); break;
        default: os_default_message(&m);
        }
    }
}
