#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "output.h"

// Support functions

static bool matches(const char *pre, const char *str) {

    size_t lenpre = strlen(pre);
    size_t lenstr = strlen(str);

    if (lenstr < lenpre || strncmp(pre, str, lenpre) != 0) return false;

    if (str[lenpre] == ' '  ||
        str[lenpre] == '\n' ||
        str[lenpre] == '\0') return true;

    return false;
}

// New

int fops_make_dir(const char* dirname) {

    struct stat st;
    if (stat(dirname, &st) && errno != ENOENT) {
        return error("Directory '%s' already exists!\n", (char *) dirname);
    }

    return mkdir(dirname, 0700);
}

int fops_remove_dir(const char* dirname) {
    return remove(dirname);
}

int fops_append_line(const char* filename, const char* line) {

    FILE* file = fopen(filename, "a");
    if (file == NULL) return error("File '%s' not found!\n", (char *) filename);

    fwrite(line, sizeof(char), strlen(line), file);
    fclose(file);
    return 0;
}

int fops_read_line(const char* filename, const char* prefix, char* buffer, size_t size) {

    FILE* file = fopen(filename, "r");
    if (file == NULL) return error("Failed to open file '%s'!\n", (char *) filename);

    while (getline(&buffer, &size, file))
        if (matches(prefix, buffer))
            return 0;

    return 1;

}

int fops_update_line(const char* filename, const char* prefix, char* (*funct)(char *)) {

    FILE* file = fopen(filename, "r");
    if (file == NULL) return error("Failed to open file '%s'!\n", (char *) filename);

    char* tempPath = "temp";
    FILE* tempFile = fopen(tempPath, "w+");
    if (tempFile == NULL) {
        fclose(file);
        return error("Failed to create a temporary file!\n", NULL);
    }

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
            free(line);
        }
    }

    int result = 0;
    if (rename(tempPath, filename)) {
        result = error("Failed to overwrite original file '%s'!\n", (char *) filename);
        remove(tempPath);
    }

    fclose(tempFile);
    fclose(file);
    free(buffer);

    return result;

}

int fops_remove_line(const char* filename, const char* prefix) {
    return fops_update_line(filename, prefix, NULL);
}
