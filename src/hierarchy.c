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


// NEW Tree Management
static void _hn_add(HierarchyNode* parent, HierarchyNode** where, HierarchyNode* what) {

    if (what->file->isDir) parent->folder_count++;
    else parent->file_count++;

    if (*where != NULL) what->next = *where;
    *where = what;

}
static void _hn_rem(HierarchyNode* parent, HierarchyNode** what) {

    if ((*what)->file->isDir) parent->folder_count--;
    else parent->file_count--;
    HierarchyNode* it = *what;
    *what = (*what)->next;
    free_hn(it);

}

static HierarchyNode** _hn_get(HierarchyNode* parent, char* name, char* owner) {

    HierarchyNode** aux = &parent->child;

    while (*aux != NULL) {
        if (!strcmp((*aux)->file->name, name)) {
            if (owner == NULL || !strcmp((*aux)->file->owner, owner)) {
                break;
            }
        }
        aux = &(*aux)->next;
    }

    return aux;

}
static HierarchyNode** _hn_next(HierarchyNode** from, char* owner) {

    while (*from != NULL) {
        if ((*from)->file->owner != NULL && !strcmp((*from)->file->owner, owner)) break;
        from = &(*from)->next;
    }
    return from;

}
static HierarchyNode** _hn_last(HierarchyNode* parent) {

    HierarchyNode** aux = &parent->child;
    while (*aux != NULL) aux = &(*aux)->next;
    return aux;

}

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
        HierarchyNode** next = _hn_get(parent, segment, NULL);

        if (*next == NULL) {
            if (!create) return ENOENT;

            LogicalFile *file = create_lf(true, segment, NULL, NULL);
            _lf_add_rec(parent, file, NULL, false);
            next = _hn_get(parent, segment, NULL); // I might not need this since we are adding it to the last element
        }
        else if (!(*next)->file->isDir) return ENOENT;

        return _lf_add_rec(*next, what, where, create);
    }

}
static int _lf_add(LogicalFile* what, const char* where, bool create) {

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
        HierarchyNode** who = _hn_get(parent, what, owner);
        if (*who == NULL) return ENOENT;
        _hn_rem(parent, who);
        _lf_conf_rem(parent, what);
        return 0;
    }
    else {
        HierarchyNode** next = _hn_get(parent, segment, NULL);
        if (*next == NULL || !(*next)->file->isDir) return ENOENT;
        int res = _lf_rem_rec(*next, where, what, owner, remove_empty);
        if (remove_empty && empty_folder(*next)) _hn_rem(parent, next);
        return res;
    }

}
static int _lf_rem(const char* where, bool remove_empty) {

    char* _where = (char *) malloc(sizeof(char) * (strlen(where) + 1));
    strcpy(_where, where);

    char* name;
    split_path(_where, &name);

    char* owner;
    dissolved_name(name, &owner);
    if (owner == NULL) owner = get_current_member()->id;

    int res = _lf_rem_rec(&root, _where, name, owner, remove_empty);

    if (owner != get_current_member()->id) free(_where);
    free(owner);
    return res;
}

void test() {

    member* self = build_member("1", "127.0.0.1", 4000, "/Users/rcopstein/Desktop/s1");
    initialize_metadata_members(self);

    LogicalFile* file1 = create_lf(false, "a.txt", NULL, "a.txt");
    LogicalFile* file2 = create_lf(false, "a.txt", NULL, "b.txt");
    LogicalFile* file3 = create_lf(false, "c.txt", NULL, "c.txt");
    LogicalFile* file4 = create_lf(false, "d.txt", NULL, "d.txt");

    _lf_add(file1, "/A/B/", true);
    _lf_add(file2, "/B/C/", true);
    _lf_add(file3, "/B/C/", true);
    _lf_add(file4, "/B/B/", true);

    _lf_rem("/B/B/d.txt", true);

    print_tree(0, &root);

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

    if (node == &root) return;

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

    HierarchyNode* parent = node;
    while (parent->parent == NULL) parent = parent->prev;
    parent = parent->parent;

    if (node->file->isDir) parent->folder_count--;
    else parent->file_count--;

    free_hn(node);

}

void rem_subtree(HierarchyNode *node) {

    if (node == NULL) return;

    rem_subtree(node->child);
    rem_subtree(node->next);

    hn_rem(node);

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
static void rem_beneath(HierarchyNode* level, char* name, char* owner, bool automatic) {

    HierarchyNode* aux = level->child;
    while (aux != NULL) {
        if (strcmp(aux->file->name, name) == 0) {
            if (owner == NULL || strcmp(aux->file->owner, owner) == 0) break;
        }
        aux = aux->next;
    }

    HierarchyNode* par = NULL;

    if (aux != NULL) {
        par = aux->parent;
        if (aux->file->isDir) rem_subtree(aux->child);
        hn_rem(aux);
    }

    if (automatic && par != NULL && par->file->isDir) {
        if (par->file_count + par->folder_count == 0) hn_rem(par);
    }

    // print_tree(0, root.child);
    // printf("\n");
}
static HierarchyNode* get_node(char *path, bool create) {

    char* _path = (char *) malloc(strlen(path) + 1);
    strcpy(_path, path);
    char* aux = _path;

    HierarchyNode* parent = NULL;
    HierarchyNode* node = &root;

    char* sep = "/\n";
    char* token;

    while (true) {

        token = strsep(&aux, sep);

        if (token == NULL) break;
        if (strlen(token) == 0) continue;

        parent = node;
        node = find_in_level(node->child, token, NULL);

        if (node == NULL) {
            if (!create) break;
            else {
                LogicalFile* nfile = create_lf(true, token, NULL, NULL);
                HierarchyNode* nnode = create_hn(nfile);
                hn_add_child(parent, nnode, false);
                node = nnode;
            }
        }
    }

    free(_path);
    return node;

}

LogicalFile** list_lf(char* path, int** conflicts) {

    HierarchyNode* folder = get_node(path, false);
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

    printf("Listed %d files\n", aux);

    (*conflicts)[aux] = -1;
    result[aux] = NULL;
    return result;
}


// Conflict Checking
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
int add_lf(LogicalFile *file, char *path, bool create) {

    char* hasAt = strchr(file->name, '@'); // Don't allow names with '@'
    if (hasAt != NULL) return -EINVAL;

    HierarchyNode* parent = get_node(path, create);
    if (parent == NULL || !parent->file->isDir) return -ENOENT;

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
    dissolved_name(name, &owner);

    HierarchyNode* parent = get_node(_path, false);
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
    dissolved_name(name, &owner);

    HierarchyNode* parent = &root;
    if (_path != NULL && strlen(_path) > 0) parent = get_node(_path, false);

    HierarchyNode* node = parent;
    if (parent != NULL && name != NULL && strlen(name) > 0) node = find_in_level(parent->child, name, owner);

    if (owner != NULL) free(owner);
    free(_path);

    LogicalFile* res = node != NULL ? node->file : NULL;
    return res;
}
int rem_lf(char *path, bool automatic) {

    char* _path = (char *) malloc(strlen(path) + 1);
    strcpy(_path, path);
    int res = 0;

    char *name, *owner;
    split_path(_path, &name);
    dissolved_name(name, &owner);

    HierarchyNode* parent = get_node(_path, false);
    if (parent != NULL && parent->file->isDir) {
        rem_beneath(parent, name, owner, automatic);
        dissolve_conflict(parent->child, name);
    } else res = -ENOENT;

    if (owner != NULL) free(owner);
    free(_path);

    return res;
}


// Cleanup (Remove metadata of files that got removed)
static void cleanup(HierarchyNode* node, char* owner, uint16_t current_level) {

    if (node == NULL) return;

    cleanup(node->child, owner, current_level);
    cleanup(node->next, owner, current_level);

    if ((node->file->isDir && node->file_count + node->folder_count == 0) ||
        (!node->file->isDir && (owner == NULL || !strcmp(owner, node->file->owner)) && node->seq_num < current_level))
    {
        hn_rem(node);
    }
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

    // printf("Deserialized name: %s\n", name);

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

        // printf("Deserialized owner: %s\n", owner);

        memcpy(&size, buffer, sizeof(uint16_t)); // Read realpath size
        buffer += sizeof(uint16_t);

        char *realpath = (char *) malloc(sizeof(char) * (size + 1)); // Allocate realpath
        memcpy(realpath, buffer, size); // Read realpath
        (*file)->realpath = realpath; // Assign realpath
        realpath[size] = '\0';
        //buffer += size;

        // printf("Deserialized realpath: %s\n", realpath);

    } else {
        (*file)->owner = NULL;
        (*file)->realpath = NULL;
    }

    return 0;

}


static size_t copy_member_buffer(uint16_t level, char** buffer, size_t offset, size_t* message_size, HierarchyNode* node) {

    if (node == NULL) return offset;

    // Calculate Size and Offset
    size_t node_size = size_of_lf(node->file) + sizeof(uint16_t);
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
        serialize_file(&where_to, node->file);

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
char* build_hierarchy_message(size_t prefix_size, char* prefix, size_t* size) {

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



// NOT IN USE
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
char* _build_hierarchy_message(size_t prefix_size, char* prefix, size_t* size) {

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
// END NOT IN USE

static LogicalFile* _read_logical_file(char** message) {

    LogicalFile* file = (LogicalFile *) malloc(sizeof(LogicalFile));
    deserialize_file(*message, &file);
    (*message) += size_of_lf(file);

    if (file->name[0] < '0' || file->name[0] > '9') warning("@ Deserialized File name is '%s'\n", file->name);

    return file;

}
static HierarchyNode* _sync_logical_file(uint16_t sn, HierarchyNode *parent, HierarchyNode* current, LogicalFile *file) {

    if (file->isDir) {
        HierarchyNode* node = find_in_level(current->next, file->name, NULL);
        if (node == NULL) {
            node = create_hn(file);
            // check_conflict(parent->child, node);
            if (parent->child != NULL && current->file != NULL) hn_add_next(current, node); // Add next to the current
            else hn_add_child(parent, node, false);
        }
        return node;
    }

    else {
        while (1) {
            HierarchyNode *node = find_in_level(current->next, NULL, file->owner);

            if (node == NULL) { // This is a new entry for this owner
                // printf("%s is new!\n", file->name);
                node = create_hn(file);
                node->seq_num = sn;
                check_conflict(parent->child, node);
                if (parent->child != NULL && current->file != NULL) hn_add_next(current, node); // Add next to the current
                else hn_add_child(parent, node, false);
                return node;

            } else if (strcmp(file->name, node->file->name) == 0) { // I already have this entry
                // printf("I already have %s\n", node->file->name);
                node->seq_num = sn; // Update entry
                return node;

            } else { // The names are different, my entry has been removed
                // printf("I'm removing %s\n", node->file->name);

                if (current->next == node) current->next = current->next->next;

                char* name = (char *) malloc(sizeof(char) * (strlen(node->file->name) + 1));
                strcpy(name, node->file->name);
                hn_rem(node);
                dissolve_conflict(parent->child, name);
                free(name);
            }
        }
    }
}
static void _read_hierarchy_message_entry(uint16_t sn, HierarchyNode* parent, uint16_t* level, uint16_t* count, char** message) {

    if (*level == 0) return;

    uint16_t l;

    HierarchyNode first = { .file = NULL, .next = parent->child };
    HierarchyNode* current = &first;

    do {

        LogicalFile* file = _read_logical_file(message);
        current = _sync_logical_file(sn, parent, current, file);
        free_lf(file);

        memcpy(&l, *message, sizeof(uint16_t));
        (*message) += sizeof(uint16_t);
        (*count)++;

        if (l > *level) _read_hierarchy_message_entry(sn, current, &l, count, message);

    }
    while (l == *level);
    *level = l;

}
void read_hierarchy_message(uint16_t sn, member* m, uint16_t* count, char* message) {

    // Read first level
    uint16_t level;
    memcpy(&level, message, sizeof(uint16_t));

    // Call recursive read of message
    char* aux = message + sizeof(uint16_t);
    HierarchyNode* node = &root;
    _read_hierarchy_message_entry(sn, node, &level, count, &aux);

    // Cleanup old files and empty folders
    cleanup(root.child, m->id, sn);

    printf("Count in the end was %d\n", *count);

    // printf("\n");
    // print_tree(0, root.child);
    // printf("\n");

}
