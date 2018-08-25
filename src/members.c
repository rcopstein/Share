#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "members.h"
#include "output.h"

// Member struct
typedef struct __member {

    char* id;
    char ip[16];
    char* prefix;
    uint16_t port;
    uint16_t id_size;
    uint16_t prefix_size;

    struct __member *next;

} _member;

// Variables
static const char filepath[] = "metadata/members.txt";

static _member* members = NULL;
static uint16_t child_count = 0;

// Generate a new unique ID following the pattern
char* generate_member_id() {

    ++child_count;
    // TO-DO Serialize in the members file

    char* id = malloc(sizeof(char) * (strlen(members->id) + 6));
    sprintf(id, "%s-%d", members->id, child_count);

    return id;

}

// Remove a member from the members list
void remove_member(char* id) {

    _member** aux = &members;

    while (*aux != NULL) {
        if (strcmp((*aux)->id, id) == 0) {
            _member* m = *aux;
            *aux = (*aux)->next;
            free(m);
            return;
        }
        *aux = (*aux)->next;
    }

}

// Add a member to the members list
void add_member(member* member) {

    _member* m = (_member *) malloc(sizeof(_member));
    memcpy(m, member, sizeof(member));

    m->next = members->next;
    members->next = m;

}

// Load members from file
int member_load_from_file() {

    char buffer[256];
    FILE* file = fopen(filepath, "r");
    if (file == NULL) return error("Failed to open file!\n", NULL);

    _member** head = &members;

    while (fgets(buffer, 255, file)) {
        _member* next = (_member*) malloc(sizeof(_member));
        deserialize_member(buffer, next);

        *head = next;
        *head = next->next;
    }

    return 0;
}

// Return the current member
member* get_current_member() {

    return (member*) members;

}

// Return a member with a given id
member* get_certain_member(char* id) {

    _member* aux = members;

    while (aux != NULL) {
        if (strcmp(aux->id, id) == 0) return (member *) aux;
        aux = aux->next;
    }

    return NULL;
}

// Execute an operation for each member
void members_for_each(void (*funct)(member*)) {

    _member* aux = members;

    while (aux != NULL) {
        funct((member *) aux);
        aux = aux->next;
    }

}

// Print a member to a string (assume buffer is big enough)
int serialize_member(member *param, char **buffer) {

    // Calculate the size of the output
    uint32_t size = sizeof(member); // 2 string terminators
    size += param->prefix_size;
    size += param->id_size;

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

    container->id = (char *) malloc(size); // Read the ID
    strncpy(container->id, aux, size);
    aux += size;

    strncpy(container->ip, aux, 15); // Read the IP
    aux += 15;

    memcpy(&(container->port), aux, sizeof(uint16_t)); // Read the port
    aux += sizeof(uint16_t);

    memcpy(&size, aux, sizeof(uint16_t)); // Read the prefix size
    container->prefix_size = size;
    aux += sizeof(uint16_t);

    container->prefix = (char *) malloc(size); // Read the prefix
    strncpy(container->prefix, aux, size);
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
