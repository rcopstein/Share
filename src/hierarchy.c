#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "hierarchy.h"

typedef struct _HierarchyNode {

    int child_count;
    LogicalFile* file;
    struct _HierarchyNode* child;
    struct _HierarchyNode* sibling;

} HierarchyNode;

static LogicalFile root_folder = { .isDir = true, .name = "/", .owner = "", .realpath = "/Users/rcopstein/Desktop/s1" };
static HierarchyNode root = { .file = &root_folder, .child = NULL, .sibling = NULL, .child_count = 0 };

static HierarchyNode* find_node(char* path) {

    char* _path = (char *) malloc(strlen(path) + 1); // Copy path so we don't mess with params
    strcpy(_path, path);
    char* aux = _path;

    char* sep = "/\n";
    HierarchyNode* node = &root;
    char* token = strsep(&aux, sep);

    while (token != NULL) {
        if (strlen(token) > 0) {

            node = node->child;

            while (node != NULL) {
                if (strcmp(node->file->name, token) == 0) break;
                node = node->sibling;
            }

            if (node == NULL) break;
        }

        token = strsep(&aux, sep);
    }

    free(_path);
    return node;
}

LogicalFile* create_logical_file(bool isDir, char* name, char* owner, char* realpath) {

    LogicalFile* result = (LogicalFile *) malloc(sizeof(LogicalFile));

    result->isDir = isDir;

    result->name = (char *) malloc(strlen(name) + 1);
    strcpy(result->name, name);

    result->owner = (char *) malloc(strlen(owner) + 1);
    strcpy(result->owner, owner);

    result->realpath = (char *) malloc(strlen(realpath) + 1);
    strcpy(result->realpath, realpath);

    return result;

}
LogicalFile* copy_logical_file(LogicalFile* file) {
    return create_logical_file(file->isDir, file->name, file->owner, file->realpath);
}
void free_logical_file(LogicalFile* file) {

    free(file->realpath);
    free(file->owner);
    free(file->name);
    free(file);

}

int add_logical_file(char* path, LogicalFile* file) {

    HierarchyNode* add_point = find_node(path);
    if (add_point == NULL || (add_point->file != NULL && !add_point->file->isDir)) return 1;

    HierarchyNode* to_add = (HierarchyNode *) malloc(sizeof(HierarchyNode));
    to_add->file = copy_logical_file(file);
    to_add->sibling = add_point->child;
    to_add->child_count = 0;
    to_add->child = NULL;

    add_point->child = to_add;
    add_point->child_count++;
    return 0;
}
LogicalFile* get_logical_file(char* path) {

    HierarchyNode* file = find_node(path);
    if (file == NULL) return NULL;
    return file->file;

}
int rem_logical_file(char* path) {

    char* _path = (char *) malloc(strlen(path) + 1); // Copy path to we don't mess with params
    strcpy(_path, path);

    char* lastsep = strrchr(_path, '/'); // Find the last occurrence of '/'
    *lastsep = '\0';

    char* filename = lastsep + 1; // Get a pointer to the file/folder name

    HierarchyNode* parent = find_node(_path);
    if (parent == NULL) return 1;

    HierarchyNode* previous = NULL;
    HierarchyNode* current = parent->child;

    while (current != NULL) {
        if (strcmp(current->file->name, filename) == 0) {
            if (previous != NULL) previous->sibling = current->sibling;
            else parent->child = current->sibling;
            parent->child_count--;
            break;
        }

        previous = current;
        current = current->sibling;
    }

    if (current == NULL) return 1;

    free_logical_file(current->file);
    free(current);
    return 0;
}

int serialize_file(char** buffer, LogicalFile* file) {

    uint16_t name_size = (uint16_t) strlen(file->name);
    uint16_t owner_size = (uint16_t) strlen(file->owner);

    if (*buffer == NULL) *buffer = (char *) malloc(sizeof(uint16_t) * 2 + name_size + owner_size + 2);
    char* aux = *buffer;

    memcpy(aux, &name_size, sizeof(uint16_t)); // Copy name size
    aux += sizeof(uint16_t);

    memcpy(aux, file->name, name_size); // Copy name
    aux += name_size;

    memcpy(aux, &owner_size, sizeof(uint16_t)); // Copy owner size
    aux += sizeof(uint16_t);

    memcpy(aux, file->owner, owner_size); // Copy owner
    aux += owner_size;

    memcpy(aux, &file->isDir, sizeof(uint8_t)); // Copy is directory
    aux += sizeof(uint8_t);

    return 0;
}
int deserialize_file(char* buffer, LogicalFile** file) {

    uint16_t size;

    memcpy(&size, buffer, sizeof(uint16_t)); // Read name size
    buffer += sizeof(uint16_t);

    char* name = (char *) malloc(size * sizeof(char)); // Allocate name
    memcpy(name, buffer, size); // Read name
    (*file)->name = name; // Assign name
    buffer += size;

    memcpy(&size, buffer, sizeof(uint16_t)); // Read owner size
    buffer += sizeof(uint16_t);

    char* owner = (char *) malloc(size * sizeof(char)); // Allocate owner
    memcpy(owner, buffer, size); // Read owner
    (*file)->owner = owner; // Assign owner
    buffer += size;

    memcpy(&(*file)->isDir, buffer, sizeof(uint8_t)); // Read is directory
    buffer += sizeof(uint8_t);

    return 0;
}

LogicalFile** list_logical_files(char* path) {

    HierarchyNode* folder = find_node(path);
    if (folder == NULL || !folder->file->isDir) return NULL;

    LogicalFile** list = (LogicalFile **) malloc((folder->child_count + 1) * sizeof(LogicalFile *));
    folder = folder->child;

    int count = 0;
    while (folder != NULL) { list[count++] = folder->file; folder = folder->sibling; }
    list[count] = NULL;

    return list;
}

/*
int main2() {

    LogicalFile* file  = create_logical_file(false, "Potato.png", "1", "/1/Potato.png");
    LogicalFile* file2 = create_logical_file(false, "Banana.png", "1", "/1/Banana.png");
    LogicalFile* fold  = create_logical_file(true,  "Images",     "1", "/");

    int result = add_logical_file("/", fold);
    printf("The result is %d\n", result);

    result = add_logical_file("/Images", file);
    printf("The result is %d\n", result);

    result = add_logical_file("/Images", file2);
    printf("The result is %d\n", result);

    LogicalFile* got = get_logical_file("/Images/Potato.png");
    printf("Got: %p\n", got);

    got = get_logical_file("Images/Banana.png");
    printf("Got: %p\n", got);

    //result = rem_logical_file("/Images/Potato.png");
    //printf("The result is %d\n", result);

    got = get_logical_file("Images/Potato.png");
    printf("Got: %p\n", got);

    got = get_logical_file("Images/Banana.png");
    printf("Got: %p\n", got);

    HierarchyNode* images = find_node("Images");
    printf("Images has %d children\n", images->child_count);

    return 0;

}
*/
