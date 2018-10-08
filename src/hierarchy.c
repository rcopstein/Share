#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <members.h>
#include <ctype.h>

#include "output.h"
#include "hierarchy.h"
#include "semaphores.h"

typedef struct _HierarchyNode {

    uint16_t seq_num;

    int file_count;
    int folder_count;
    bool conflict_free;

    LogicalFile* file;
    struct _HierarchyNode* parent;
    struct _HierarchyNode* child;
    struct _HierarchyNode* next;
    struct _HierarchyNode* prev;

} HierarchyNode;

// Sequence Number
static uint16_t seq_num = 0;
static semaphore* seq_num_sem;

uint16_t get_lhier_seq_num() {

    return seq_num;

}
uint16_t inc_lhier_seq_num() {

    if (seq_num_sem == NULL) {
        seq_num_sem = (semaphore *) malloc(sizeof(semaphore));
        portable_sem_init(seq_num_sem, 1);
    }

    portable_sem_wait(seq_num_sem);
    ++seq_num;
    portable_sem_post(seq_num_sem);

    return seq_num;

}


// Variables
static LogicalFile root_f = { .isDir = true, .name = "/" };
static HierarchyNode root = { .file = &root_f, .parent = NULL, .child = NULL, .next = NULL, .prev = NULL, .folder_count = 0, .file_count = 0, .conflict_free = true };


// Auxiliary Functions
char* get_segment(char** path) {

    char* segment;
    do { segment = strsep(path, "/"); } while (segment != NULL && !strlen(segment));
    return segment;

}
char* resolve_path(LogicalFile* file) {

    if (file == NULL) return NULL;
    if (file->isDir) return NULL;

    member* current = get_current_member();

    size_t size = strlen(file->realpath);
    size += current->prefix_size;
    size += strlen(file->owner);
    size += sizeof(char) * 3;

    char* result = (char *) malloc(size);
    sprintf(result, "%s/%s/%s", current->prefix, file->owner, file->realpath);

    return result;
}
int empty_folder(HierarchyNode* file) {
    return file->file_count + file->folder_count == 0;
}
void split_path(char* path, char** name) {

    *name = strrchr(path, '/'); // Find the last occurrence of '/'
    **name = '\0';
    (*name)++;

}
void split_ext(char* name, char** extension) {

    *extension = strrchr(name, '.'); // Find the last occurrence of '.'
    if (*extension == NULL) return;

    **extension = '\0';
    (*extension)++;

}
void print_tree(int level, HierarchyNode* root) {

    if (root == NULL) return;
    for (int i = 0; i < level; ++i) printf("\t");

    if (root->conflict_free) printf("%s %s\n", root->file->name, root->file->owner);
    else {
        char* name = resolved_name(root->file);
        printf("%s %s\n", name, root->file->owner);
        free(name);
    }

    print_tree(level+1, root->child);
    print_tree(level, root->next);

}


// Name Conflict Resolution
char* resolved_name(LogicalFile* file) {

    size_t size = 0;
    char *name, *ext, *resolved_name;

    name = (char *) malloc(sizeof(char) * (strlen(file->name) + 1));
    strcpy(name, file->name);
    split_ext(name, &ext);

    size += strlen(file->owner);
    size += strlen(name);
    size += 2;

    if (ext == NULL) {
        resolved_name = (char *) malloc(sizeof(char) * size);
        sprintf(resolved_name, "%s@%s", name, file->owner);
    }
    else {
        size += strlen(ext) + 1;
        resolved_name = (char *) malloc(sizeof(char) * size);
        sprintf(resolved_name, "%s@%s.%s", name, file->owner, ext);
    }

    free(name);
    return resolved_name;

}
void dissolved_name(char *name, char **owner) {

    *owner = NULL;
    size_t size = strlen(name);

    char* end = name + size;
    char* at = strrchr(name, '@');
    char* dot = strrchr(name, '.');

    if (at == NULL) return;
    if (dot > at) end = dot;

    size_t owner_size = end - at - 1;
    *owner = (char *) malloc(owner_size + 1);
    memcpy(*owner, at + 1, owner_size);
    (*owner)[owner_size] = '\0';

    if (dot == NULL) *at = '\0';
    else while (*dot != '\0') { *at = *dot; at++; dot++; }
    *at = '\0';

}


// Logical Files
LogicalFile* create_lf(bool isDir, char* name, char* owner, char* realpath) {

    LogicalFile* result = (LogicalFile *) malloc(sizeof(LogicalFile));

    result->isDir = isDir;

    result->name = (char *) malloc(strlen(name) + 1);
    strcpy(result->name, name);

    if (!result->isDir) {

        result->owner = (char *) malloc(strlen(owner) + 1);
        strcpy(result->owner, owner);

        result->realpath = (char *) malloc(strlen(realpath) + 1);
        strcpy(result->realpath, realpath);

    } else {
        result->owner = NULL;
        result->realpath = NULL;
    }

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
    result->parent = NULL;
    result->child = NULL;
    result->prev = NULL;
    result->next = NULL;

    result->seq_num = get_lhier_seq_num();

    return result;

}
static void free_hn(HierarchyNode* node) {

    free_lf(node->file);
    free(node);

}


// Tree Management
static void _hn_add(HierarchyNode* parent, HierarchyNode** where, HierarchyNode* what) {

    if (what->file->isDir) parent->folder_count++;
    else parent->file_count++;

    if (*where != NULL) what->next = *where;
    *where = what;

}
static void _hn_rem(HierarchyNode* parent, HierarchyNode** what) {

    if ((*what)->file->isDir) parent->folder_count--;
    else parent->file_count--;
    *what = (*what)->next;

}

static HierarchyNode** _hn_get(HierarchyNode** from, char* name, char* owner) {

    HierarchyNode** aux = from;

    while (*aux != NULL) {
        if (name == NULL || !strcmp((*aux)->file->name, name)) {
            if (owner == NULL || !strcmp((*aux)->file->owner, owner)) {
                break;
            }
        }
        aux = &(*aux)->next;
    }

    return aux;

}
static HierarchyNode** _hn_last(HierarchyNode* parent) {

    HierarchyNode** aux = &parent->child;
    while (*aux != NULL) aux = &(*aux)->next;
    return aux;

}

static int _hn_get_path_rec(HierarchyNode *parent, HierarchyNode **result, char *where, char *what, char *owner) {

    char* segment = get_segment(&where);

    if (segment == NULL) { // It's here
        HierarchyNode** who = _hn_get(&parent->child, what, owner);
        if (*who == NULL) return ENOENT;
        *result = *who;
        return 0;
    }
    else {
        HierarchyNode** next = _hn_get(&parent->child, segment, NULL);
        if (*next == NULL || !(*next)->file->isDir) return ENOENT;
        return _hn_get_path_rec(*next, result, where, what, owner);
    }

}
static int _hn_get_path(HierarchyNode **result, const char *where) {

    char* _where = (char *) malloc(sizeof(char) * (strlen(where) + 1));
    strcpy(_where, where);

    char* name;
    split_path(_where, &name);

    char* owner;
    dissolved_name(name, &owner);
    int res = _hn_get_path_rec(&root, result, _where, name, owner);

    free(_where);
    free(owner);
    return res;
}


// Conflict Checking
static int _lf_conf_add(HierarchyNode* parent, char* name) {

    int result = 0;
    HierarchyNode* aux = parent->child;

    while (aux != NULL) {
        if (!strcmp(aux->file->name, name)) { aux->conflict_free = false; result = 1; }
        aux = aux->next;
    }

    return result;

}
static int _lf_conf_rem(HierarchyNode* parent, char* name) {

    HierarchyNode* aux = parent->child;
    HierarchyNode* fst = NULL;
    int count = 0;

    while (aux != NULL) {
        if (!strcmp(aux->file->name, name)) {
            fst = aux;
            count++;
        }
        aux = aux->next;
    }

    if (count == 1) { fst->conflict_free = true; return 0; }
    return 1;

}


// File Management
static int _lf_add_rec(HierarchyNode* parent, LogicalFile* what, char* where, bool create) {

    char* segment = get_segment(&where);

    if (segment == NULL) { // It's here
        HierarchyNode* to_add = create_hn(what);
        HierarchyNode** last = _hn_last(parent);

        if (_lf_conf_add(parent, what->name)) to_add->conflict_free = false;
        _hn_add(parent, last, to_add);
        return 0;
    }
    else {
        HierarchyNode** next = _hn_get(&parent->child, segment, NULL);

        if (*next == NULL) {
            if (!create) return ENOENT;

            LogicalFile *file = create_lf(true, segment, NULL, NULL);
            _lf_add_rec(parent, file, NULL, false);
            next = _hn_get(&parent->child, segment, NULL); // I might not need this since we are adding it to the last element
        }
        else if (!(*next)->file->isDir) return ENOENT;

        return _lf_add_rec(*next, what, where, create);
    }

}
int _lf_add(LogicalFile* what, const char* where, bool create) {

    char* _where = (char *) malloc(sizeof(char) * (strlen(where) + 1));
    strcpy(_where, where);

    LogicalFile* _what = copy_lf(what);
    int res = _lf_add_rec(&root, what, _where, create);

    if (res) free(_what);
    free(_where);

    return res;
}

static int _lf_rem_rec(HierarchyNode* parent, char* where, char* what, char* owner, bool remove_empty) {

    char* segment = get_segment(&where);

    if (segment == NULL) { // It's here
        HierarchyNode** who = _hn_get(&parent->child, what, owner);
        if (*who == NULL) return ENOENT;
        HierarchyNode* to_free = *who;
        _hn_rem(parent, who);
        free_hn(to_free);
        _lf_conf_rem(parent, what);
        return 0;
    }
    else {
        HierarchyNode** next = _hn_get(&parent->child, segment, NULL);
        if (*next == NULL || !(*next)->file->isDir) return ENOENT;
        int res = _lf_rem_rec(*next, where, what, owner, remove_empty);
        if (remove_empty && empty_folder(*next)) {
            HierarchyNode* to_free = *next;
            _hn_rem(parent, next);
            free_hn(to_free);
        }
        return res;
    }

}
int _lf_rem(const char* where, bool remove_empty) {

    char* _where = (char *) malloc(sizeof(char) * (strlen(where) + 1));
    strcpy(_where, where);

    char* name;
    split_path(_where, &name);

    char* owner;
    dissolved_name(name, &owner);
    int res = _lf_rem_rec(&root, _where, name, owner, remove_empty);

    free(_where);
    free(owner);
    return res;
}

static int _lf_ren_rec(HierarchyNode* parent, char* where, char* what, char* rename) {

    char* segment = get_segment(&where);

    if (segment == NULL) { // It's here

        HierarchyNode** target = _hn_get(&parent->child, what, NULL);
        if (*target == NULL) return ENOENT;

        _lf_conf_add(parent, rename); // Check for new conflict with rename

        char* nname = (char *) malloc(strlen(rename) + 1); // Allocate new name
        strcpy(nname, rename);

        free((*target)->file->name); // Apply the rename
        (*target)->file->name = nname;

        _lf_conf_rem(parent, what); // Remove old conflict, if any
        return 0;

    }
    else {

        HierarchyNode** next = _hn_get(&parent->child, segment, NULL);
        if (*next == NULL || !(*next)->file->isDir) return ENOENT;
        return _lf_ren_rec(*next, where, what, rename);

    }

}
int _lf_ren(char* from, char* to) {

    char *what, *rename;
    split_path(from, &what);
    split_path(to, &rename);

    if (strcmp(from, to) != 0) return EINVAL;
    return _lf_ren_rec(&root, from, what, rename);

}

int _lf_get(const char* where, LogicalFile** result) {

    HierarchyNode* parent;
    int res = _hn_get_path(&parent, where);
    if (!res) *result = parent->file;
    return res;

}


// Serialization
size_t _lf_size(LogicalFile *file) {

    size_t size = 0;
    size += sizeof(uint8_t);
    size += sizeof(uint16_t);
    size += strlen(file->name);

    if (!file->isDir) {
        size += strlen(file->realpath);
        size += strlen(file->owner);
        size += sizeof(uint16_t) * 2;
    }

    return size;

}
size_t _lf_serialize(char **buffer, LogicalFile *file) {

    char* aux = *buffer;
    size_t size = _lf_size(file);
    uint16_t name_size = (uint16_t) strlen(file->name);

    if (*buffer == NULL) *buffer = (char *) malloc(size);

    memcpy(aux, &name_size, sizeof(uint16_t)); // Copy name size
    aux += sizeof(uint16_t);

    memcpy(aux, file->name, sizeof(char) * name_size); // Copy name
    aux += sizeof(char) * name_size;

    // printf("Serializing name: %s\n", file->name);

    memcpy(aux, &file->isDir, sizeof(uint8_t)); // Copy is directory
    aux += sizeof(uint8_t);

    if (!file->isDir) {

        uint16_t owner_size = (uint16_t) strlen(file->owner);
        uint16_t realpath_size = (uint16_t) strlen(file->realpath);

        memcpy(aux, &owner_size, sizeof(uint16_t)); // Copy owner size
        aux += sizeof(uint16_t);

        memcpy(aux, file->owner, sizeof(char) * owner_size); // Copy owner
        aux += sizeof(char) * owner_size;

        // printf("Serializing owner: %s\n", file->owner);

        memcpy(aux, &realpath_size, sizeof(uint16_t)); // Copy realpath size
        aux += sizeof(uint16_t);

        memcpy(aux, file->realpath, sizeof(char) * realpath_size); // Copy realpath
        //aux += sizeof(char) * realpath_size;

        // printf("Serializing realpath: %s\n", file->realpath);

    }

    return size;

}
size_t _lf_deserialize(char *buffer, LogicalFile **file) {

    if (*file == NULL) {
        *file = (LogicalFile *) malloc(sizeof(LogicalFile));
    }

    uint16_t size;
    size_t total_size = 0;

    memcpy(&size, buffer, sizeof(uint16_t)); // Read name size
    total_size += sizeof(uint16_t);
    buffer += sizeof(uint16_t);

    char* name = (char *) malloc(sizeof(char) * (size + 1)); // Allocate name
    memcpy(name, buffer, sizeof(char) * size); // Read name
    (*file)->name = name; // Assign name
    total_size += size;
    name[size] = '\0';
    buffer += size;

    // printf("Deserialized name: %s\n", name);

    memcpy(&(*file)->isDir, buffer, sizeof(uint8_t)); // Read is directory
    total_size += sizeof(uint8_t);
    buffer += sizeof(uint8_t);

    if (!(*file)->isDir) {

        memcpy(&size, buffer, sizeof(uint16_t)); // Read owner size
        total_size += sizeof(uint16_t);
        buffer += sizeof(uint16_t);

        char* owner = (char *) malloc(sizeof(char) * (size + 1)); // Allocate owner
        memcpy(owner, buffer, size); // Read owner
        (*file)->owner = owner; // Assign owner
        total_size += size;
        owner[size] = '\0';
        buffer += size;

        // printf("Deserialized owner: %s\n", owner);

        memcpy(&size, buffer, sizeof(uint16_t)); // Read realpath size
        total_size += sizeof(uint16_t);
        buffer += sizeof(uint16_t);

        char *realpath = (char *) malloc(sizeof(char) * (size + 1)); // Allocate realpath
        memcpy(realpath, buffer, size); // Read realpath
        (*file)->realpath = realpath; // Assign realpath
        realpath[size] = '\0';
        total_size += size;
        //buffer += size;

        // printf("Deserialized realpath: %s\n", realpath);

    } else {
        (*file)->owner = NULL;
        (*file)->realpath = NULL;
    }

    return total_size;

}


// File Synchronization
static void cleanup(HierarchyNode* parent, HierarchyNode** current, char* owner, uint16_t seq_num) {

    if (*current == NULL) return;

    cleanup(*current, &(*current)->child, owner, seq_num);
    cleanup(parent, &(*current)->next, owner, seq_num);

    if ((*current)->file->isDir && empty_folder(*current)) {
        HierarchyNode* to_free = *current;
        _hn_rem(parent, current);
        free_hn(to_free);
    }
    else if (!(*current)->file->isDir && !strcmp((*current)->file->owner, owner) && (*current)->seq_num < seq_num) {
        HierarchyNode* to_free = *current;
        _hn_rem(parent, current);
        free_hn(to_free);
    }

}

static int _lf_sync_files(HierarchyNode *parent, HierarchyNode **current, LogicalFile *file, uint16_t seq_num) {

    // The file is new to this node
    if (*current == NULL) {

        printf("Added new file %s\n", file->name);

        HierarchyNode* new = create_hn(file);

        if (_lf_conf_add(parent, file->name)) new->conflict_free = false;
        _hn_add(parent, current, new);
        return 1;
    }

    // The file already exists, must update it
    else if (file != NULL && !strcmp((*current)->file->name, file->name)) {

        printf("Updated file %s\n", file->name);

        (*current)->seq_num = seq_num;
        return 0;
    }

    // The current file doesn't exist anymore
    else {

        printf("Removed file %s\n", (*current)->file->name);

        HierarchyNode* to_free = *current;
        _hn_rem(parent, current);
        _lf_conf_rem(parent, (*current)->file->name);
        free_hn(to_free);
        return -1;
    }

}
static int _lf_sync_level(HierarchyNode* parent, char** message, int level, uint16_t seq_num) {

    if (level == 0) return 0;

    LogicalFile* file = NULL;
    int res, cur_level = level;

    HierarchyNode** current = &parent->child;
    HierarchyNode** next = current;

    do {

        free(file);
        file = NULL;
        size_t msg_size = _lf_deserialize(*message, &file);
        *message += msg_size;

        if (file->isDir) {

            HierarchyNode** folder = _hn_get(&parent->child, file->name, NULL);
            if (*folder == NULL) _hn_add(parent, folder, create_hn(file));
            next = folder;

        }
        else {
            do {

                if (*current != NULL) current = _hn_get(current, NULL, (*current)->file->owner);
                res = _lf_sync_files(parent, current, file, seq_num);
                if (res != -1) current = &(*current)->next;

            }
            while (res == -1);
        }

        memcpy(&cur_level, *message, sizeof(uint16_t));
        *message += sizeof(uint16_t);

        if (cur_level > level) cur_level = _lf_sync_level(*next, message, cur_level, seq_num);

    }
    while (cur_level == level);

    return cur_level;
}
void _lf_sync_message(char* message, char* from) {

    uint16_t level;
    memcpy(&level, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    _lf_sync_level(&root, &message, level, seq_num);
    cleanup(&root, &root.child, from, seq_num);

}

static size_t copy_member_buffer(uint16_t level, char** buffer, size_t offset, size_t* message_size, HierarchyNode* node) {

    if (node == NULL) return offset;

    // Calculate Size and Offset
    size_t node_size = _lf_size(node->file) + sizeof(uint16_t);
    size_t new_offset = offset + node_size;
    size_t res_offset = offset;

    // Determine if it should write
    bool should_write = false;
    if (!node->file->isDir) should_write = true;
    else {
        size_t result = copy_member_buffer((uint16_t)(level + 1), buffer, new_offset, message_size, node->child);
        if (result > new_offset) { res_offset = new_offset = result; should_write = true; }
        if (result == 0) return 0;
    }

    if (should_write) {

        printf("Writing %s\n", node->file->name);

        // In case this is the last file we write we will make sure there is at least enough space for another
        // uint16_t to hold the terminator. In practice, different entries will not leave a blank space between
        // them

        int i;
        for (i = 0; *message_size < offset + node_size + sizeof(uint16_t); ++i) *message_size = *message_size * 2;
        if (i) {
            *buffer = realloc(*buffer, *message_size);
            if (*buffer == NULL) { errno = ENOMEM; return 0; }
        }

        char* where_to = *buffer + offset;
        memcpy(where_to, &level, sizeof(uint16_t));

        where_to += sizeof(uint16_t);
        _lf_serialize(&where_to, node->file);

        size_t result = copy_member_buffer(level, buffer, new_offset, message_size, node->next);
        if (result > res_offset) res_offset = result;
        if (result == 0) return 0;

    } else {
        size_t result = copy_member_buffer(level, buffer, offset, message_size, node->next);
        if (result > offset) res_offset = result;
        if (result == 0) return 0;
    }

    return res_offset;
}
char* _lf_build_message(size_t prefix_size, char *prefix, size_t *size) {

    // Allocate initial buffer
    size_t offset = 0;
    uint16_t terminator = 0;
    size_t message_size = prefix_size + 128;
    char* message = (char *) malloc(sizeof(char) * message_size);

    // Copy Prefix
    memcpy(message + offset, prefix, prefix_size);
    offset += prefix_size;

    // Copy Sequence Number
    memcpy(message + offset, &seq_num, sizeof(seq_num));
    offset += sizeof(seq_num);

    // Copy Files/Folders into buffer
    *size = copy_member_buffer(1, &message, offset, &message_size, root.child);
    if (*size == 0) { free(message); message = NULL; }
    else {
        memcpy(message + *size, &terminator, sizeof(uint16_t));
        *size += sizeof(uint16_t);

        // Print Message
        printf("\n");
        for (int i = 0; i < *size; ++i) {
            if (!isprint(message[i])) printf("\\%d", message[i]);
            else printf("%c", message[i]);
        }
        printf("\n\n");
    }

    // Return Result
    return message;
}


// Testing
void test() {

    member* self = build_member("1", "127.0.0.1", 4000, "/Users/rcopstein/Desktop/s1");
    initialize_metadata_members(self);

    LogicalFile* file1 = create_lf(false, "a.txt", "2", "a.txt");
    LogicalFile* file2 = create_lf(false, "a.txt", "2", "b.txt");
    LogicalFile* file3 = create_lf(false, "c.txt", "2", "c.txt");
    LogicalFile* file4 = create_lf(false, "d.txt", "2", "d.txt");

    _lf_add(file1, "/A/B/", true);
    _lf_add(file2, "/B/C/", true);

    size_t size;
    char* message1 = _lf_build_message(0, NULL, &size);
    inc_lhier_seq_num();
    char* message2 = _lf_build_message(0, NULL, &size);
    inc_lhier_seq_num();

    _lf_rem("/A/B/a.txt", true);
    _lf_rem("/B/C/a.txt", true);

    _lf_sync_message(message1, "2");
    print_tree(0, &root);
    printf("\n");

    _lf_add(file3, "/A/B/", true);
    _lf_add(file4, "/B/C/", true);

    _lf_sync_message(message2, "2");
    print_tree(0, &root);
    printf("\n");

    char message3[] = "\1\0\0\0";
    _lf_sync_message(message3, "2");
    print_tree(0, &root);
    printf("\n");

    // HierarchyNode* parent = NULL;
    // _hn_get_path(&parent, "/B/B");
    // _lf_sync_files(parent, &parent->child, file4, 10);

}
