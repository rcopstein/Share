#pragma once

#include <stdbool.h>

typedef struct _LogicalFile {

    bool isDir;
    char* name;
    char* owner;

} LogicalFile;

LogicalFile* create_logical_file(bool isDir, char* name, char* owner);

int add_logical_file(char* path, LogicalFile* file);
LogicalFile* get_logical_file(char* path);
int rem_logical_file(char* path);

LogicalFile** list_logical_files(char* path);
char* translate_logical_file(char* path);
