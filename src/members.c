#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include "members.h"
#include "output.h"

// Member struct
typedef struct __member {

    member *content;
    struct __member *next;

} _member;

// Variables
static const char filepath[] = "metadata/members.txt";

static _member* members = NULL;
static uint16_t child_count = 0;

// Return the size of the contents of a member (without string terminators)
size_t size_of_member(member* m) {

    return 3 * sizeof(uint16_t) + 15 + m->prefix_size + m->id_size;

}

// Frees a member from memory
void free_member(member *m) {

    free(m->prefix);
    free(m->id);
    free(m);

}
static void _free_member(_member *m) {

    free_member(m->content);
    free(m);

}

// Load members from file
int member_load_from_file() {

    char buffer[256];
    FILE* file = fopen(filepath, "r");
    if (file == NULL) return error("Failed to open file!\n", NULL);

    _member** head = &members;

    while (fgets(buffer, 255, file)) {

        member* m = (member*) malloc(sizeof(member));
        deserialize_member(buffer, m);

        _member* next = (_member*) malloc(sizeof(_member));
        next->content = m;
        next->next = NULL;

        *head = next;
        *head = next->next;
    }

    if (*head) (*head)->next = NULL;
    return 0;
}

// Generate a new unique ID following the pattern
char* generate_member_id() {

    ++child_count;
    // TO-DO Serialize in the members file

    member *current = get_current_member();
    char* id = malloc(sizeof(char) * (current->id_size + 6));
    sprintf(id, "%s-%d", current->id, child_count);

    return id;

}

// Remove a member from the members list
void remove_member(char* id) {

    _member** aux = &members;

    while (*aux != NULL) {
        if (strcmp((*aux)->content->id, id) == 0) {
            _member* m = *aux;
            *aux = (*aux)->next;
            _free_member(m);
            return;
        }
        *aux = (*aux)->next;
    }

}

// Add a member to the members list
void add_member(member* memb) {

    _member* m = (_member *) malloc(sizeof(_member));
    member* n = (member *) malloc(sizeof(member));

    memcpy(n, memb, sizeof(member));
    m->content = n;

    if (members != NULL) {
        m->next = members->next;
        members->next = m;
    } else {
        members = m;
    }

}

// Return the current member
member* get_current_member() {

    return members->content;

}

// Return a member with a given id
member* get_certain_member(char* id) {

    _member* aux = members;

    while (aux != NULL) {
        if (strcmp(aux->content->id, id) == 0) return aux->content;
        aux = aux->next;
    }

    return NULL;
}

// Execute an operation for each member
void members_for_each(void (*funct)(member*)) {

    _member* aux = members;

    while (aux != NULL) {
        funct(aux->content);
        aux = aux->next;
    }

}

// Print a member to a string (assume buffer is big enough)
size_t serialize_member(member *param, char **buffer) {

    // Calculate the size of the output
    size_t size = size_of_member(param);
    printf("Size is: %zu\n", size);

    // Allocate the buffer, if necessary
    if (*buffer == NULL) *buffer = (char *) malloc(size);

    // Copy the content to the buffer
    char* aux = *buffer;

    memcpy(aux, &(param->id_size), sizeof(uint16_t)); // Copy the ID size
    aux += sizeof(uint16_t);

    memcpy(aux, param->id, param->id_size); // Copy the ID
    aux += param->id_size;

    memcpy(aux, param->ip, 15 * sizeof(char)); // Copy the IP
    aux += 15 * sizeof(char);

    memcpy(aux, &(param->port), sizeof(uint16_t)); // Copy the Port
    aux += sizeof(uint16_t);

    memcpy(aux, &(param->prefix_size), sizeof(uint16_t)); // Copy the prefix size
    aux += sizeof(uint16_t);

    memcpy(aux, param->prefix, param->prefix_size); // Copy the prefix
    //aux += param->prefix_size;

    return size;

}

// Read a line and build a 'member' struct
int deserialize_member(char *input, member *container) {

    uint16_t size = 0;
    char* aux = input;

    memcpy(&size, aux, sizeof(uint16_t)); // Read ID size
    container->id_size = size;
    aux += sizeof(uint16_t);

    container->id = (char *) malloc(size + 1); // Read the ID
    strncpy(container->id, aux, size);
    container->id[size] = '\0';
    aux += size;

    strncpy(container->ip, aux, 15); // Read the IP
    container->ip[15] = '\0';
    aux += 15;

    memcpy(&(container->port), aux, sizeof(uint16_t)); // Read the port
    aux += sizeof(uint16_t);

    memcpy(&size, aux, sizeof(uint16_t)); // Read the prefix size
    container->prefix_size = size;
    aux += sizeof(uint16_t);

    container->prefix = (char *) malloc(size + 1); // Read the prefix
    strncpy(container->prefix, aux, size);
    container->prefix[size] = '\0';
    // aux += size;

    return 0;
}

// Build a member struct based on the parameters (Assume ip is decently sized)
member* build_member(char* id, char* ip, uint16_t port, char* prefix) {

    member* result = (member *) malloc(sizeof(member));

    strncpy(result->ip, ip, 15);
    result->port = port;

    result->id_size = (uint16_t)(strlen(id) * sizeof(char));
    result->id = (char *) malloc(result->id_size + sizeof(char));
    strcpy(result->id, id);

    result->prefix_size = (uint16_t)(strlen(prefix) * sizeof(char));
    result->prefix = (char *) malloc(result->prefix_size + sizeof(char));
    strcpy(result->prefix, prefix);

    return result;

}
