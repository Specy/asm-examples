/*
 * The kernel: processes and the scheduler, and the services processes ask for through the os_
 * calls of os.h (memory, files, messages, timers, the clipboard, settings, sound).
 *
 * Every application runs as a process with a stack of its own. Process 0 is the system: the main
 * loop of main.c, which reads the keyboard and the pointer, delivers messages and composes the
 * screen. The others are applications. One of them runs at a time. A process gives the processor
 * up when it waits for a message or sleeps, and on a machine with a timer (PLATFORM_PREEMPTIVE)
 * the timer ticks every 10 ms while applications are ready to run, and takes the processor from
 * one that does not give it up: preemption. The platform layer does the switch itself, its own
 * way (os/platform/): on x86 a signal handler rewrites the registers the signal frame holds, and
 * on a machine without a timer a call keeps the callee-saved registers. The kernel only says which
 * process runs next, in switch_to_next.
 *
 * A process may be stopped at any instruction, so the kernel's own data is changed only under the
 * kernel lock: an os_ call takes it, and the timer leaves a process holding it alone until it lets
 * go. The system process is never preempted: it gives the processor up itself, between two passes
 * of its loop, when an application is ready. Without a timer, every process does so: when it
 * waits, and at the yield points of its long loops (os_yield_point), once it has had its turn.
 */

enum { MAX_PROCESSES = PLATFORM_MAX_PROCESSES, QUEUE_SIZE = PLATFORM_QUEUE_SIZE, MAX_TIMERS = 64 };
enum { TURN_MS = 10 };                /* a turn, between two ticks of the timer */
enum { WAIT_NONE, WAIT_MESSAGE, WAIT_ANSWER };

/* What os_alloc puts before the memory it hands out: 32 bytes on x86-64, 16 on a 32-bit machine,
 * so the memory keeps the 16-byte alignment malloc gave the block. */
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
    PlatformContext context;         /* its registers, while it does not run */
    Message queue[QUEUE_SIZE];
    int head, count;
    int waiting;
    int answer;
    int midi_used;                   /* it sent MIDI: its notes are silenced when it ends */
    long wake_at;
    Allocation allocations;          /* a ring: its own memory, freed when the process ends */
    long memory;
    uint64_t cpu, cpu_mark;          /* instructions run: all, and all at the last second */
    long cpu_rate;                   /* in the last second */
    GfxContext gfx;
} Process;

static Process processes[MAX_PROCESSES];
static Process *volatile current = &processes[0];
#define SYSTEM (&processes[0])
static int next_pid = 1;
static volatile int kernel_lock, need_resched;
static uint64_t switched_at;          /* the instruction count at the last switch */
static long turn_started;             /* the clock when the current process got the processor */
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

long os_now_ms(void) { return platform_now_ms() - boot_ms; }

/* ------------------------------------------------------------------ switching */

/* The profiler's samples: where the timer found the process os_profile was called by. A machine
 * with no timer takes none, and keeps no room for them. */
enum { PROFILE_MAX = PLATFORM_PREEMPTIVE ? 4096 : 1 };
static uint64_t profile_samples[PROFILE_MAX];
static int profile_count, profile_pid = -1;

static void lock(void) { kernel_lock++; }
static void yield(void);

/* Lets go of the kernel lock, and gives the processor up if the timer asked for it meanwhile. */
static void unlock(void) {
    if (--kernel_lock == 0 && need_resched && current != SYSTEM) {
        need_resched = 0;
        yield();
    }
}

/* Starts the timer while applications are ready to run, and stops it while none is, so that a
 * desktop with nothing to do does nothing. A machine without one has nothing to start. */
static void timer_set(int on) {
    if (on == timer_on) return;
    timer_on = on;
#if PLATFORM_PREEMPTIVE
    platform_timer(on ? TURN_MS : 0);
#endif
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

/* Picks the process that runs next, round the table, and makes it the current one: answers it,
 * or NULL when the current one goes on. Both kinds of switch below come here. */
static Process *switch_to_next(void) {
    Process *from = current;
    if (from->state == PROC_RUNNING) from->state = PROC_READY;
    Process *to = next_ready();
    if (!to) to = from->state == PROC_READY ? from : SYSTEM;
    if (to == from) {
        from->state = PROC_RUNNING;
        return NULL;
    }
    uint64_t now = platform_instructions();
    from->cpu += now - switched_at;
    switched_at = now;
    to->state = PROC_RUNNING;
    current = to;
    gfx = to == SYSTEM ? &gfx_system : &to->gfx;
#if !PLATFORM_PREEMPTIVE
    turn_started = os_now_ms();
#endif
    return to;
}

#if PLATFORM_PREEMPTIVE

/* The platform's trap handler asks here: the timer's tick (by_timer), or a process giving the
 * processor up. The tick leaves the system process alone, and one holding the kernel lock until it
 * lets go (unlock). The profiler notes where the tick found the process it watches. */
int kernel_switch(int by_timer, uintptr_t pc, PlatformContext **from, PlatformContext **to) {
    if (by_timer && current->pid == profile_pid && profile_count < PROFILE_MAX)
        profile_samples[profile_count++] = pc;
    if (by_timer) {
        if (current == SYSTEM) return 0;
        if (kernel_lock) {
            need_resched = 1;
            return 0;
        }
    }
    Process *was = current, *next = switch_to_next();
    if (!next) return 0;
    *from = &was->context;
    *to = &next->context;
    return 1;
}

/* Giving the processor up is a trap into the platform's handler, as the timer's tick is. */
static void yield(void) { platform_trap(); }

#else

/* Giving the processor up is a call, which returns once the kernel switches back. */
static void yield(void) {
    Process *was = current, *next = switch_to_next();
    if (next) platform_switch(&was->context, &next->context);
}

#endif

void os_yield_point(void) {
#if !PLATFORM_PREEMPTIVE
    if (current != SYSTEM && !kernel_lock && os_now_ms() - turn_started >= TURN_MS) yield();
#endif
}

/* Whether the heap holds what asmOS needs before it can show anything: `screen_bytes` for the back
 * buffer, and room for the first applications' stacks and memory. Where the heap is small, as on
 * MARS and RARS, which give static data and the heap 4 MB together, asmOS says what it needs and
 * stops rather than run out half way. The kernel's own tables are static data, already there. */
static int kernel_memory_check(size_t screen_bytes) {
    size_t left = platform_memory_left();
    size_t need = screen_bytes + 2 * PLATFORM_STACK_SIZE + 256 * 1024;
    if (left >= need) return 1;
    char line[200];
    snprintf(line, sizeof line,
             "asmOS needs %lu KB of memory to start, %lu KB of it for the screen; %lu KB are left "
             "(the process table takes %lu KB)\n",
             (unsigned long)(need / 1024), (unsigned long)(screen_bytes / 1024),
             (unsigned long)(left / 1024), (unsigned long)(sizeof processes / 1024));
    platform_print(line);
    return 0;
}

static void system_reserve_refill(void);   /* below, with the memory */

static void kernel_start(void) {
    boot_ms = 0;
    boot_ms = os_now_ms();
    Process *system = SYSTEM;
    system->used = 1;
    system->pid = 0;
    system->state = PROC_RUNNING;
    snprintf(system->name, sizeof system->name, "System");
    system->allocations.next = system->allocations.prev = &system->allocations;
#if PLATFORM_PREEMPTIVE
    platform_scheduler_start();
#endif
    switched_at = platform_instructions();
    system_reserve_refill();
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
    /* an application the machine leaves out has no main (apps.c) */
    if (app < 0 || app >= APP_COUNT || !APPS[app].main) return -1;
    lock();
    Process *process = NULL;
    for (int i = 1; i < MAX_PROCESSES && !process; i++)
        if (!processes[i].used) process = &processes[i];
    uint8_t *stack = process ? platform_stack_alloc(PLATFORM_STACK_SIZE) : NULL;
    if (!process || !stack) {
        if (stack) platform_stack_free(stack);
        unlock();
        os_beep();
        return -1;
    }
    memset(process, 0, sizeof *process);
    process->used = 1;
    process->pid = next_pid++;
    process->app = app;
    process->stack = stack;
    platform_context_init(&process->context, stack, PLATFORM_STACK_SIZE, process_start);
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
            platform_stack_free(process->stack);
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

uint64_t os_instructions(void) {
    lock();     /* no switch between reading the two */
    uint64_t count = current->cpu + platform_instructions() - switched_at;
    unlock();
    return count;
}

/* Once a second: what each process ran in the second gone, and how busy the processor was. */
static void measure_load(long elapsed_ms) {
    uint64_t now = platform_instructions();
    current->cpu += now - switched_at;
    switched_at = now;
    load_total = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        Process *process = &processes[i];
        if (!process->used) continue;
        process->cpu_rate = (long)(process->cpu - process->cpu_mark);
        process->cpu_mark = process->cpu;
        load_total += process->cpu_rate;
    }
    load_busy = elapsed_ms > 0 ? clamp_int((int)(100 - idle_ms * 100 / elapsed_ms), 0, 100) : 0;
    idle_ms = 0;
    system_reserve_refill();
}

/* ------------------------------------------------------------------ memory */

/* Where all of the heap is a few megabytes (PLATFORM_SMALL_MEMORY), the applications can use it
 * up, and then the system could not even show a message box saying so. So the kernel keeps a
 * reserve, enough for one, which the system's own windows take when nothing else is left and which
 * it takes back once a second, when memory is free again. */
enum { SYSTEM_RESERVE = PLATFORM_SMALL_MEMORY ? 224 * 1024 : 0 };
static void *system_reserve;

static void system_reserve_refill(void) {
    if (SYSTEM_RESERVE && !system_reserve) system_reserve = malloc(SYSTEM_RESERVE);
}

/* malloc for the system's own windows: the reserve goes when nothing else is left. */
static void *system_alloc(size_t size) {
    void *memory = malloc(size);
    if (!memory && system_reserve) {
        free(system_reserve);
        system_reserve = NULL;
        memory = malloc(size);
    }
    return memory;
}

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

/* A whole file in memory from `allocate`, with a 0 after it; NULL if it cannot be read. */
static char *read_file_with(const char *path, long *size, void *(*allocate)(size_t),
                            void (*release)(void *)) {
    long length = platform_file_size(path);
    char *data = length >= 0 ? allocate(length + 1) : NULL;
    if (data && platform_file_read(path, data, length) != length) {
        release(data);
        data = NULL;
    }
    if (data) data[length] = 0;
    if (size) *size = data ? length : 0;
    return data;
}

char *os_read_file(const char *path, long *size) {
    lock();
    char *data = read_file_with(path, size, os_alloc, os_free);
    unlock();
    return data;
}

int os_write_file(const char *path, const void *data, long size) {
    lock();
    int result = platform_file_write(path, data, size);
    unlock();
    return result;
}

int os_file_system(void) { return PLATFORM_FILE_SYSTEM; }

static int entry_order(const void *a, const void *b) {
    const DirEntry *x = a, *y = b;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;
    return strcmp(x->name, y->name);
}

int os_list_dir(const char *path, DirEntry *entries, int max) {
    if (!(PLATFORM_FILE_SYSTEM & FS_LIST)) return -1;
    lock();
    int count = platform_list_dir(path, entries, max);
    if (count > 0) qsort(entries, count, sizeof entries[0], entry_order);
    unlock();
    return count;
}

int os_stat(const char *path, long *size) {
    lock();
    int kind = platform_stat(path, size);
    unlock();
    return kind;
}

int os_delete(const char *path) {
    if (!(PLATFORM_FILE_SYSTEM & FS_DELETE)) return -1;
    lock();
    int result = platform_delete(path);
    unlock();
    return result;
}

int os_rename(const char *from, const char *to) {
    if (!(PLATFORM_FILE_SYSTEM & FS_RENAME)) return -1;
    lock();
    int result = platform_rename(from, to);
    unlock();
    return result;
}

int os_make_dir(const char *path) {
    if (!(PLATFORM_FILE_SYSTEM & FS_MAKE_DIR)) return -1;
    lock();
    int result = platform_make_dir(path);
    unlock();
    return result;
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
    if (has_extension(path, ".vid")) return APPS[APP_VIDEO].main ? APP_VIDEO : APP_HEX;
    if (has_extension(path, ".wad")) return APPS[APP_DOOM].main ? APP_DOOM : APP_HEX;
    if (has_extension(path, ".riscv")) return APP_IDE;
    for (size_t i = 0; i < sizeof text / sizeof text[0]; i++)
        if (has_extension(path, text[i])) return APP_NOTEPAD;
    return APP_HEX;
}

int os_app_named(const char *name) {
    for (int i = 0; i < APP_COUNT; i++) {
        if (!APPS[i].main) continue;
        if (APPS[i].command && strcmp(name, APPS[i].command) == 0) return i;
        if (strcmp(name, APPS[i].name) == 0) return i;
    }
    return -1;
}

/* ------------------------------------------------------------------ time */

/* The calendar date of a time in seconds since 1970 began, in UTC. The days are counted in eras
 * of 400 years from 1 March of year 0, since every era has the same 146,097 days and a year that
 * starts in March has its leap day at its end (Howard Hinnant's days_from_civil, backwards). */
static void date_from_seconds(int64_t seconds, DateTime *date) {
    long days = (long)(seconds / 86400), second = (long)(seconds % 86400);
    if (second < 0) {
        second += 86400;
        days--;
    }
    date->hour = (int)(second / 3600);
    date->minute = (int)(second / 60 % 60);
    date->second = (int)(second % 60);
    date->weekday = (int)((days % 7 + 11) % 7);             /* 1 January 1970 was a Thursday */
    days += 719468;                                         /* days since 1 March of year 0 */
    long era = (days >= 0 ? days : days - 146096) / 146097;
    long day_of_era = days - era * 146097;                  /* 0 to 146,096 */
    long year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    long day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    long month = (5 * day_of_year + 2) / 153;               /* 0 is March */
    date->day = (int)(day_of_year - (153 * month + 2) / 5 + 1);
    date->month = (int)(month < 10 ? month + 3 : month - 9);
    date->year = (int)(year_of_era + era * 400 + (date->month <= 2));
}

void os_time(DateTime *now) {
    lock();
    date_from_seconds(platform_time(), now);
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
#if !PLATFORM_PREEMPTIVE
    os_yield_point();                 /* a process whose messages keep coming waits for none */
#endif
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
#if !PLATFORM_PREEMPTIVE
    os_yield_point();                 /* a frame loop's: it peeks every frame */
#endif
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
    char *text = read_file_with(SETTINGS_FILE, NULL, malloc, free);
    if (!text) return;
    for (char *line = text, *next; *line; line = next) {
        next = line + strcspn(line, "\n");
        if (*next) *next++ = 0;
        char key[32], value[96];
        if (sscanf(line, "%31[^=]=%95[^\n]", key, value) != 2) continue;
        if (!strcmp(key, "wallpaper")) settings.wallpaper = clamp_int(atoi(value), 0, WALLPAPER_COUNT - 1);
        else if (!strcmp(key, "picture")) snprintf(settings.picture, sizeof settings.picture, "%.63s", value);
        else if (!strcmp(key, "theme")) settings.theme = clamp_int(atoi(value), 0, THEME_COUNT - 1);
        else if (!strcmp(key, "double_click_ms")) settings.double_click_ms = clamp_int(atoi(value), 200, 900);
        else if (!strcmp(key, "clock_24h")) settings.clock_24h = atoi(value) != 0;
        else if (!strcmp(key, "sounds")) settings.sounds = atoi(value) != 0;
    }
    free(text);
}

void wm_settings_changed(void);

void os_apply_settings(const Settings *changed) {
    lock();
    settings = *changed;
    char text[256];
    int length = snprintf(text, sizeof text,
                          "wallpaper=%d\npicture=%s\ntheme=%d\ndouble_click_ms=%d\nclock_24h=%d\nsounds=%d\n",
                          settings.wallpaper, settings.picture, settings.theme, settings.double_click_ms,
                          settings.clock_24h, settings.sounds);
    platform_file_write(SETTINGS_FILE, text, length);
    wm_settings_changed();
    Message message = { 0 };
    message.type = MSG_SETTINGS;
    for (int i = 1; i < MAX_PROCESSES; i++)
        if (processes[i].used && processes[i].state != PROC_ENDED)
            post_message(processes[i].pid, &message);
    unlock();
}

/* ------------------------------------------------------------------ sampled sound */

static int pcm_owner = -1;            /* the process that has the platform's sampled sound */

int os_pcm_open(int rate) {
    lock();
    if (pcm_owner >= 0) {
        int busy = pcm_owner != current->pid;
        unlock();
        return busy ? -1 : 0;
    }
    if (platform_pcm_open(rate) < 0) {
        unlock();
        return -1;
    }
    pcm_owner = current->pid;
    unlock();
    return 0;
}

int os_pcm_space(void) {
    lock();
    int space = pcm_owner >= 0 && pcm_owner == current->pid ? platform_pcm_space() : 0;
    unlock();
    return space;
}

int os_pcm_write(const int16_t *samples, int count) {
    int space = os_pcm_space();
    if (count > space) count = space;
    if (count <= 0) return 0;
    lock();
    int written = platform_pcm_write(samples, count);
    unlock();
    return written;
}

void pcm_release(int pid) {
    if (pcm_owner >= 0 && pcm_owner == pid) {
        platform_pcm_close();
        pcm_owner = -1;
    }
}

void os_pcm_close(void) {
    lock();
    pcm_release(current->pid);
    unlock();
}

/* ------------------------------------------------------------------ the machine */

const char *os_machine(int what) {
    static const char *const words[MACHINE_LINES] = PLATFORM_MACHINE;
    return what >= 0 && what < MACHINE_LINES ? words[what] : "";
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
