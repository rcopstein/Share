#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "members.h"
#include "output.h"

// Return the information of the current member
int current_member(char* path, member* container) {

    // Open the members file
    FILE* file = fopen(path, "r");
    if (file == NULL) {
        error("Failed to open the members file!\n", NULL);
        return 1;
    }

    // Get the current line
    char line[256];
    if (!fgets((char *)line, 255, file)) {
        error("Failed to read the members file!\n", NULL);
        fclose(file);
        return 1;
    }

    // Return the parsed member
    parse_member(line, container);

    return 0;

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

    return 0;
}

// Print a member to a string (assume buffer is big enough)
void print_member(member* param, char* buffer) {

    sprintf(buffer, "%d %s %d %d %d", param->id, param->ip, param->port, 0, 0);

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

        char* token = strtok(buffer, " ");
        printf("Token: %s\n", token);
        next->id = (uint16_t) strtol(token, NULL, 10);

        token = strtok(NULL, " ");
        printf("Token: %s\n", token);
        strncpy(next->ip, token, 15);

        token = strtok(NULL, " ");
        printf("Token: %s\n", token);
        next->port = (uint16_t) strtol(token, NULL, 10);

        next->next = NULL;
        last->next = next;
        last = next;
    }

    return m.next;
}

// Build a member struct based on the parameters (Assume ip is decently sized)
member build_member(uint16_t id, char* ip, uint16_t port) {

    member result;

    strncpy(result.ip, ip, 15);
    result.port = port;
    result.next = NULL;
    result.id = id;

    return result;

}
