#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "members.h"
#include "output.h"

member* members = NULL;
uint16_t child_count = 0;
uint16_t members_count = 0;

// Generate a new unique ID following the pattern
char* generate_member_id() {

    ++child_count;
    // TO-DO Serialize in the members file

    char* id = malloc(sizeof(char) * (strlen(members->id) + 6));
    sprintf(id, "%s-%d", members->id, child_count);

    return id;

}

// Reads members from the members file
member* read_members(char* path) {

    char buffer[256];
    FILE* file = fopen(path, "r");
    if (file == NULL) return NULL;

    member m;
    m.next = NULL;
    member* last = &m;

    while (fgets(buffer, 255, file)) {
        member* next = (member*) malloc(sizeof(member));
        parse_member(buffer, next);
        last->next = next;
        last = next;
    }

    return m.next;
}

// Print a member to a string (assume buffer is big enough)
uint32_t print_member(member* param, char** buffer) {

    // Calculate the size of the output
    uint32_t size = sizeof(member) + 2; // 2 string terminators
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
int parse_member(char* input, member* container) {

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

    container->next = NULL;
    return 0;
}

// Build a member struct based on the parameters (Assume ip is decently sized)
member* build_member(char* id, char* ip, uint16_t port, char* prefix) {

    member* result = (member *) malloc(sizeof(member));

    strncpy(result->ip, ip, 15);
    result->port = port;
    result->next = NULL;

    result->id_size = (uint16_t)(strlen(id) * sizeof(char));
    result->id = (char *) malloc(result->id_size + sizeof(char));
    strcpy(result->id, id);

    result->prefix_size = (uint16_t)(strlen(prefix) * sizeof(char));
    result->prefix = (char *) malloc(result->prefix_size + sizeof(char));
    strcpy(result->prefix, prefix);

    return result;

}
