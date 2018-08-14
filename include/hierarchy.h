#pragma once

#include <stdbool.h>

// File Struct
typedef struct _FileNode {

    bool isDir;
    char name[50];
    struct _FileNode* next;

} FileNode;

// Load the file nodes in a path
FileNode* hierarchy_list(const char* path);

// Loads the path tree from a file
uint8_t hierarchy_load(const char *source_file);

// Find a path in the path tree
char* hierarchy_translate(const char *path);
