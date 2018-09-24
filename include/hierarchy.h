#pragma once

#include <stdbool.h>

// Structs
typedef struct _LogicalFile {

    bool isDir;
    char* name;
    char* owner;
    char* realpath;

    int num_links;

} LogicalFile;

// Auxiliar Functions
void split_path(char* path, char** name);

// Logical Files
LogicalFile* create_lf(bool isDir, char* name, char* owner, char* realpath);
LogicalFile** list_lf(char* path, int** conflicts);
void free_lf(LogicalFile* file);

// Conflict Checking
char* resolved_name(LogicalFile* file);

// Public Management Operations
int add_lf(LogicalFile *file, char *path);
int ren_lf(char* path, char* new_name);
LogicalFile* get_lf(char *path);
int rem_lf(char *path);
