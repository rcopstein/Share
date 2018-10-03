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

// Public Management Operations
int add_lf(LogicalFile *file, char *path);
int ren_lf(char* path, char* new_name);
LogicalFile* get_lf(char *path);
int rem_lf(char *path);

// Serialization
size_t size_of_lf(LogicalFile* file);
int deserialize_file(char* buffer, LogicalFile** file);
size_t serialize_file(char** buffer, LogicalFile* file);

void read_hierarchy_message(uint16_t sn, member* m, char* message);
char* build_hierarchy_message(size_t prefix_size, char* prefix, size_t* size);
