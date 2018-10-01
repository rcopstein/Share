#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <members.h>

#include "output.h"
#include "hierarchy.h"
#include "semaphores.h"

typedef struct _HierarchyNode {

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


// Auxiliar Functions
char* resolve_path(LogicalFile* file) {

    member* current = get_current_member();

    size_t size = strlen(file->realpath);
    size += current->prefix_size;
    size += strlen(file->owner);
    size += sizeof(char) * 3;

    char* result = (char *) malloc(size);
    sprintf(result, "%s/%s/%s", current->prefix, file->owner, file->realpath);

    return result;
}
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
    print_tree(level, root->next);

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
    result->child = NULL;
    result->parent = NULL;
    result->prev = NULL;
    result->next = NULL;

    return result;

}
static void free_hn(HierarchyNode* node) {

    free_lf(node->file);
    free(node);

}


// Tree Management
static void hn_add_next(HierarchyNode* prev, HierarchyNode* node) {

    if (prev->next != NULL) prev->next->prev = node;

    node->next = prev->next;
    prev->next = node;
    node->prev = prev;

    while (prev->parent == NULL) prev = prev->prev;
    if (node->file->isDir) prev->parent->folder_count++;
    else prev->parent->file_count++;

}
static void hn_add_child(HierarchyNode *parent, HierarchyNode *node, bool last) {

    if (parent->child != NULL) {

        if (last) {
            HierarchyNode *aux = parent->child;
            while (aux->next != NULL) aux = aux->next;
            hn_add_next(aux, node);
            return;
        }

        parent->child->parent = NULL;
        parent->child->prev = node;

    }

    if (node->file->isDir) parent->folder_count++;
    else parent->file_count++;

    node->next = parent->child;
    node->parent = parent;
    parent->child = node;

}
static void hn_rem(HierarchyNode* node) {

    if (node->prev != NULL) {
        if (node->next != NULL) node->next->prev = node->prev;
        node->prev->next = node->next;
    }

    if (node->parent != NULL) {
        if (node->next != NULL) {
            node->next->prev = node->prev;
            node->next->parent = node->parent;
        }
        node->parent->child = node->next;
    }

    free_hn(node);

}

static HierarchyNode* find_in_level(HierarchyNode* level, char* name, char* owner) {

    while (level != NULL) {
        if (name == NULL || strcmp(level->file->name, name) == 0) {
            if (owner == NULL || (level->file->owner != NULL && strcmp(level->file->owner, owner) == 0)) {
                return level;
            }
        }
        level = level->next;
    }

    return NULL;

}
static void rem_beneath(HierarchyNode* parent, char* name, char* owner) {

    print_tree(0, root.child);
    printf("\n");

    HierarchyNode* aux = parent->child;
    while (aux != NULL) {
        if (strcmp(aux->file->name, name) == 0) {
            if (owner == NULL || strcmp(aux->file->owner, owner) == 0) {
                if (aux->file->isDir) parent->folder_count--;
                else parent->file_count--;
                break;
            }
        }
        aux = aux->next;
    }

    if (aux != NULL) hn_rem(aux);
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

        member* owner = get_certain_member(folder->file->owner);
        if (owner == NULL || member_get_state(owner, AVAIL)) {

            (*conflicts)[aux] = folder->conflict_free ? 0 : 1;
            result[aux] = folder->file;
            aux++;

        }

        folder = folder->next;
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
}

void dissolve_conflict(HierarchyNode* level, char* name) {

    HierarchyNode* conflict1 = find_in_level(level, name, NULL);
    if (conflict1 == NULL) return; // No conflicts, do nothing

    HierarchyNode* conflict2 = find_in_level(conflict1->next, name, NULL);
    if (conflict2 != NULL) return; // More than one conflict, do nothing

    conflict1->conflict_free = true; // Only conflict means no conflict!

}
int check_conflict(HierarchyNode* level, HierarchyNode* file) {

    HierarchyNode* conflict = find_in_level(level, file->file->name, NULL);
    if (conflict == NULL) { file->conflict_free = true; return 0; }

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

    //if (file->isDir) parent->folder_count++;
    //else parent->file_count++;

    HierarchyNode* node = create_hn(file);
    HierarchyNode* level = parent->child;
    check_conflict(level, node);

    hn_add_child(parent, node, true);
    return 0;
}
int ren_lf(char *path, char* new_name) {

    printf("From '%s' to '%s'\n", path, new_name);

    char* at = strchr(new_name, '@');
    if (at != NULL) return -EINVAL;

    char* _path = (char *) malloc(strlen(path) + 1);
    strcpy(_path, path);
    int res = 0;

    char *name, *owner;
    split_path(_path, &name);
    dissolve_name(name, &owner);

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

            dissolve_conflict(level, to_rn->file->name);
            dissolve_conflict(level, oname);

            free(oname);

        }
    }

    free(_path);
    return res;
}
LogicalFile* get_lf(char *path) {

    char* _path = (char *) malloc(strlen(path) + 1);
    strcpy(_path, path);

    char *name, *owner;
    split_path(_path, &name);
    dissolve_name(name, &owner);

    HierarchyNode* parent = &root;
    if (_path != NULL && strlen(_path) > 0) parent = get_node(_path);

    HierarchyNode* node = parent;
    if (parent != NULL && name != NULL && strlen(name) > 0) node = find_in_level(parent->child, name, owner);

    if (owner != NULL) free(owner);
    free(_path);

    LogicalFile* res = node != NULL ? node->file : NULL;
    return res;
}
int rem_lf(char *path) {

    char* _path = (char *) malloc(strlen(path) + 1);
    strcpy(_path, path);
    int res = 0;

    char *name, *owner;
    split_path(_path, &name);
    dissolve_name(name, &owner);

    HierarchyNode* parent = get_node(_path);
    if (parent != NULL && parent->file->isDir) {
        rem_beneath(parent, name, owner);
        dissolve_conflict(parent->child, name);
    } else res = -ENOENT;

    if (owner != NULL) free(owner);
    free(_path);

    return res;
}


// Serialization
size_t size_of_lf(LogicalFile* file) {

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
size_t serialize_file(char** buffer, LogicalFile* file) {

    char* aux = *buffer;
    size_t size = size_of_lf(file);
    uint16_t name_size = (uint16_t) strlen(file->name);

    if (*buffer == NULL) *buffer = (char *) malloc(size);

    memcpy(aux, &name_size, sizeof(uint16_t)); // Copy name size
    aux += sizeof(uint16_t);

    memcpy(aux, file->name, sizeof(char) * name_size); // Copy name
    aux += sizeof(char) * name_size;

    memcpy(aux, &file->isDir, sizeof(uint8_t)); // Copy is directory
    aux += sizeof(uint8_t);

    if (!file->isDir) {

        uint16_t owner_size = (uint16_t) strlen(file->owner);
        uint16_t realpath_size = (uint16_t) strlen(file->realpath);

        memcpy(aux, &owner_size, sizeof(uint16_t)); // Copy owner size
        aux += sizeof(uint16_t);

        memcpy(aux, file->owner, sizeof(char) * owner_size); // Copy owner
        aux += sizeof(char) * owner_size;

        memcpy(aux, &realpath_size, sizeof(uint16_t)); // Copy realpath size
        aux += sizeof(uint16_t);

        memcpy(aux, file->realpath, sizeof(char) * realpath_size); // Copy realpath
        //aux += sizeof(char) * realpath_size;

    }

    return size;

}
int deserialize_file(char* buffer, LogicalFile** file) {

    if (*file == NULL) {
        *file = (LogicalFile *) malloc(sizeof(LogicalFile));
    }

    uint16_t size;

    memcpy(&size, buffer, sizeof(uint16_t)); // Read name size
    buffer += sizeof(uint16_t);

    char* name = (char *) malloc(sizeof(char) * (size + 1)); // Allocate name
    memcpy(name, buffer, sizeof(char) * size); // Read name
    (*file)->name = name; // Assign name
    name[size] = '\0';
    buffer += size;

    memcpy(&(*file)->isDir, buffer, sizeof(uint8_t)); // Read is directory
    buffer += sizeof(uint8_t);

    if (!(*file)->isDir) {

        memcpy(&size, buffer, sizeof(uint16_t)); // Read owner size
        buffer += sizeof(uint16_t);

        char* owner = (char *) malloc(sizeof(char) * (size + 1)); // Allocate owner
        memcpy(owner, buffer, size); // Read owner
        (*file)->owner = owner; // Assign owner
        owner[size] = '\0';
        buffer += size;

        memcpy(&size, buffer, sizeof(uint16_t)); // Read realpath size
        buffer += sizeof(uint16_t);

        char *realpath = (char *) malloc(sizeof(char) * (size + 1)); // Allocate realpath
        memcpy(realpath, buffer, size); // Read realpath
        (*file)->realpath = realpath; // Assign realpath
        realpath[size] = '\0';
        //buffer += size;

    } else {
        (*file)->owner = NULL;
        (*file)->realpath = NULL;
    }

    return 0;

}

static size_t total_size(HierarchyNode* node, uint16_t* count, char* owner) {

    if (node == NULL) return 0;

    size_t size = 0;

    if (node->file->isDir) {

        size = total_size(node->child, count, owner);
        if (size > 0) {
            size += size_of_lf(node->file) + sizeof(uint16_t);
            (*count)++;
        }

    }

    else {

        if (strcmp(node->file->owner, owner) == 0) {
            size += size_of_lf(node->file) + sizeof(uint16_t);
            (*count)++;
        }

    }

    size += total_size(node->next, count, owner);
    return size;

}
static bool serialize_all(HierarchyNode* node, uint16_t level, char** message, char* owner) {

    if (node == NULL) return false;

    bool flag = false;
    size_t size = 0;

    if (node->file->isDir || strcmp(node->file->owner, owner) == 0) { // It's a file that belongs to me

        memcpy(*message, &level, sizeof(uint16_t)); // Copy Level
        (*message) += sizeof(uint16_t);

        size = serialize_file(message, node->file); // Serialize Message
        (*message) += size;
        flag = true;

    }

    if (node->file->isDir) {

        flag = serialize_all(node->child, (uint16_t)(level+1), message, owner); // Remove folder entry if it is empty!
        if (!flag) (*message) -= size + sizeof(uint16_t);

    }

    return serialize_all(node->next, level, message, owner) || flag;
}
char* build_hierarchy_message(size_t prefix_size, char* prefix, size_t* size) {

    uint16_t count = 0;
    *size = prefix_size;
    *size += sizeof(uint16_t);
    *size += sizeof(uint16_t);
    *size += sizeof(uint16_t);
    member* current = get_current_member();

    HierarchyNode* node = root.child;
    *size += total_size(node, &count, current->id);

    char* message = (char *) malloc(prefix_size + *size);
    char* aux = message;

    if (message == NULL) {
        error("Failed to allocate memory!\n", NULL);
        return NULL;
    }

    if (prefix != NULL) {
        memcpy(aux, prefix, prefix_size); // Copy Prefix
        aux += prefix_size;
    }

    memcpy(aux, &seq_num, sizeof(uint16_t)); // Copy the sequence number
    aux += sizeof(uint16_t);

    memcpy(aux, &count, sizeof(uint16_t)); // Copy the number of files
    aux += sizeof(uint16_t);

    serialize_all(node, 1, &aux, current->id); // Serialize all files

    count = 0;
    memcpy(aux, &count, sizeof(uint16_t)); // Copy the terminator
    // aux += sizeof(uint16_t);

    return message;

}

static LogicalFile* _read_logical_file(char** message) {

    LogicalFile* file = (LogicalFile *) malloc(sizeof(LogicalFile));
    deserialize_file(*message, &file);
    (*message) += size_of_lf(file);

    return file;

}
static HierarchyNode* _sync_logical_file(HierarchyNode *parent, HierarchyNode* current, LogicalFile *file) {

    if (file->isDir) {
        HierarchyNode* node = find_in_level(current->next, file->name, NULL);
        if (node == NULL) {
            node = create_hn(file);
            check_conflict(parent->child, node);
            if (parent->child != NULL && current->file != NULL) hn_add_next(current, node); // Add next to the current
            else hn_add_child(parent, node, false);
        }
        return node;
    }

    else {
        while (1) {
            HierarchyNode *node = find_in_level(current->next, NULL, file->owner);

            if (node == NULL) { // This is a new entry for this owner
                printf("%s is new!\n", file->name);
                node = create_hn(file);
                check_conflict(parent->child, node);
                if (parent->child != NULL && current->file != NULL) hn_add_next(current, node); // Add next to the current
                else hn_add_child(parent, node, false);
                return node;

            } else if (strcmp(file->name, node->file->name) == 0) { // I already have this entry
                printf("I already have %s\n", node->file->name);
                // TODO: Update entry
                return node;

            } else { // The names are different, my entry has been removed
                printf("I'm removing %s\n", node->file->name);

                char* name = (char *) malloc(sizeof(char) * (strlen(node->file->name) + 1));
                strcpy(name, node->file->name);
                hn_rem(node);
                dissolve_conflict(parent->child, name);
                free(name);
            }
        }
    }
}
static void _read_hierarchy_message_entry(HierarchyNode* parent, uint16_t* level, char** message) {

    if (*level == 0) return;

    uint16_t l;

    HierarchyNode first = { .file = NULL, .next = parent->child };
    HierarchyNode* current = &first;

    do {

        LogicalFile* file = _read_logical_file(message);
        current = _sync_logical_file(parent, current, file);
        free_lf(file);

        memcpy(&l, *message, sizeof(uint16_t));
        (*message) += sizeof(uint16_t);

        if (l > *level) _read_hierarchy_message_entry(current, &l, message);

    }
    while (l == *level);
    *level = l;

}
void read_hierarchy_message(char* message) {

    // Read first level
    uint16_t level;
    memcpy(&level, message, sizeof(uint16_t));

    // Call recursive read of message
    char* aux = message + sizeof(uint16_t);
    HierarchyNode* node = &root;
    _read_hierarchy_message_entry(node, &level, &aux);

    printf("\n");
    print_tree(0, root.child);
    printf("\n");

}
