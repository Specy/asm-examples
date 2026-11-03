/*
 * The keyboard and the pointer are Linux input devices, /dev/input/event0 and on, each a stream of
 * struct input_event: a type, a code and a value. The keyboard is the device that has a key A; the
 * pointer is the machine's USB tablet, the device that reports an absolute position, from 0 to
 * its axis's maximum whatever the screen's size. Linux numbers keys as asmOS does (keys.h), so a
 * key's code passes through as it is.
 */
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

static int keyboard_fd = -1, tablet_fd = -1;
static struct input_absinfo axis_x, axis_y;
static int tablet_x, tablet_y;          /* the pointer, in pixels */

static int evdev_has_bit(const unsigned char *bits, int bit) { return bits[bit / 8] >> (bit % 8) & 1; }

/* The first event device that sends the event type `type` and, for EV_KEY, has the key `key`. */
static int evdev_find(int type, int key) {
    for (int number = 0; number < 32; number++) {
        char path[32];
        snprintf(path, sizeof path, "/dev/input/event%d", number);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        unsigned char types[EV_MAX / 8 + 1] = { 0 };
        unsigned char keys[KEY_MAX / 8 + 1] = { 0 };
        ioctl(fd, EVIOCGBIT(0, sizeof types), types);
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys);
        if (evdev_has_bit(types, type) && (key < 0 || evdev_has_bit(keys, key))) return fd;
        close(fd);
    }
    return -1;
}

int platform_input_open(void) {
    keyboard_fd = evdev_find(EV_KEY, KEY_A);
    tablet_fd = evdev_find(EV_ABS, -1);
    if (tablet_fd >= 0) {
        ioctl(tablet_fd, EVIOCGABS(ABS_X), &axis_x);
        ioctl(tablet_fd, EVIOCGABS(ABS_Y), &axis_y);
    }
    if (keyboard_fd < 0 || tablet_fd < 0) printf("No keyboard or pointer found under /dev/input\n");
    return keyboard_fd >= 0 && tablet_fd >= 0;
}

static int tablet_pixel(int value, int size, const struct input_absinfo *axis) {
    int maximum = axis->maximum > 0 ? axis->maximum : 32767;
    return clamp_int((int)(((long)value * size + maximum - 1) / maximum), 0, size - 1);
}

/* An event of asmOS's, if the Linux event is one: answers 1 when it put one in `to`. */
static int evdev_key(const struct input_event *event, InputEvent *to) {
    if (event->type != EV_KEY || event->code >= BTN_MISC) return 0;   /* not a key, or a button */
    to->kind = event->value == 0 ? INPUT_KEY_UP : event->value == 1 ? INPUT_KEY_DOWN : INPUT_KEY_REPEAT;
    to->code = event->code;
    return 1;
}

static int evdev_tablet(const struct input_event *event, InputEvent *to) {
    if (event->type == EV_ABS && event->code == ABS_X) {
        tablet_x = tablet_pixel(event->value, x86_screen_w, &axis_x);
        to->kind = INPUT_MOVE;
    } else if (event->type == EV_ABS && event->code == ABS_Y) {
        tablet_y = tablet_pixel(event->value, x86_screen_h, &axis_y);
        to->kind = INPUT_MOVE;
    } else if (event->type == EV_KEY) {
        int button = event->code == BTN_LEFT ? BUTTON_LEFT
                     : event->code == BTN_RIGHT ? BUTTON_RIGHT
                     : event->code == BTN_MIDDLE ? BUTTON_MIDDLE : 0;
        if (!button) return 0;
        to->kind = event->value ? INPUT_BUTTON_DOWN : INPUT_BUTTON_UP;
        to->code = button;
    } else if (event->type == EV_REL && event->code == REL_WHEEL) {
        to->kind = INPUT_WHEEL;
        to->code = event->value;
    } else {
        return 0;
    }
    return 1;
}

/* Up to 64 events from a device that has some: reads never wait, the devices being O_NONBLOCK.
 * Answers how many events of asmOS's they made, and in `read_any` whether there were any. */
static int evdev_read(int fd, int (*translate)(const struct input_event *, InputEvent *),
                       InputEvent *events, int room, int *read_any) {
    struct input_event got[64];
    int want = room < 64 ? room : 64;
    ssize_t bytes = read(fd, got, want * sizeof got[0]);
    int count = 0;
    for (int i = 0; i < bytes / (ssize_t)sizeof got[0]; i++) {
        if (!translate(&got[i], &events[count])) continue;
        events[count].x = tablet_x;
        events[count].y = tablet_y;
        count++;
    }
    *read_any |= bytes > 0;
    return count;
}

int platform_input_read(InputEvent *events, int max, int timeout_ms) {
    struct pollfd waiting[2] = { { keyboard_fd, POLLIN, 0 }, { tablet_fd, POLLIN, 0 } };
    if (poll(waiting, 2, timeout_ms) <= 0) return 0;
    /* the keyboard's events, then the tablet's, until some make events of asmOS's or none are left */
    for (;;) {
        int read_any = 0;
        int count = evdev_read(keyboard_fd, evdev_key, events, max, &read_any);
        count += evdev_read(tablet_fd, evdev_tablet, events + count, max - count, &read_any);
        if (count || !read_any) return count;
    }
}
