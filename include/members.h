#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Define member struct
typedef struct _member {

    uint16_t id;
    char ip[16];
    uint16_t port;
    struct _member *next;

} member;

// Define parsing methods
member parse_member(char* input);
member* read_members(char *path);
void print_member(member* param, char* buffer);
member build_member(uint16_t id, char* ip, uint16_t port);
