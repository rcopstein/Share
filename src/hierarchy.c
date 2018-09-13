#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "hierarchy.h"

typedef struct _HierarchyNode {

    LogicalFile* file;
    struct _HierarchyNode* child;
    struct _HierarchyNode* sibling;

} HierarchyNode;

static HierarchyNode* root = NULL;
static HierarchyNode* find_hierarchy_node(char *path, HierarchyNode** parent, HierarchyNode** prev_sibling) {

    char* _path = (char *) malloc(strlen(path) + 1);
    strcpy(_path, path);

    HierarchyNode* aux = root;
    HierarchyNode* _parent = NULL;
    HierarchyNode* _prev_sibling = NULL;

    char* sep = "/\n";
    char* token = NULL;
    token = strsep(&_path, sep);

    while (strlen(token) != 0) {

        _prev_sibling = NULL;

        while (aux != NULL) {
            if (strcmp(aux->file->name, token) == 0) break;
            _prev_sibling = aux;
            aux = aux->sibling;
        }

        if (aux == NULL) break;

        _parent = aux;
        aux = aux->child;

        token = strsep(&_path, sep);
    }

    free(_path);
    return aux;
}

LogicalFile* create_logical_file(bool isDir, char* name, char* owner) {

    LogicalFile* result = (LogicalFile *) malloc(sizeof(LogicalFile));

    result->isDir = isDir;

    result->name = (char *) malloc(strlen(name) + 1);
    strcpy(result->name, name);

    result->owner = (char *) malloc(strlen(owner) + 1);
    strcpy(result->owner, owner);

    return result;

}
LogicalFile* copy_logical_file(LogicalFile* file) {
    return create_logical_file(file->isDir, file->name, file->owner);
}

int add_logical_file(char* path, LogicalFile* file) {

    HierarchyNode* add_point = find_hierarchy_node(path, NULL, NULL);
    if (add_point == NULL || !add_point->file->isDir) return 1;

    HierarchyNode* to_add = (HierarchyNode *) malloc(sizeof(HierarchyNode));
    to_add->file = copy_logical_file(file);
    to_add->sibling = NULL;
    to_add->child = NULL;

    HierarchyNode** aux = &add_point->sibling;
    while (*aux != NULL) aux = &(*aux)->sibling;
    *aux = to_add;

    return 0;

}

LogicalFile* get_logical_file(char* path) {

    HierarchyNode* file = find_hierarchy_node(path, NULL, NULL);
    if (file == NULL) return NULL;
    return file->file;

}

int rem_logical_file(char* path) {

    return 1;

}
