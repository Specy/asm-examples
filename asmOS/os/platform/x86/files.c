/*
 * Files on x86 are the Project's: the program's working directory, through Linux's file calls.
 * The editor keeps a folder only while a file is in it, so a new folder gets a hidden file, .keep,
 * and deleting a folder with nothing else in it takes .keep out first.
 */
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

long platform_file_size(const char *path) {
    struct stat info;
    if (stat(path, &info) != 0 || S_ISDIR(info.st_mode)) return -1;
    return (long)info.st_size;
}

long platform_file_read(const char *path, void *data, long size) {
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    long got = (long)fread(data, 1, size, file);
    fclose(file);
    return got;
}

int platform_file_write(const char *path, const void *data, long size) {
    FILE *file = fopen(path, "wb");
    int ok = file && fwrite(data, 1, size, file) == (size_t)size;
    if (file && fclose(file) != 0) ok = 0;
    return ok ? 0 : -1;
}

int platform_stat(const char *path, long *size) {
    struct stat info = { 0 };
    int found = stat(path, &info) == 0;
    if (size) *size = found ? (long)info.st_size : 0;
    return !found ? 0 : S_ISDIR(info.st_mode) ? 2 : 1;
}

int platform_list_dir(const char *path, DirEntry *entries, int max) {
    DIR *dir = opendir(path && path[0] ? path : ".");
    if (!dir) return -1;
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
    return count;
}

int platform_delete(const char *path) {
    if (unlink(path) == 0) return 0;
    DirEntry entry;
    char keep[300];
    snprintf(keep, sizeof keep, "%s/.keep", path);
    if (platform_list_dir(path, &entry, 1) == 0) unlink(keep);
    rmdir(path);
    return platform_stat(path, NULL) ? -1 : 0;
}

int platform_rename(const char *from, const char *to) { return rename(from, to) < 0 ? -1 : 0; }

int platform_make_dir(const char *path) {
    if (mkdir(path, 0777) != 0) return -1;
    char keep[300];
    snprintf(keep, sizeof keep, "%s/.keep", path);
    return platform_file_write(keep, "", 0);
}
