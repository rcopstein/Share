//
// File Operations Module
//

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "output.h"

static bool startsWith(const char *pre, const char *str) {
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

static int fops_handle_error(int err) {
    switch (err) {

        case ENOENT:
            return error("No such file or directory...\n", NULL);

        case EACCES:
            return error("Permission Denied...\n", NULL);

        default:
            return error("Fatal error %d...\n", &err);
    }
}

int fops_make_dir(const char* dirname) {
    struct stat st = {0};
    int res = stat(dirname, &st);
    printf("Attempting to create %s\n", dirname);

    if (res == 0) { printf("Directory exists!\n"); return 1; } // Directory exists
    if (errno != ENOENT) { printf("Error: %d\n", errno); return 1; } // Unknown error

    if (mkdir(dirname, 0700) == 0) return 0;
    printf("Error 2: %d\n", errno);
    return 1;
}

int fops_remove_dir(const char* dirname) {
    if(rmdir(dirname) == 0) return 0;
    return 1;
}

FILE* fops_make_file(const char* filename) {
    FILE* file = fopen(filename, "w");
    return file;
}

int fops_remove_file(const char* filename) {
    return remove(filename);
}

int fops_append_line(const char* filename, const char* line) {
    FILE* file = fopen(filename, "a");
    if (!file) { printf("Failed to open file '%s'\n", filename); return fops_handle_error(errno); }
    fwrite(line, sizeof(char), strlen(line), file);
    fclose(file);
    return 0;
}

int fops_delete_line(const char* filename, const char* line) {
    char *buf;
    int result;
    size_t size;
    FILE *file, *cpy;
    const char *tempFile = "replica.temp";

    // Open original file
    file = fopen(filename, "r");
    if (!file) { printf("Failed to open file '%s'\n", filename); return fops_handle_error(errno); }

    // Create replica file
    cpy = fopen(tempFile, "w+");
    if (!cpy) { printf("Failed to create '%s' file\n", tempFile); fclose(file); return fops_handle_error(errno); }

    // Allocate buffer
    size = 50;
    buf = (char*) malloc(sizeof(char) * size);

    // Copy every line to new file (except the one to remove)
    while (getline(&buf, &size, file) != -1)
    {
        if (strcmp(line, buf) == 0) continue;
        fwrite(buf, sizeof(char), strlen(buf), cpy);
    }

    // Close files
    fclose(file);
    fclose(cpy);

    // Remove original file and rename new one
    result = remove(filename);
    if (result) { remove(tempFile); return fops_handle_error(errno); }

    result = rename(tempFile, filename);
    if (result) { remove(tempFile); return fops_handle_error(errno); }

    return 0;
}

int fops_delete_line_starts_with(const char* filename, const char* line) {
    char *buf;
    int result;
    size_t size;
    FILE *file, *cpy;
    const char *tempFile = "replica.temp";

    // Open original file
    file = fopen(filename, "r");
    if (!file) { printf("Failed to open file '%s'\n", filename); return fops_handle_error(errno); }

    // Create replica file
    cpy = fopen(tempFile, "w+");
    if (!cpy) { printf("Failed to create '%s' file\n", tempFile); fclose(file); return fops_handle_error(errno); }

    // Allocate buffer
    size = 50;
    buf = (char*) malloc(sizeof(char) * size);

    // Copy every line to new file (except the one to remove)
    while (getline(&buf, &size, file) != -1)
    {
        if (startsWith(line, buf)) continue;
        fwrite(buf, sizeof(char), strlen(buf), cpy);
    }

    // Close files
    fclose(file);
    fclose(cpy);

    // Remove original file and rename new one
    result = remove(filename);
    if (result) { remove(tempFile); return fops_handle_error(errno); }

    result = rename(tempFile, filename);
    if (result) { remove(tempFile); return fops_handle_error(errno); }

    return 0;
}

int fops_get_line_starts_with(char* path, const char* line, char* buffer, uint16_t size) {

    // Open the file
    FILE* file = fopen(path, "r");
    if (!file) { error("Failed to open file '%s'\n", path); return NULL; }

    // Read every line
    while (fgets(buffer, size, file)) {
        if (startsWith(line, buffer)) {
            return 0;
        }
    }

    return 1;

}