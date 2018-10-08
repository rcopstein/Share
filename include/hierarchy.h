#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "members.h"

// Structs
typedef struct _LogicalFile {

    bool isDir;
    char* name;
    char* owner;
    char* realpath;

    int num_links;

} LogicalFile;

// Auxiliar Functions
char* resolve_path(LogicalFile* file);
void split_path(char* path, char** name);

// Sequence Number
uint16_t get_lhier_seq_num();
uint16_t inc_lhier_seq_num();

// Logical Files
LogicalFile* create_lf(bool isDir, char* name, char* owner, char* realpath);
LogicalFile** list_lf(char* path, int** conflicts);
void free_lf(LogicalFile* file);

// Conflict Checking
char* resolved_name(LogicalFile* file);

// File Management
int _lf_add(LogicalFile* what, const char* where, bool create);
int _lf_get(const char* where, LogicalFile** result);
int _lf_rem(const char* where, bool remove_empty);
int _lf_ren(char* from, char* to);

// File Serialization
size_t _lf_size(LogicalFile *file);
size_t _lf_serialize(char **buffer, LogicalFile *file);
size_t _lf_deserialize(char *buffer, LogicalFile **file);

// File Synchronization
void _lf_sync_message(char* message, char* from);
char* _lf_build_message(size_t prefix_size, char *prefix, size_t *size);

void test();
