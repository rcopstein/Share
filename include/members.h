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

} member;

// Define methods
char* generate_member_id();
size_t size_of_member(member* m);

void free_member(member* m);
int member_load_from_file();

member* get_current_member();
member* get_certain_member(char* id);

void remove_member(char* id);
void add_member(member* member);

void members_for_each(void (*funct)(member*));

size_t serialize_member(member *param, char **buffer);
int deserialize_member(char *input, member *container);

member* build_member(char* id, char* ip, uint16_t port, char* prefix);
