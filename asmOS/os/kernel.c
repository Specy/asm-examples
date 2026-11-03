/*
 * The kernel: processes and the scheduler, and the services processes ask for through the os_
 * calls of os.h (memory, files, messages, timers, the clipboard, settings, sound).
 *
 * Every application runs as a process with a stack of its own. Process 0 is the system: the main
 * loop of main.c, which reads the keyboard and the pointer, delivers messages and composes the
 * screen. The others are applications. One of them runs at a time, and the kernel switches
 * between them in a signal handler. A process gives the processor up when it waits for a message
 * or sleeps: it sends itself SIGUSR2. And the interval timer sends SIGALRM every 10 ms while
 * applications are ready to run, which takes the processor from one that does not give it up:
 * preemption. Either way the handler saves the registers the signal frame holds, the
 * general-purpose ones and the 512 bytes of x87 and SSE state, into the process, and writes the
 * next process's into the frame, so the return from the handler resumes the next process where it
 * was stopped. A process's first turn starts it at process_start on its new stack.
 *
 * A process may be stopped at any instruction, so the kernel's own data is changed only under the
 * kernel lock: an os_ call takes it, and the timer leaves a process holding it alone until it lets
 * go. The system process is never preempted: it gives the processor up itself, between two passes
 * of its loop, when an application is ready.
 */

enum { MAX_PROCESSES = 32, STACK_SIZE = 256 * 1024, QUEUE_SIZE = 128, MAX_TIMERS = 64 };
enum { WAIT_NONE, WAIT_MESSAGE, WAIT_ANSWER };
/* The registers of the signal frame, in Linux's order for x86-64. */
enum { R8, R9, R10, R11, R12, R13, R14, R15, RDI, RSI, RBP, RBX, RDX, RAX, RCX, RSP, RIP, EFL,
       GREGS = 23 };

/* The signal frame's context, as Linux lays it out (sys/ucontext.h, which the headers here do not
 * have): what the handler reads the stopped process from and writes the next one into. */
typedef struct {
    uint64_t gregs[GREGS];
    void *fpregs;          /* 512 bytes in FXSAVE's layout */
    uint64_t reserved[8];
} MachineContext;
typedef struct {
    uint64_t flags;
    void *link;
    struct {
        void *sp;
        int flags;
        size_t size;
    } stack;
    MachineContext machine;
} SignalContext;

/* What os_alloc puts before the memory it hands out: 32 bytes, so the memory stays 16-aligned. */
typedef struct Allocation {
    struct Allocation *next, *prev;
    size_t size;
    size_t unused;
} Allocation;

typedef struct {
    int used, pid, state, app;
    char name[32];
    char argument[128];
    int has_argument;
    uint8_t *stack;
    uint64_t gregs[GREGS];
    uint8_t fx[512] __attribute__((aligned(16)));
    int started;
    Message queue[QUEUE_SIZE];
    int head, count;
    int waiting;
    int answer;
    int midi_used;                   /* it sent MIDI: its notes are silenced when it ends */
    long wake_at;
    Allocation allocations;          /* a ring: its own memory, freed when the process ends */
    long memory;
    long cpu, cpu_mark, cpu_rate;    /* instructions run: all, at the last second, in it */
    GfxContext gfx;
} Process;

static Process processes[MAX_PROCESSES];
static Process *volatile current = &processes[0];
#define SYSTEM (&processes[0])
static int next_pid = 1;
static volatile int kernel_lock, need_resched;
static long switched_at;              /* the instruction count at the last switch */
static int timer_on;
static long boot_ms;

typedef struct {
    int used, pid, id, interval;
    Window *win;
    long due;
} Timer;
static Timer timers[MAX_TIMERS];

void wm_process_ended(int pid);       /* wm.c: closes its windows, menus and dialogs */
static void sound_process_ended(int pid);   /* sound.c: drops its notes and silences them */
int wm_window_count(int pid);

/* ------------------------------------------------------------------ the clock */

long os_now_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000L + now.tv_nsec / 1000000 - boot_ms;
}

/* Instructions this program has run, which is what the CPU clock counts (at 100 MHz). */
static long instructions_now(void) {
    struct timespec now;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &now);
    return now.tv_sec * 100000000L + now.tv_nsec / 10;
}

/* ------------------------------------------------------------------ switching */

/* The profiler's samples: where the timer found the process os_profile was called by. */
enum { PROFILE_MAX = 4096 };
static uint64_t profile_samples[PROFILE_MAX];
static int profile_count, profile_pid = -1;

static void lock(void) { kernel_lock++; }
static void yield(void) { kill(getpid(), SIGUSR2); }

/* Lets go of the kernel lock, and gives the processor up if the timer asked for it meanwhile. */
static void unlock(void) {
    if (--kernel_lock == 0 && need_resched && current != SYSTEM) {
        need_resched = 0;
        yield();
    }
}

/* Starts the interval timer while applications are ready to run, and stops it while none is, so
 * that a desktop with nothing to do does nothing. */
static void timer_set(int on) {
    if (on == timer_on) return;
    timer_on = on;
    struct itimerval interval = { { 0, on ? 10000 : 0 }, { 0, on ? 10000 : 0 } };
    setitimer(ITIMER_REAL, &interval, NULL);
}

/* The application's code runs from here on its own stack, the first time it is switched to. */
static void process_start(void) {
    Process *process = current;
    APPS[process->app].main(process->has_argument ? process->argument : NULL);
    os_exit(0);
}

/* The next process after the current one that can run, round the table; NULL for none. */
static Process *next_ready(void) {
    int start = (int)(current - processes);
    for (int step = 1; step <= MAX_PROCESSES; step++) {
        Process *process = &processes[(start + step) % MAX_PROCESSES];
        if (process->used && process->state == PROC_READY) return process;
    }
    return NULL;
}

static void switch_process(SignalContext *context) {
    Process *from = current;
    if (from->state == PROC_RUNNING) from->state = PROC_READY;
    Process *to = next_ready();
    if (!to) to = from->state == PROC_READY ? from : SYSTEM;
    if (to == from) {
        from->state = PROC_RUNNING;
        return;
    }
    long now = instructions_now();
    from->cpu += now - switched_at;
    switched_at = now;
    uint64_t *gregs = context->machine.gregs;
    memcpy(from->gregs, gregs, sizeof from->gregs);
    if (context->machine.fpregs) memcpy(from->fx, context->machine.fpregs, sizeof from->fx);
    from->started = 1;
    if (to->started) {
        for (int r = R8; r <= EFL; r++) gregs[r] = to->gregs[r];
        if (context->machine.fpregs) memcpy(context->machine.fpregs, to->fx, sizeof to->fx);
    } else {
        /* a stack as a call leaves it: 16-aligned, less the return address */
        uint64_t top = ((uint64_t)(to->stack + STACK_SIZE) & ~(uint64_t)15) - 8;
        gregs[RSP] = top;
        gregs[RBP] = 0;
        gregs[RIP] = (uint64_t)process_start;
        to->started = 1;
    }
    to->state = PROC_RUNNING;
    current = to;
    gfx = to == SYSTEM ? &gfx_system : &to->gfx;
}

/* SIGUSR2 is a process giving the processor up; SIGALRM is the timer taking it. */
static void on_switch_signal(int signal, siginfo_t *info, void *context) {
    (void)info;
    if (signal == SIGALRM && current->pid == profile_pid && profile_count < PROFILE_MAX)
        profile_samples[profile_count++] = ((SignalContext *)context)->machine.gregs[RIP];
    if (signal == SIGALRM) {
        if (current == SYSTEM) return;
        if (kernel_lock) {
            need_resched = 1;
            return;
        }
    }
    switch_process(context);
}

static void kernel_start(void) {
    boot_ms = 0;
    boot_ms = os_now_ms();
    Process *system = SYSTEM;
    system->used = 1;
    system->pid = 0;
    system->state = PROC_RUNNING;
    system->started = 1;
    snprintf(system->name, sizeof system->name, "System");
    system->allocations.next = system->allocations.prev = &system->allocations;
    struct sigaction action;
    memset(&action, 0, sizeof action);
    action.sa_sigaction = on_switch_signal;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGALRM);
    sigaddset(&action.sa_mask, SIGUSR2);
    sigaction(SIGALRM, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);
    switched_at = instructions_now();
}

/* ------------------------------------------------------------------ processes */

static Process *process_of(int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++)
        if (processes[i].used && processes[i].pid == pid && processes[i].state != PROC_ENDED)
            return &processes[i];
    return NULL;
}

int os_getpid(void) { return current->pid; }

int os_spawn(int app, const char *argument) {
    if (app < 0 || app >= APP_COUNT) return -1;
    lock();
    Process *process = NULL;
    for (int i = 1; i < MAX_PROCESSES && !process; i++)
        if (!processes[i].used) process = &processes[i];
    uint8_t *stack = process ? malloc(STACK_SIZE) : NULL;
    if (!process || !stack) {
        free(stack);
        unlock();
        os_beep();
        return -1;
    }
    memset(process, 0, sizeof *process);
    process->used = 1;
    process->pid = next_pid++;
    process->app = app;
    process->stack = stack;
    process->state = PROC_READY;
    snprintf(process->name, sizeof process->name, "%s", APPS[app].name);
    if (argument) {
        snprintf(process->argument, sizeof process->argument, "%s", argument);
        process->has_argument = 1;
    }
    process->allocations.next = process->allocations.prev = &process->allocations;
    int pid = process->pid;
    unlock();
    return pid;
}

/* What ends with a process: its windows, timers, sound and memory. Its stack is the system's to
 * free, once the process is surely not running on it. */
static void process_cleanup(Process *process) {
    wm_process_ended(process->pid);
    for (int i = 0; i < MAX_TIMERS; i++)
        if (timers[i].used && timers[i].pid == process->pid) timers[i].used = 0;
    void pcm_release(int pid);
    pcm_release(process->pid);
    if (process->midi_used) sound_process_ended(process->pid);
    Allocation *ring = &process->allocations;
    while (ring->next != ring) {
        Allocation *block = ring->next;
        ring->next = block->next;
        free(block);
    }
    ring->prev = ring;
    process->memory = 0;
    process->count = 0;
    process->state = PROC_ENDED;
}

void os_exit(int code) {
    (void)code;
    lock();
    process_cleanup(current);
    kernel_lock = 0;
    for (;;) yield();
}

int os_kill(int pid) {
    if (pid == 0) return -1;
    lock();
    Process *process = process_of(pid);
    if (!process) {
        unlock();
        return -1;
    }
    if (process == current) os_exit(0);
    process_cleanup(process);
    unlock();
    return 0;
}

void os_yield(void) { yield(); }

void os_sleep(int ms) {
    lock();
    current->wake_at = os_now_ms() + ms;
    current->state = PROC_SLEEPING;
    kernel_lock--;
    yield();
}

/* The system's part of scheduling, once a pass: wakes the processes whose sleep is over and frees
 * the stacks of the ones that ended; answers how long until the next one wakes, or -1. */
static long schedule_pass(void) {
    long now = os_now_ms(), next = -1;
    for (int i = 1; i < MAX_PROCESSES; i++) {
        Process *process = &processes[i];
        if (!process->used) continue;
        if (process->state == PROC_ENDED) {
            free(process->stack);
            process->used = 0;
        } else if (process->state == PROC_SLEEPING) {
            if (process->wake_at <= now) process->state = PROC_READY;
            else if (next < 0 || process->wake_at - now < next) next = process->wake_at - now;
        }
    }
    return next;
}

static int applications_ready(void) {
    for (int i = 1; i < MAX_PROCESSES; i++)
        if (processes[i].used && processes[i].state == PROC_READY) return 1;
    return 0;
}

static int applications_running(void) {
    int count = 0;
    for (int i = 1; i < MAX_PROCESSES; i++)
        count += processes[i].used && processes[i].state != PROC_ENDED;
    return count;
}

int os_processes(ProcessInfo *list, int max) {
    lock();
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        Process *process = &processes[i];
        if (!process->used || process->state == PROC_ENDED) continue;
        if (count < max) {
            ProcessInfo *info = &list[count];
            info->pid = process->pid;
            snprintf(info->name, sizeof info->name, "%s", process->name);
            info->state = process->state;
            info->memory = process->memory;
            info->cpu = process->cpu_rate;
            info->windows = wm_window_count(process->pid);
        }
        count++;
    }
    unlock();
    return count;
}

static long load_total;
static int load_busy;
static long idle_ms;                  /* the system's time in poll this second */

void os_cpu_load(int *busy_percent, long *instructions) {
    *busy_percent = load_busy;
    *instructions = load_total;
}

void os_profile(int on) {
    lock();
    profile_pid = on ? current->pid : -1;
    profile_count = 0;
    unlock();
}

int os_profile_take(uint64_t *addresses, int max) {
    lock();
    int count = profile_count < max ? profile_count : max;
    memcpy(addresses, profile_samples, count * sizeof *addresses);
    profile_count = 0;
    unlock();
    return count;
}

long os_instructions(void) {
    lock();     /* no switch between reading the two */
    long count = current->cpu + instructions_now() - switched_at;
    unlock();
    return count;
}

/* Once a second: what each process ran in the second gone, and how busy the processor was. */
static void measure_load(long elapsed_ms) {
    long now = instructions_now();
    current->cpu += now - switched_at;
    switched_at = now;
    load_total = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        Process *process = &processes[i];
        if (!process->used) continue;
        process->cpu_rate = process->cpu - process->cpu_mark;
        process->cpu_mark = process->cpu;
        load_total += process->cpu_rate;
    }
    load_busy = elapsed_ms > 0 ? clamp_int((int)(100 - idle_ms * 100 / elapsed_ms), 0, 100) : 0;
    idle_ms = 0;
}

/* ------------------------------------------------------------------ memory */

void *os_alloc(size_t size) {
    lock();
    Allocation *block = calloc(1, sizeof(Allocation) + size);
    if (!block) {
        unlock();
        return NULL;
    }
    block->size = size;
    Allocation *ring = &current->allocations;
    block->next = ring->next;
    block->prev = ring;
    ring->next->prev = block;
    ring->next = block;
    current->memory += size;
    unlock();
    return block + 1;
}

void os_free(void *memory) {
    if (!memory) return;
    lock();
    Allocation *block = (Allocation *)memory - 1;
    block->prev->next = block->next;
    block->next->prev = block->prev;
    current->memory -= block->size;
    free(block);
    unlock();
}

void *os_realloc(void *memory, size_t size) {
    if (!memory) return os_alloc(size);
    size_t old = ((Allocation *)memory - 1)->size;
    void *grown = os_alloc(size);
    if (!grown) return NULL;
    memcpy(grown, memory, old < size ? old : size);
    os_free(memory);
    return grown;
}

/* ------------------------------------------------------------------ files */

char *os_read_file(const char *path, long *size) {
    lock();
    FILE *file = fopen(path, "rb");
    char *data = NULL;
    long length = 0;
    if (file) {
        fseek(file, 0, SEEK_END);
        length = ftell(file);
        fseek(file, 0, SEEK_SET);
        data = length >= 0 ? os_alloc(length + 1) : NULL;
        if (data && fread(data, 1, length, file) != (size_t)length) {
            os_free(data);
            data = NULL;
        }
        fclose(file);
    }
    if (size) *size = data ? length : 0;
    unlock();
    return data;
}

int os_write_file(const char *path, const void *data, long size) {
    lock();
    FILE *file = fopen(path, "wb");
    int ok = file && fwrite(data, 1, size, file) == (size_t)size;
    if (file && fclose(file) != 0) ok = 0;
    unlock();
    return ok ? 0 : -1;
}

static int entry_order(const void *a, const void *b) {
    const DirEntry *x = a, *y = b;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;
    return strcmp(x->name, y->name);
}

int os_list_dir(const char *path, DirEntry *entries, int max) {
    lock();
    DIR *dir = opendir(path && path[0] ? path : ".");
    if (!dir) {
        unlock();
        return -1;
    }
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) && count < max) {
        if (entry->d_name[0] == '.') continue;
        DirEntry *item = &entries[count++];
        snprintf(item->name, sizeof item->name, "%.63s", entry->d_name);
        char full[256];
        snprintf(full, sizeof full, "%s%s%s", path && path[0] ? path : "", path && path[0] ? "/" : "",
                 item->name);
        struct stat info = { 0 };
        item->is_dir = stat(full, &info) == 0 && S_ISDIR(info.st_mode);
        item->size = item->is_dir ? 0 : (long)info.st_size;
    }
    closedir(dir);
    qsort(entries, count, sizeof entries[0], entry_order);
    unlock();
    return count;
}

int os_stat(const char *path, long *size) {
    lock();
    struct stat info = { 0 };
    int found = stat(path, &info) == 0;
    unlock();
    if (size) *size = found ? (long)info.st_size : 0;
    return !found ? 0 : S_ISDIR(info.st_mode) ? 2 : 1;
}

/* A folder the Project keeps only while a file is in it, so os_make_dir puts a hidden one, .keep, in
 * a new folder, and os_delete takes it out of a folder with nothing else in it. */
int os_delete(const char *path) {
    lock();
    int result = unlink(path);
    if (result < 0) {
        DirEntry entry;
        char keep[300];
        snprintf(keep, sizeof keep, "%s/.keep", path);
        if (os_list_dir(path, &entry, 1) == 0) unlink(keep);
        rmdir(path);
        result = os_stat(path, NULL) ? -1 : 0;
    }
    unlock();
    return result < 0 ? -1 : 0;
}

int os_rename(const char *from, const char *to) {
    lock();
    int result = rename(from, to);
    unlock();
    return result < 0 ? -1 : 0;
}

int os_make_dir(const char *path) {
    char keep[300];
    lock();
    int result = mkdir(path, 0777);
    snprintf(keep, sizeof keep, "%s/.keep", path);
    if (result == 0) result = os_write_file(keep, "", 0);
    unlock();
    return result < 0 ? -1 : 0;
}

int os_copy_file(const char *from, const char *to) {
    long size;
    char *data = os_read_file(from, &size);
    if (!data) return -1;
    int result = os_write_file(to, data, size);
    os_free(data);
    return result;
}

const char *os_base_name(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int has_extension(const char *path, const char *extension) {
    size_t length = strlen(path), extension_length = strlen(extension);
    if (length < extension_length) return 0;
    for (size_t i = 0; i < extension_length; i++) {
        char c = path[length - extension_length + i];
        if (c >= 'A' && c <= 'Z') c += 32;
        if (c != extension[i]) return 0;
    }
    return 1;
}

int os_app_for_file(const char *path) {
    static const char *const text[] = { ".txt", ".md", ".c", ".h", ".asm", ".s", ".json", ".csv",
                                        ".log", ".ini", ".cfg" };
    if (has_extension(path, ".bmp")) return APP_VIEWER;
    if (has_extension(path, ".mid") || has_extension(path, ".midi")) return APP_MUSIC;
    if (has_extension(path, ".vid")) return APP_VIDEO;
    if (has_extension(path, ".wad")) return APP_DOOM;
    if (has_extension(path, ".riscv")) return APP_IDE;
    for (size_t i = 0; i < sizeof text / sizeof text[0]; i++)
        if (has_extension(path, text[i])) return APP_NOTEPAD;
    return APP_HEX;
}

int os_app_named(const char *name) {
    for (int i = 0; i < APP_COUNT; i++) {
        if (APPS[i].command && strcmp(name, APPS[i].command) == 0) return i;
        if (strcmp(name, APPS[i].name) == 0) return i;
    }
    return -1;
}

/* ------------------------------------------------------------------ time */

void os_time(DateTime *now) {
    lock();
    time_t seconds = time(NULL);
    struct tm *parts = gmtime(&seconds);
    now->year = parts->tm_year + 1900;
    now->month = parts->tm_mon + 1;
    now->day = parts->tm_mday;
    now->weekday = parts->tm_wday;
    now->hour = parts->tm_hour;
    now->minute = parts->tm_min;
    now->second = parts->tm_sec;
    unlock();
}

/* ------------------------------------------------------------------ messages */

/* Puts a message in a process's queue and wakes it if it waits for one. Moves are folded into a
 * move already waiting, and a timer's into one of the same timer, so a busy process never finds a
 * backlog of either. The system calls this; a process sending one holds the lock. */
static void post_message(int pid, const Message *message) {
    Process *process = process_of(pid);
    if (!process) return;
    if (message->type == MSG_MOUSE_MOVE || message->type == MSG_TIMER) {
        for (int i = 0; i < process->count; i++) {
            Message *queued = &process->queue[(process->head + i) % QUEUE_SIZE];
            if (queued->type == message->type && queued->win == message->win &&
                (message->type == MSG_MOUSE_MOVE || queued->id == message->id)) {
                if (message->type == MSG_MOUSE_MOVE) *queued = *message;
                return;
            }
        }
    }
    if (process->count == QUEUE_SIZE) return;
    process->queue[(process->head + process->count++) % QUEUE_SIZE] = *message;
    if (process->state == PROC_WAITING && process->waiting == WAIT_MESSAGE)
        process->state = PROC_READY;
}

void wm_take_retired(Window *win);   /* wm.c: frees pictures a resize replaced */

void os_get_message(Message *message) {
    Process *process = current;
    for (;;) {
        lock();
        if (process->count) {
            *message = process->queue[process->head];
            process->head = (process->head + 1) % QUEUE_SIZE;
            process->count--;
            if (message->type == MSG_RESIZE && message->win) wm_take_retired(message->win);
            unlock();
            return;
        }
        process->state = PROC_WAITING;
        process->waiting = WAIT_MESSAGE;
        kernel_lock--;
        yield();
    }
}

int os_peek_message(Message *message) {
    Process *process = current;
    lock();
    int got = process->count > 0;
    if (got) {
        *message = process->queue[process->head];
        process->head = (process->head + 1) % QUEUE_SIZE;
        process->count--;
        if (message->type == MSG_RESIZE && message->win) wm_take_retired(message->win);
    }
    unlock();
    return got;
}

/* Waits for the answer of a dialog or a menu the system shows for this process. The caller holds
 * the lock, which this lets go of. */
static int wait_for_answer(void) {
    Process *process = current;
    process->answer = 0;
    process->state = PROC_WAITING;
    process->waiting = WAIT_ANSWER;
    kernel_lock--;
    yield();
    return process->answer;
}

/* The system's side: gives a waiting process its answer and lets it run. */
static void answer_process(int pid, int answer) {
    Process *process = process_of(pid);
    if (!process || process->waiting != WAIT_ANSWER) return;
    process->answer = answer;
    process->waiting = WAIT_NONE;
    if (process->state == PROC_WAITING) process->state = PROC_READY;
}

void os_set_timer(Window *win, int id, int ms) {
    lock();
    Timer *slot = NULL;
    for (int i = 0; i < MAX_TIMERS; i++) {
        Timer *timer = &timers[i];
        if (timer->used && timer->pid == current->pid && timer->win == win && timer->id == id)
            slot = timer;
    }
    if (ms <= 0) {
        if (slot) slot->used = 0;
        unlock();
        return;
    }
    for (int i = 0; i < MAX_TIMERS && !slot; i++)
        if (!timers[i].used) slot = &timers[i];
    if (slot) {
        slot->used = 1;
        slot->pid = current->pid;
        slot->win = win;
        slot->id = id;
        slot->interval = ms;
        slot->due = os_now_ms() + ms;
    }
    unlock();
}

/* The system's part: posts the timers that came due; answers how long until the next, or -1. */
static long run_timers(void) {
    long now = os_now_ms(), next = -1;
    for (int i = 0; i < MAX_TIMERS; i++) {
        Timer *timer = &timers[i];
        if (!timer->used) continue;
        if (timer->due <= now) {
            Message message = { 0 };
            message.type = MSG_TIMER;
            message.win = timer->win;
            message.id = timer->id;
            post_message(timer->pid, &message);
            timer->due = now + timer->interval;
        }
        if (next < 0 || timer->due - now < next) next = timer->due - now;
    }
    return next;
}

static void forget_window_timers(Window *win) {
    for (int i = 0; i < MAX_TIMERS; i++)
        if (timers[i].used && timers[i].win == win) timers[i].used = 0;
}

/* ------------------------------------------------------------------ the clipboard */

static int clip_kind;
static char *clip_text;
static int clip_length;
static uint32_t *clip_pixels;
static int clip_w, clip_h;

static void clip_clear(void) {
    free(clip_text);
    free(clip_pixels);
    clip_text = NULL;
    clip_pixels = NULL;
    clip_kind = CLIP_NONE;
}

int os_clipboard_kind(void) { return clip_kind; }

void os_clipboard_set_text(const char *text, int length) {
    lock();
    clip_clear();
    clip_text = malloc(length + 1);
    if (clip_text) {
        memcpy(clip_text, text, length);
        clip_text[length] = 0;
        clip_length = length;
        clip_kind = CLIP_TEXT;
    }
    unlock();
}

char *os_clipboard_text(int *length) {
    lock();
    char *copy = NULL;
    if (clip_kind == CLIP_TEXT && (copy = os_alloc(clip_length + 1))) {
        memcpy(copy, clip_text, clip_length + 1);
        if (length) *length = clip_length;
    }
    unlock();
    return copy;
}

void os_clipboard_set_picture(const uint32_t *pixels, int w, int h) {
    lock();
    clip_clear();
    clip_pixels = malloc((size_t)w * h * 4);
    if (clip_pixels) {
        memcpy(clip_pixels, pixels, (size_t)w * h * 4);
        clip_w = w;
        clip_h = h;
        clip_kind = CLIP_PICTURE;
    }
    unlock();
}

uint32_t *os_clipboard_picture(int *w, int *h) {
    lock();
    uint32_t *copy = NULL;
    if (clip_kind == CLIP_PICTURE && (copy = os_alloc((size_t)clip_w * clip_h * 4))) {
        memcpy(copy, clip_pixels, (size_t)clip_w * clip_h * 4);
        *w = clip_w;
        *h = clip_h;
    }
    unlock();
    return copy;
}

/* ------------------------------------------------------------------ settings */

static Settings settings = { WALLPAPER_TEAL, "pictures/sunset.bmp", THEME_CLASSIC, 450, 1, 1 };
static const char SETTINGS_FILE[] = "asmos.ini";

const Settings *os_settings(void) { return &settings; }

static const uint32_t THEMES[THEME_COUNT][5] = {
    [THEME_CLASSIC] = { 0x000080, 0x1084d0, 0x808080, 0xb4b4b4, 0x000080 },
    [THEME_OLIVE] = { 0x385020, 0x8fa860, 0x7d7d6e, 0xb0b09a, 0x506030 },
    [THEME_PLUM] = { 0x581848, 0xb05890, 0x806878, 0xb8a8b0, 0x702060 },
    [THEME_SLATE] = { 0x203040, 0x6888a8, 0x707880, 0xa8b0b8, 0x305070 },
};

uint32_t os_theme_colour(int which) { return THEMES[settings.theme][which]; }

static void settings_load(void) {
    FILE *file = fopen(SETTINGS_FILE, "r");
    if (!file) return;
    char line[128];
    while (fgets(line, sizeof line, file)) {
        char key[32], value[96];
        if (sscanf(line, "%31[^=]=%95[^\n]", key, value) != 2) continue;
        if (!strcmp(key, "wallpaper")) settings.wallpaper = clamp_int(atoi(value), 0, WALLPAPER_COUNT - 1);
        else if (!strcmp(key, "picture")) snprintf(settings.picture, sizeof settings.picture, "%.63s", value);
        else if (!strcmp(key, "theme")) settings.theme = clamp_int(atoi(value), 0, THEME_COUNT - 1);
        else if (!strcmp(key, "double_click_ms")) settings.double_click_ms = clamp_int(atoi(value), 200, 900);
        else if (!strcmp(key, "clock_24h")) settings.clock_24h = atoi(value) != 0;
        else if (!strcmp(key, "sounds")) settings.sounds = atoi(value) != 0;
    }
    fclose(file);
}

void wm_settings_changed(void);

void os_apply_settings(const Settings *changed) {
    lock();
    settings = *changed;
    FILE *file = fopen(SETTINGS_FILE, "w");
    if (file) {
        fprintf(file, "wallpaper=%d\npicture=%s\ntheme=%d\ndouble_click_ms=%d\nclock_24h=%d\nsounds=%d\n",
                settings.wallpaper, settings.picture, settings.theme, settings.double_click_ms,
                settings.clock_24h, settings.sounds);
        fclose(file);
    }
    wm_settings_changed();
    Message message = { 0 };
    message.type = MSG_SETTINGS;
    for (int i = 1; i < MAX_PROCESSES; i++)
        if (processes[i].used && processes[i].state != PROC_ENDED)
            post_message(processes[i].pid, &message);
    unlock();
}

/* ------------------------------------------------------------------ sampled sound */

static int pcm_fd = -1, pcm_owner = -1;

int os_pcm_open(int rate) {
    lock();
    if (pcm_fd >= 0) {
        int busy = pcm_owner != current->pid;
        unlock();
        return busy ? -1 : 0;
    }
    pcm_fd = open("/dev/dsp", O_WRONLY);
    if (pcm_fd < 0) {
        unlock();
        return -1;
    }
    int format = AFMT_S16_LE, channels = 1, speed = rate;
    /* six fragments, asked for in the device's starting format, which the 16-bit format and the
     * rate below then scale to 2 KB each: 1,024 samples, under a twentieth of a second at 22 kHz,
     * since /dev/dsp plays whole fragments and a larger one only makes the sound later */
    int fragments = 6 << 16 | 10;
    ioctl(pcm_fd, SNDCTL_DSP_SETFRAGMENT, &fragments);
    ioctl(pcm_fd, SNDCTL_DSP_SETFMT, &format);
    ioctl(pcm_fd, SNDCTL_DSP_CHANNELS, &channels);
    ioctl(pcm_fd, SNDCTL_DSP_SPEED, &speed);
    pcm_owner = current->pid;
    unlock();
    return 0;
}

int os_pcm_space(void) {
    lock();
    audio_buf_info room = { 0 };
    int space = pcm_fd >= 0 && pcm_owner == current->pid &&
                        ioctl(pcm_fd, SNDCTL_DSP_GETOSPACE, &room) == 0
                    ? room.bytes / 2
                    : 0;
    unlock();
    return space;
}

int os_pcm_write(const int16_t *samples, int count) {
    int space = os_pcm_space();
    if (count > space) count = space;
    if (count <= 0) return 0;
    lock();
    ssize_t written = write(pcm_fd, samples, (size_t)count * 2);
    unlock();
    return written > 0 ? (int)(written / 2) : 0;
}

void pcm_release(int pid) {
    if (pcm_fd >= 0 && pcm_owner == pid) {
        /* what is still buffered is dropped: close would wait for it to play */
        ioctl(pcm_fd, SNDCTL_DSP_RESET, NULL);
        close(pcm_fd);
        pcm_fd = -1;
        pcm_owner = -1;
    }
}

void os_pcm_close(void) {
    lock();
    pcm_release(current->pid);
    unlock();
}

void os_default_message(Message *message) {
    if (message->type == MSG_QUIT) os_exit(0);
    if (message->type == MSG_CLOSE && message->win) {
        os_window_close(message->win);
        lock();
        int left = wm_window_count(current->pid);
        unlock();
        if (!left) os_exit(0);
    }
}
