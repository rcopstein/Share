#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "members.h"
#include "output.h"

member parse_member(char* input) {

    // Read a line and build a 'member' struct

}

void print_member(member* param, char* buffer) {

    // Print a member to a string (assume buffer is big enough)

}

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

member build_member(uint16_t id, char* ip, uint16_t port) {

    // Build a member struct based on the parameters (Assume ip is decently sized)

}