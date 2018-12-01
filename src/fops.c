#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "output.h"

// Support functions

static bool matches(const char *pre, const char *str) {

    size_t lenpre = strlen(pre);
    if (strncmp(pre, str, lenpre) != 0) return false;

    if (str[lenpre] == ' '  ||
        str[lenpre] == '\n' ||
        str[lenpre] == '\0') return true;

    return false;
}

static int lock_file(int file, short type) {

    struct flock lock = {
            .l_len = 0,
            .l_start = 0,
            .l_type = type,
            .l_pid = getpid(),
            .l_whence = SEEK_SET,
    };

    return fcntl(file, F_SETLKW, &lock);
}

//
// File locking with fcntl is not supported with stdio operations
// With that said, I believe that porting all functions to open/read/write
// is the best strategy
//
// According to other sources, if I don't explicitly unlock the file after using fclose(),
// then fcntl() will release the lock after the content has been actually written to disk.
// The source for fcntl() actually just say that buffering with stdio functions will cause
// some trouble, but this second strategy should work around that.
//

// Methods

int fops_exists_dir(const char* dirname) {

    struct stat st;
    if (!stat(dirname, &st) || errno != ENOENT) return 1;
    return 0;

}

int fops_make_dir(const char* dirname) {

    if (!fops_exists_dir(dirname)) return error("Directory '%s' already exists!\n", (char *) dirname);
    return mkdir(dirname, 0700);

}

int fops_remove_dir(const char* dirname) {
    return remove(dirname);
}

int fops_append_line(const char* filename, const char* line) {

    int file = open(filename, O_APPEND);
    if (file < 0) return error("File '%s' not found!\n", (char *) filename);

    int result = 0;
    int ack_lock = lock_file(file, F_WRLCK);

    if (ack_lock > 0) write(file, line, sizeof(char) * strlen(line));
    else result = error("Failed to acquire file lock!\n", NULL);

    close(file);
    return result;

    // There is no call for unlocking on purpose. fcntl should release the lock automatically
    // after the call for close.
}

int fops_read_line(const char* filename, const char* prefix, char* buffer, size_t size) {

    FILE* file = fopen(filename, "r");
    if (file == NULL) return error("Failed to open file '%s'!\n", (char *) filename);

    int result = 1;
    int ack_lock = lock_file(fileno(file), F_RDLCK);

    if (ack_lock > 0) {
        while (getline(&buffer, &size, file)) {
            if (matches(prefix, buffer)) {
                result = 0;
                break;
            }
        }

    } else result = error("Failed to acquire lock!\n", NULL);

    fclose(file);
    return result;

    // There is no call for unlocking on purpose. fcntl should release the lock automatically
    // after the call for fclose.
}

int fops_update_line(const char* filename, const char* prefix, char* (*funct)(char *)) {

    FILE* file = fopen(filename, "r+");
    if (file == NULL) { printf("Error is %d\n", errno); return error("Failed to open file '%s'!\n", (char *) filename); }
    if (lock_file(fileno(file), F_WRLCK) < 0) { return error("Failed to acquire lock for '%s'!\n", (char *) filename); }

    char* tempPath = "temp";
    FILE* tempFile = fopen(tempPath, "w+");
    if (tempFile == NULL) { fclose(file); return error("Failed to create a temporary file!\n", NULL); }
    if (lock_file(fileno(file), F_WRLCK) < 0) { fclose(file); return error("Failed to acquire lock for temp file!\n", NULL); }

    bool flag;
    char* line;
    bool found = false;
    size_t size = 1024;
    char* buffer = (char *) malloc(sizeof(char) * size);

    while (getline(&buffer, &size, file) > 0) {

        flag = false;
        line = buffer;

        if (matches(prefix, buffer)) {
            line = funct == NULL ? NULL : funct(buffer);
            found = true;
            flag = true;
        }

        if (line != NULL) {
            fwrite(line, sizeof(char), strlen(line), tempFile);
            if (flag) free(line);
        }
    }

    if (!found && funct != NULL) {
        line = funct(NULL);

        if (line != NULL) {
            fwrite(line, sizeof(char), strlen(line), tempFile);
            printf("'%s' was written!\n", line);
            free(line);
        }
    }

    int result = 0;
    if (rename(tempPath, filename)) {
        result = error("Failed to overwrite original file '%s'!\n", (char *) filename);
        printf("Overwrite original file error: %d\n", errno);
        remove(tempPath);
    }

    fflush(tempFile);
    fflush(file);

    fclose(tempFile);
    fclose(file);
    free(buffer);

    return result;
}

int fops_remove_line(const char* filename, const char* prefix) {
    return fops_update_line(filename, prefix, NULL);
}
