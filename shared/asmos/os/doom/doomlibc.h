/*
 * What DOOM asks of the C library, on asmOS. DOOM is a process like any other application, so it
 * may only use the pure parts of the C library and the os_ calls (see os.h): this File gives it
 * malloc, the stdio of files and the console, exit and the few names the Runtime library lacks,
 * all made of os_ calls, and renames DOOM's calls to them. doomtype.h includes it, so every File of
 * DOOM sees it; doomlibc.c is the other half.
 *
 * A file DOOM opens is read whole into memory with os_read_file, and one it writes is kept in
 * memory and written with os_write_file when it is closed: DOOM reads its IWAD and its
 * configuration, and writes its configuration, screenshots and saved games, no more.
 */
#ifndef DOOM_LIBC_H
#define DOOM_LIBC_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../os.h"

typedef struct {
    char *data;
    long size, capacity, position;
    int writing, console, at_end;
    char path[128];
} DoomFile;

extern DoomFile *doom_stdout, *doom_stderr;

DoomFile *doom_fopen(const char *path, const char *mode);
int doom_fclose(DoomFile *file);
size_t doom_fread(void *buffer, size_t size, size_t count, DoomFile *file);
size_t doom_fwrite(const void *buffer, size_t size, size_t count, DoomFile *file);
int doom_fseek(DoomFile *file, long offset, int whence);
long doom_ftell(DoomFile *file);
int doom_feof(DoomFile *file);
int doom_fgetc(DoomFile *file);
char *doom_fgets(char *line, int size, DoomFile *file);
int doom_fputc(int ch, DoomFile *file);
int doom_fputs(const char *text, DoomFile *file);
int doom_fflush(DoomFile *file);
int doom_vfprintf(DoomFile *file, const char *format, va_list args);
int doom_fprintf(DoomFile *file, const char *format, ...);
int doom_printf(const char *format, ...);
int doom_puts(const char *text);
int doom_putchar(int ch);
int doom_fscanf(DoomFile *file, const char *format, ...);
int doom_remove(const char *path);
int doom_rename(const char *from, const char *to);
int doom_mkdir(const char *path, int mode);

void *doom_malloc(size_t size);
void *doom_calloc(size_t count, size_t size);
void *doom_realloc(void *memory, size_t size);
void doom_free(void *memory);
char *doom_strdup(const char *text);
int doom_strcasecmp(const char *a, const char *b);
int doom_strncasecmp(const char *a, const char *b, size_t n);
char *doom_getenv(const char *name);
int doom_system(const char *command);

/* Whether a file or a folder is there. */
int doom_exists(const char *path);

/* Ends DOOM, after the platform's goodbyes, and an error after showing it (i_asmos.c). */
void doom_exit(int code) __attribute__((noreturn));
void doom_fatal(const char *message) __attribute__((noreturn));

/* The last lines DOOM printed, for the message of an error. */
const char *doom_console_tail(void);

#define FILE DoomFile
#undef stdout
#undef stderr
#define stdout doom_stdout
#define stderr doom_stderr
#define fopen doom_fopen
#define fclose doom_fclose
#define fread doom_fread
#define fwrite doom_fwrite
#define fseek doom_fseek
#define ftell doom_ftell
#define feof doom_feof
#define fgetc doom_fgetc
#undef getc
#define getc doom_fgetc
#define fgets doom_fgets
#define fputc doom_fputc
#undef putc
#define putc doom_fputc
#define fputs doom_fputs
#define fflush doom_fflush
#define vfprintf doom_vfprintf
#define fprintf doom_fprintf
#define printf doom_printf
#define puts doom_puts
#undef putchar
#define putchar doom_putchar
#define fscanf doom_fscanf
#define remove doom_remove
#define rename doom_rename
#define mkdir doom_mkdir

#define malloc doom_malloc
#define calloc doom_calloc
#define realloc doom_realloc
#define free doom_free
#define strdup doom_strdup
#define strcasecmp doom_strcasecmp
#define strncasecmp doom_strncasecmp
#define getenv doom_getenv
#define system doom_system
#define exit doom_exit
#define abort() doom_exit(-1)

#endif
