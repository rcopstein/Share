#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "members.h"
#include "output.h"

// Read a line and build a 'member' struct
member parse_member(char* input) {

    member result;
    result.next = NULL;

    // Copy ID
    char* token = strtok(input, " ");
    result.id = (uint16_t) strtol(token, NULL, 10);

    // Copy IP
    token = strtok(NULL, " ");
    strncpy(result.ip, token, 15);

    // Copy Port
    token = strtok(NULL, " ");
    result.port = (uint16_t) strtol(token, NULL, 10);

    return result;
}

// Print a member to a string (assume buffer is big enough)
void print_member(member* param, char* buffer) {

    sprintf(buffer, "%d %s %d %d %d", param->id, param->ip, param->port, 0, 0);

}

// Reads members from the members file
member* read_members(char* path) {

    member m;
    m.next = NULL;
    member* last = &m;

    char buffer[256];
    FILE* file = fopen(path, "r");
    if (file == NULL) return NULL;

    printf("Opening %s\n", path);

    while (fgets(buffer, 255, file)) {

        member* next = (member*) malloc(sizeof(member));

        char* token = strtok(buffer, " ");
        printf("Token: %s\n", token);
        next->id = atoi(token);

        token = strtok(NULL, " ");
        printf("Token: %s\n", token);
        strncpy(next->ip, token, 15);

        token = strtok(NULL, " ");
        printf("Token: %s\n", token);
        next->port = atoi(token);

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
