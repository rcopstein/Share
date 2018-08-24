#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Define member struct
typedef struct _member {

    char* id;
    char ip[16];
    char* prefix;
    uint16_t port;
    uint16_t id_size;
    uint16_t prefix_size;

    struct _member *next;

} member;

// Current members
extern member* members;
extern uint16_t child_count;
extern uint16_t members_count;

// Define parsing methods
char* generate_member_id();

member* read_members(char *path);

int parse_member(char* input, member* container);

uint32_t print_member(member* param, char** buffer);

member* build_member(char* id, char* ip, uint16_t port, char* prefix);
