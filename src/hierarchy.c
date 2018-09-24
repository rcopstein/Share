#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "hierarchy.h"

typedef struct _HierarchyNode {

    int file_count;
    int folder_count;
    bool conflict_free;

    LogicalFile* file;
    struct _HierarchyNode* child;
    struct _HierarchyNode* sibling;

} HierarchyNode;

// NEW

// Variables
static LogicalFile root_f = { .isDir = true, .name = "/" };
static HierarchyNode root = { .file = &root_f, .child = NULL, .sibling = NULL, .folder_count = 0, .file_count = 0, .conflict_free = true };

// Auxiliar Functions
void split_path(char* path, char** name) {

    *name = strrchr(path, '/'); // Find the last occurrence of '/'
    **name = '\0';
    (*name)++;

}
void split_ext(char* name, char** extension) {

    *extension = strrchr(name, '.'); // Find the last occurrence of '.'
    **extension = '\0';
    (*extension)++;

}
void print_tree(int level, HierarchyNode* root) {

    if (root == NULL) return;
    for (int i = 0; i < level; ++i) printf("\t");
    printf("%s %s\n", root->file->name, root->file->owner);

    print_tree(level+1, root->child);
    print_tree(level, root->sibling);

}

// Logical Files
LogicalFile* create_lf(bool isDir, char* name, char* owner, char* realpath) {

    LogicalFile* result = (LogicalFile *) malloc(sizeof(LogicalFile));

    result->isDir = isDir;

    result->name = (char *) malloc(strlen(name) + 1);
    strcpy(result->name, name);

    result->owner = (char *) malloc(strlen(owner) + 1);
    strcpy(result->owner, owner);

    result->realpath = (char *) malloc(strlen(realpath) + 1);
    strcpy(result->realpath, realpath);

    result->num_links = 0;

    return result;

}
LogicalFile* copy_lf(LogicalFile* file) {

    return create_lf(file->isDir, file->name, file->owner, file->realpath);

}
void free_lf(LogicalFile* file) {

    free(file->realpath);
    free(file->owner);
    free(file->name);
    free(file);

}

// Hierarchy Nodes
static HierarchyNode* create_hn(LogicalFile* file) {

    HierarchyNode* result = (HierarchyNode *) malloc(sizeof(HierarchyNode));

    result->file = copy_lf(file);
    result->conflict_free = true;
    result->folder_count = 0;
    result->file_count = 0;
    result->sibling = NULL;
    result->child = NULL;

    return result;

}
static void free_hn(HierarchyNode* node) {

    free_lf(node->file);
    free(node);

}

// Tree Management
static void add_after(HierarchyNode** prev, HierarchyNode* node, HierarchyNode* next) {

    node->sibling = next;
    *prev = node;

}
static void rem_after(HierarchyNode** prev) {

    HierarchyNode* curr = *prev;
    *prev = curr->sibling;
    free_hn(curr);

}

static HierarchyNode* find_in_level(HierarchyNode* level, char* name, char* owner) {

    while (level != NULL) {
        if (strcmp(level->file->name, name) == 0) {
            if (owner == NULL || strcmp(level->file->owner, owner) == 0) {
                return level;
            }
        }
        level = level->sibling;
    }

    return NULL;

}
static void rem_beneath(HierarchyNode* parent, char* name, char* owner) {

    print_tree(0, root.child);
    printf("\n");

    HierarchyNode** aux = &parent->child;
    while (*aux != NULL) {
        if (strcmp((*aux)->file->name, name) == 0) {
            if (owner == NULL || strcmp((*aux)->file->owner, owner) == 0) {
                if ((*aux)->file->isDir) parent->folder_count--;
                else parent->file_count--;
                break;
            }
        }
        aux = &(*aux)->sibling;
    }

    if (*aux != NULL) rem_after(aux);
    print_tree(0, root.child);
}
static HierarchyNode* get_node(char *path) {

    char* _path = (char *) malloc(strlen(path) + 1);
    strcpy(_path, path);
    char* aux = _path;

    HierarchyNode* node = &root;
    char* sep = "/\n";
    char* token;

    while (true) {

        token = strsep(&aux, sep);

        if (token == NULL) break;
        if (strlen(token) == 0) continue;

        node = node->child;
        node = find_in_level(node, token, NULL);
        if (node == NULL) break;

    }

    free(_path);
    return node;

}

LogicalFile** list_lf(char* path, int** conflicts) {

    HierarchyNode* folder = get_node(path);
    if (folder == NULL || !folder->file->isDir) return NULL;

    int count = folder->file_count;
    count += folder->folder_count;
    count += 1;

    LogicalFile** result = (LogicalFile **) malloc(count * sizeof(LogicalFile *));
    *conflicts = (int *) malloc(count * sizeof(int));

    folder = folder->child;

    int aux = 0;
    while (folder != NULL) {

        (*conflicts)[aux] = folder->conflict_free ? 0 : 1;
        result[aux] = folder->file;
        aux++;

        folder = folder->sibling;
    }

    (*conflicts)[aux] = -1;
    result[aux] = NULL;
    return result;
}

// Conflict Checking
char* resolved_name(LogicalFile* file) {

    char* name = (char *) malloc(sizeof(char) * (strlen(file->name) + 1));
    strcpy(name, file->name);

    char* extension;
    split_ext(name, &extension);

    char* resolved_name = (char *) malloc(sizeof(char) * (strlen(name) + strlen(file->owner) + strlen(extension) + 3));
    sprintf(resolved_name, "%s@%s.%s", name, file->owner, extension);
    return resolved_name;

}
void dissolve_name(char* name, char** owner) {

    char* at = strrchr(name, '@');
    char* dot = strrchr(name, '.');

    if (at == NULL || dot == NULL || dot <= at) {
        *owner = NULL;
        return;
    }

    *dot = '\0';
    char* own = at+1;
    *owner = (char *) malloc(sizeof(char) * strlen(own));
    strcpy(*owner, own);
    *dot = '.';

    char* ext = (char *) malloc(sizeof(char) * (strlen(dot) + 1));
    strcpy(ext, dot);
    strcpy(at, ext);
    free(ext);

    printf("Dissolved name is %s\n", name);
    printf("Dissolved owner is %s\n", *owner);
}

void dissolve_conflict(HierarchyNode* level, char* name) {

    HierarchyNode* conflict1 = find_in_level(level, name, NULL);
    if (conflict1 == NULL) {
        printf("Dissolve: no files named %s! Abort!\n", name);
        return; // No conflicts, do nothing
    }

    printf("Dissolve: one file named %s!\n", name);

    HierarchyNode* conflict2 = find_in_level(conflict1->sibling, name, NULL);
    if (conflict2 != NULL) {
        printf("Dissolve: two files named %s! Abort!\n", name);
        return; // More than one conflict, do nothing
    }

    printf("Dissolve: only one file named %s! No more conflict!\n", name);
    conflict1->conflict_free = true; // Only conflict means no conflict!

}
int check_conflict(HierarchyNode* level, HierarchyNode* file) {

    HierarchyNode* conflict = find_in_level(level, file->file->name, NULL);
    if (conflict == NULL) return 0;

    printf("There is a conflict for %s!\n", conflict->file->name);
    conflict->conflict_free = false;
    file->conflict_free = false;

    return 1;

}

// Public Management Operations
int add_lf(LogicalFile *file, char *path) {

    char* hasAt = strchr(file->name, '@'); // Don't allow names with '@'
    if (hasAt != NULL) return -EINVAL;

    HierarchyNode* parent = get_node(path);
    if (parent == NULL || !parent->file->isDir) return -ENOENT;

    if (file->isDir) parent->folder_count++;
    else parent->file_count++;

    HierarchyNode* node = create_hn(file);
    HierarchyNode* level = parent->child;
    check_conflict(level, node);

    add_after(&parent->child, node, parent->child);

    printf("Added file %s, Conflict free: %d\n", node->file->name, node->conflict_free);

    return 0;

}
int ren_lf(char *path, char* new_name) {

    char* _path = (char *) malloc(strlen(path) + 1);
    strcpy(_path, path);
    int res = 0;

    char* rest;
    split_path(_path, &rest);

    char* name = strsep(&rest, "@");
    char* owner = rest;

    HierarchyNode* parent = get_node(_path);
    if (parent == NULL || !parent->file->isDir) res = -ENOENT;
    else {

        HierarchyNode* level = parent->child;
        HierarchyNode* to_rn = find_in_level(level, name, owner);

        if (to_rn == NULL) res = -ENOENT;
        else {

            char* nname = (char *) malloc(sizeof(char) * (strlen(new_name) + 1));
            strcpy(nname, new_name);

            char* oname = to_rn->file->name;
            to_rn->file->name = nname;

            dissolve_conflict(level, oname);
            free(oname);
        }
    }

    free(_path);
    return res;

}
LogicalFile* get_lf(char *path) {

    printf("For path: %s\n", path);

    char* _path = (char *) malloc(strlen(path) + 1);
    strcpy(_path, path);

    char *name, *owner;
    split_path(_path, &name);
    dissolve_name(name, &owner);

    HierarchyNode* parent = &root;
    if (_path != NULL && strlen(_path) > 0) parent = get_node(_path);

    HierarchyNode* node = parent;
    if (parent != NULL && name != NULL && strlen(name) > 0) node = find_in_level(parent->child, name, owner);

    if (strcmp(name, "potato.xlsx") == 0) print_tree(0, parent);

    if (owner != NULL) free(owner);
    free(_path);

    LogicalFile* res = node != NULL ? node->file : NULL;
    printf("Found file: %p\n", (void *) res);
    return res;

}
int rem_lf(char *path) {

    printf("I'm here rem!\n");

    char* _path = (char *) malloc(strlen(path) + 1);
    strcpy(_path, path);
    int res = 0;

    char *name, *owner;
    split_path(_path, &name);
    dissolve_name(name, &owner);

    printf("Attempting to remove file %s with owner %s at path %s\n", name, owner, _path);

    HierarchyNode* parent = get_node(_path);
    if (parent != NULL && parent->file->isDir) {
        rem_beneath(parent, name, owner);
        dissolve_conflict(parent->child, name);
    } else res = -ENOENT;

    printf("Result of deletion was %d\n", res);
    if (owner != NULL) free(owner);
    free(_path);
    return res;

}

// Serialization
size_t size_of_lf(LogicalFile* file) {

    uint16_t name_size = (uint16_t) strlen(file->name);
    uint16_t owner_size = (uint16_t) strlen(file->owner);

    size_t size = 1;
    size += name_size;
    size += owner_size;
    size += sizeof(uint8_t);
    size += sizeof(uint16_t) * 2;

    return size;

}
int serialize_file(char** buffer, LogicalFile* file) {

    if (*buffer == NULL) {
        size_t size = size_of_lf(file);
        *buffer = (char *) malloc(size);
    }

    char* aux = *buffer;
    uint16_t name_size = (uint16_t) strlen(file->name);
    uint16_t owner_size = (uint16_t) strlen(file->owner);

    memcpy(aux, &name_size, sizeof(uint16_t)); // Copy name size
    aux += sizeof(uint16_t);

    memcpy(aux, file->name, sizeof(char) * name_size); // Copy name
    aux += sizeof(char) * name_size;

    memcpy(aux, &owner_size, sizeof(uint16_t)); // Copy owner size
    aux += sizeof(uint16_t);

    memcpy(aux, file->owner, sizeof(char) * owner_size); // Copy owner
    aux += sizeof(char) * owner_size;

    memcpy(aux, &file->isDir, sizeof(uint8_t)); // Copy is directory
    // aux += sizeof(uint8_t);

    return 0;

}
int deserialize_file(char* buffer, LogicalFile** file) {

    if (*file == NULL) {
        *file = (LogicalFile *) malloc(sizeof(LogicalFile));
    }

    uint16_t size;

    memcpy(&size, buffer, sizeof(uint16_t)); // Read name size
    buffer += sizeof(uint16_t);

    char* name = (char *) malloc(sizeof(char) * size); // Allocate name
    memcpy(name, buffer, size); // Read name
    (*file)->name = name; // Assign name
    buffer += size;

    memcpy(&size, buffer, sizeof(uint16_t)); // Read owner size
    buffer += sizeof(uint16_t);

    char* owner = (char *) malloc(sizeof(char) * size); // Allocate owner
    memcpy(owner, buffer, size); // Read owner
    (*file)->owner = owner; // Assign owner
    buffer += size;

    memcpy(&(*file)->isDir, buffer, sizeof(uint8_t)); // Read is directory
    // buffer += sizeof(uint8_t);

    return 0;

}
