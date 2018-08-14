#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "hierarchy.h"

#define TRANSLATOR_LOAD_SUCCESS 0
#define TRANSLATOR_LOAD_FAILURE 1

typedef struct _treenode_ {
    char* path;
    char* segment;
    struct _treenode_* next;
    struct _treenode_* child;
} treenode;
treenode root = {0};

// Removes leading spaces and trailing '\n'
static char* trim_string(char* string) {
    size_t buff_len = strlen(string);
    if (string[buff_len - 1] == '\n') string[buff_len - 1] = '\0';

    uint8_t level;
    for (level = 0; string[level] == ' '; ++level);

    return string + level;
}

// Allocates a treenode with a given name
static treenode* create_treenode(char* path) {
    treenode* node = (treenode*) calloc(1, sizeof(treenode));
    node->segment = (char*) malloc(sizeof(char) * strlen(path));
    strcpy(node->segment, path);
    node->path = NULL;
    return node;
}

// Print the path tree
static void print_treenode(treenode* node, uint8_t lvl) {
    if (node == NULL) return;
    for (uint8_t i = 0; i < lvl; ++i) printf("\t");
    printf("%s\n", node->segment);
    print_treenode(node->child, (uint8_t)(lvl + 1));
    print_treenode(node->next, lvl);
}

// Loads the path tree from a file
uint8_t hierarchy_load(const char *source_file) {
    FILE* source = fopen(source_file, "r");
    if (!source) return TRANSLATOR_LOAD_FAILURE;

    int8_t lvl[25];
    char buffer[51];
    treenode* stack[25];
    uint8_t stack_ptr = 0;

    lvl[0] = -1;
    stack[0] = &root;

    while (fgets(buffer, 50, source)) {
        char* trimmed = trim_string(buffer);
        uint8_t level = (uint8_t)(trimmed - buffer);

        if ((buffer + level)[0] == ':') {

            stack[stack_ptr]->path = malloc(strlen(buffer) * sizeof(char));
            strcpy(stack[stack_ptr]->path, buffer + level + 1);

            printf("File: %s, Path: %s\n", stack[stack_ptr]->segment, stack[stack_ptr]->path);

        } else {
            treenode* node = create_treenode(buffer + level);

            if (level > lvl[stack_ptr]) {
                stack[stack_ptr++]->child = node;
            } else if (level == lvl[stack_ptr]) {
                stack[stack_ptr]->next = node;
                stack[stack_ptr] = node;
            } else {
                while (lvl[--stack_ptr] != level && stack_ptr > 0) {}
                stack[stack_ptr]->next = node;
            }

            stack[stack_ptr] = node;
            lvl[stack_ptr] = level;

            // printf("> %s %d\n", trimmed, lvl[stack_ptr]);
        }
    }

    //print_treenode(root.child, 0);

    fclose(source);
    return TRANSLATOR_LOAD_SUCCESS;
}

// Unloads the path tree
void hierarchy_unload(treenode *node) {
    if (node == NULL) return;
    hierarchy_unload(node->child);
    hierarchy_unload(node->next);
    free(node->segment);
    free(node->path);
    free(node);
}

// Find a path in the path tree
char* hierarchy_translate(const char* _path) {

    char path[256];
    strncpy(path, _path, 255);

    printf("Translating: %s\n", path);

    const char delimiter[2] = "/";

    treenode* aux = root.child;
    char* result = NULL;

    char* token = strtok(path, delimiter);
    //printf("%s\n", token);

    while (token) {
        while (aux != NULL && strcmp(aux->segment, token) != 0) {
            // printf("Token is: %s, Segment is: %s\n", token, aux->segment);
            aux = aux->next;
        }
        if (aux == NULL) return NULL;

        // printf("Found segment: %s\n", aux->segment);

        token = strtok(NULL, delimiter);
        result = aux->path;
        aux = aux->child;
    }

    return result;
}

// Load the file nodes in a path
FileNode* hierarchy_list(const char* _path) {

    char path[256];
    strncpy(path, _path, 255);

    const char delimiter[2] = "/";
    treenode* aux = root.child;

    char* token = strtok((char *)path, delimiter);

    while (token) {
        while (aux != NULL && strcmp(aux->segment, token) != 0) aux = aux->next;
        if (aux == NULL) return NULL;

        token = strtok(NULL, delimiter);
        aux = aux->child;
    }

    FileNode* first = NULL;
    FileNode* src = NULL;

    while (aux != NULL) {

        FileNode* node = (FileNode *) malloc(sizeof(FileNode));
        strncpy(node->name, aux->segment, 49);
        node->isDir = aux->path == NULL;
        node->next = NULL;

        if (src != NULL) src->next = node;
        else first = node;
        src = node;

        aux = aux->next;

    }

    return first;

}
