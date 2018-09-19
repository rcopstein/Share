#pragma once

#include <stdbool.h>

typedef struct _LogicalFile {

    bool isDir;
    char* name;
    char* owner;
    char* realpath;

    int num_links;

} LogicalFile;

LogicalFile* create_logical_file(bool isDir, char* name, char* owner, char* realpath);
void free_logical_file(LogicalFile* file);

int add_logical_file(char* path, LogicalFile* file);
LogicalFile* get_logical_file(char* path);
int rem_logical_file(char* path);

LogicalFile** list_logical_files(char* path);
