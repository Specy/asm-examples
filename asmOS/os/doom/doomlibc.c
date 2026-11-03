/*
 * The C library DOOM sees (see doomlibc.h), made of os_ calls.
 */
#include "doomlibc.h"

/* What DOOM prints goes nowhere but here: the last of it shows in the message of an error. */
enum { CONSOLE_SIZE = 2048 };
static char doom_console[CONSOLE_SIZE];
static int doom_console_length;
static DoomFile doom_console_file = { .console = 1 };
DoomFile *doom_stdout = &doom_console_file, *doom_stderr = &doom_console_file;

static void doom_console_add(const char *text, long length) {
    if (length >= CONSOLE_SIZE) {
        text += length - (CONSOLE_SIZE - 1);
        length = CONSOLE_SIZE - 1;
    }
    int keep = doom_console_length + length >= CONSOLE_SIZE
                   ? (int)(CONSOLE_SIZE - 1 - length)
                   : doom_console_length;
    if (keep < doom_console_length)
        memmove(doom_console, doom_console + doom_console_length - keep, keep);
    memcpy(doom_console + keep, text, length);
    doom_console_length = keep + (int)length;
    doom_console[doom_console_length] = 0;
}

const char *doom_console_tail(void) {
    return doom_console;
}

/* ------------------------------------------------------------------ memory */

void *doom_malloc(size_t size) {
    return os_alloc(size ? size : 1);
}

void *doom_calloc(size_t count, size_t size) {
    size_t total = count * size;
    return os_alloc(total ? total : 1);
}

void *doom_realloc(void *memory, size_t size) {
    return os_realloc(memory, size ? size : 1);
}

void doom_free(void *memory) {
    os_free(memory);
}

char *doom_strdup(const char *text) {
    size_t length = strlen(text);
    char *copy = os_alloc(length + 1);
    if (copy) memcpy(copy, text, length + 1);
    return copy;
}

int doom_strncasecmp(const char *a, const char *b, size_t n) {
    for (; n; n--, a++, b++) {
        int x = tolower((unsigned char)*a), y = tolower((unsigned char)*b);
        if (x != y || !x) return x - y;
    }
    return 0;
}

int doom_strcasecmp(const char *a, const char *b) {
    return doom_strncasecmp(a, b, (size_t)-1);
}

char *doom_getenv(const char *name) {
    (void)name;
    return NULL;
}

int doom_system(const char *command) {
    (void)command;
    return -1;
}

/* ------------------------------------------------------------------ files */

/* Paths as DOOM makes them, "./doom1.wad" or "games/doom//default.cfg", as the Project's. */
static void doom_clean_path(char *to, const char *from, size_t size) {
    while (from[0] == '.' && from[1] == '/') from += 2;
    size_t n = 0;
    for (; *from && n + 1 < size; from++) {
        if (*from == '/' && n > 0 && to[n - 1] == '/') continue;
        to[n++] = *from;
    }
    to[n] = 0;
}

DoomFile *doom_fopen(const char *path, const char *mode) {
    DoomFile *file = os_alloc(sizeof *file);
    if (!file) return NULL;
    doom_clean_path(file->path, path, sizeof file->path);
    if (mode[0] == 'r') {
        file->data = os_read_file(file->path, &file->size);
        if (!file->data) {
            os_free(file);
            return NULL;
        }
        file->capacity = file->size;
        file->writing = mode[1] == '+' || (mode[1] && mode[2] == '+');
    } else {
        file->writing = 1;
        if (mode[0] == 'a') {
            file->data = os_read_file(file->path, &file->size);
            file->capacity = file->position = file->size;
        }
    }
    return file;
}

int doom_fclose(DoomFile *file) {
    if (!file || file->console) return 0;
    int result = 0;
    if (file->writing) result = os_write_file(file->path, file->data ? file->data : "", file->size);
    os_free(file->data);
    os_free(file);
    return result;
}

size_t doom_fread(void *buffer, size_t size, size_t count, DoomFile *file) {
    if (!size || file->console) return 0;
    long left = file->size - file->position;
    size_t whole = left > 0 ? (size_t)left / size : 0;
    if (whole < count) {
        count = whole;
        file->at_end = 1;
    }
    memcpy(buffer, file->data + file->position, count * size);
    file->position += (long)(count * size);
    return count;
}

/* Room for `size` more bytes at the position. */
static int doom_grow(DoomFile *file, long size) {
    long need = file->position + size;
    if (need <= file->capacity) return 1;
    long capacity = file->capacity ? file->capacity : 256;
    while (capacity < need) capacity *= 2;
    char *data = os_realloc(file->data, capacity);
    if (!data) return 0;
    file->data = data;
    file->capacity = capacity;
    return 1;
}

size_t doom_fwrite(const void *buffer, size_t size, size_t count, DoomFile *file) {
    long length = (long)(size * count);
    if (file->console) {
        doom_console_add(buffer, length);
        return count;
    }
    if (!file->writing || !doom_grow(file, length)) return 0;
    memcpy(file->data + file->position, buffer, length);
    file->position += length;
    if (file->position > file->size) file->size = file->position;
    return count;
}

int doom_fseek(DoomFile *file, long offset, int whence) {
    long base = whence == SEEK_CUR ? file->position : whence == SEEK_END ? file->size : 0;
    if (base + offset < 0) return -1;
    file->position = base + offset;
    file->at_end = 0;
    return 0;
}

long doom_ftell(DoomFile *file) {
    return file->position;
}

int doom_feof(DoomFile *file) {
    return file->at_end;
}

int doom_fgetc(DoomFile *file) {
    if (file->console || file->position >= file->size) {
        file->at_end = 1;
        return EOF;
    }
    return (unsigned char)file->data[file->position++];
}

char *doom_fgets(char *line, int size, DoomFile *file) {
    int n = 0;
    while (n + 1 < size) {
        int ch = doom_fgetc(file);
        if (ch == EOF) break;
        line[n++] = (char)ch;
        if (ch == '\n') break;
    }
    if (!n) return NULL;
    line[n] = 0;
    return line;
}

int doom_fputc(int ch, DoomFile *file) {
    char c = (char)ch;
    return doom_fwrite(&c, 1, 1, file) ? (unsigned char)c : EOF;
}

int doom_fputs(const char *text, DoomFile *file) {
    size_t length = strlen(text);
    return doom_fwrite(text, 1, length, file) == length ? 0 : EOF;
}

int doom_fflush(DoomFile *file) {
    (void)file;
    return 0;
}

int doom_vfprintf(DoomFile *file, const char *format, va_list args) {
    char text[1024];
    int length = vsnprintf(text, sizeof text, format, args);
    if (length < 0) return length;
    if (length >= (int)sizeof text) length = sizeof text - 1;
    doom_fwrite(text, 1, length, file);
    return length;
}

int doom_fprintf(DoomFile *file, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int length = doom_vfprintf(file, format, args);
    va_end(args);
    return length;
}

int doom_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int length = doom_vfprintf(doom_stdout, format, args);
    va_end(args);
    return length;
}

int doom_puts(const char *text) {
    doom_fputs(text, doom_stdout);
    return doom_fputc('\n', doom_stdout);
}

int doom_putchar(int ch) {
    return doom_fputc(ch, doom_stdout);
}

/* A line at a time: what DOOM scans, its configuration, has a setting a line. */
int doom_fscanf(DoomFile *file, const char *format, ...) {
    char line[512];
    if (!doom_fgets(line, sizeof line, file)) return EOF;
    va_list args;
    va_start(args, format);
    int count = vsscanf(line, format, args);
    va_end(args);
    return count;
}

int doom_exists(const char *path) {
    char clean[128];
    doom_clean_path(clean, path, sizeof clean);
    return os_stat(clean, NULL) != 0;
}

int doom_remove(const char *path) {
    char clean[128];
    doom_clean_path(clean, path, sizeof clean);
    return os_delete(clean);
}

int doom_rename(const char *from, const char *to) {
    char a[128], b[128];
    doom_clean_path(a, from, sizeof a);
    doom_clean_path(b, to, sizeof b);
    os_delete(b);
    return os_rename(a, b);
}

int doom_mkdir(const char *path, int mode) {
    (void)mode;
    char clean[128];
    doom_clean_path(clean, path, sizeof clean);
    size_t length = strlen(clean);
    if (length > 1 && clean[length - 1] == '/') clean[length - 1] = 0;
    return os_stat(clean, NULL) == 2 ? 0 : os_make_dir(clean);
}
