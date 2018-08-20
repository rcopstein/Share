#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "members.h"
#include "output.h"

// Print a member to a string (assume buffer is big enough)
void print_member(member* param, char* buffer) {

    sprintf(buffer, "%d %s %d %s %d %d", param->id, param->ip, param->port, param->prefix, 0, 0);

}

// Read a line and build a 'member' struct
int parse_member(char* input, member* container) {

    container->next = NULL;

    // Copy ID
    char* token = strtok(input, " ");
    if (token == NULL) return error("Failed to parse member from input!\n", NULL);
    container->id = (uint16_t) strtol(token, NULL, 10);

    // Copy IP
    token = strtok(NULL, " ");
    if (token == NULL) return error("Failed to parse member from input!\n", NULL);
    strncpy(container->ip, token, 15);

    // Copy Port
    token = strtok(NULL, " ");
    if (token == NULL) return error("Failed to parse member from input!\n", NULL);
    container->port = (uint16_t) strtol(token, NULL, 10);

    // Copy Prefix
    token = strtok(NULL, " ");
    if (token == NULL) return error("Failed to parse member from input!\n", NULL);
    container->prefix = (char *) malloc(strlen(token) * sizeof(char) + 1);
    strcpy(container->prefix, token);

    return 0;
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

// Build a member struct based on the parameters (Assume ip is decently sized)
member build_member(uint16_t id, char* ip, uint16_t port, char* prefix) {

    member result;

    strncpy(result.ip, ip, 15);
    result.port = port;
    result.next = NULL;
    result.id = id;

    result.prefix = (char *) malloc(strlen(prefix) * sizeof(char) + 1);
    strcpy(result.prefix, prefix);

    return result;

}
