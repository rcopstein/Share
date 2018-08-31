#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define AVAIL 0x01
#define MOUNT 0x02

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
int initialize_metadata_members(member *m);
int remove_metadata_members();

char* generate_member_id();
void print_member(member* m);
size_t size_of_member(member* m);

int member_load_from_file();

member* get_current_member();
member* get_certain_member(char* id);

void add_member(member* member);
void remove_member(char* id);

void members_for_each(void (*funct)(member*));

size_t serialize_member(member *param, char **buffer);
int deserialize_member(char *input, member *container);

member* build_member(char* id, char* ip, uint16_t port, char* prefix);
void free_member(member* m);

char* build_members_message();

uint8_t member_get_state(member* m, uint16_t* state);
uint8_t member_set_state(member* m, uint16_t* state);
uint8_t member_unset_state(member* m, uint16_t* state);