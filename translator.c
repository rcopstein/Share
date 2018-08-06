#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

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
    node->segment = (char*) calloc(1, sizeof(char) * strlen(path));
    strcpy(node->segment, path);
    return node;
}

// Loads the path tree from a file
uint8_t translator_load(const char* source_file) {
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
            stack[stack_ptr]->path = calloc(1, strlen(buffer));
            strcpy(stack[stack_ptr]->path, buffer + level + 1);
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

    fclose(source);
    return TRANSLATOR_LOAD_SUCCESS;
}

// Unloads the path tree
void translator_unload(treenode* node) {
    if (node == NULL) return;
    translator_unload(node->child);
    translator_unload(node->next);
    free(node);
}

// Find a path in the path tree
char* translator_find(char* path) {
    const char delimiter[2] = "/";

    treenode* aux = root.child;
    char* result = NULL;

    char* token = strtok(path, delimiter);
    printf("%s\n", token);

    while (token) {
        while (strcmp(aux->segment, token) != 0 && aux != NULL) {
            printf("Token is: %s, Segment is: %s\n", token, aux->segment);
            aux = aux->next;
        }
        if (aux == NULL) return NULL;

        printf("Found segment: %s\n", aux->segment);
        result = aux->path;
        aux = aux->child;

        token = strtok(NULL, delimiter);
    }

    return result;
}

// Print the path tree
void print_treenode(treenode* node, uint8_t lvl) {
    if (node == NULL) return;
    for (uint8_t i = 0; i < lvl; ++i) printf("\t");
    printf("%s\n", node->segment);
    print_treenode(node->child, (uint8_t)(lvl + 1));
    print_treenode(node->next, lvl);
}

int main() {
    uint8_t result = translator_load("../../sample.txt");
    if (result == TRANSLATOR_LOAD_FAILURE) return 1;

    print_treenode(root.child, 0);

    char  str[50] = "My Folder 1/My File 1.1";
    char* res = translator_find(str);
    printf("%s\n", res);

    return 0;
}
