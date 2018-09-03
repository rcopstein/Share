#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>

#define AVAIL 0x00001
#define MOUNT 0x00002
#define RECP  0x00004

// Define member struct
typedef struct _member {

    // Clocks
    uint16_t member_clock;

    // State Management
    uint8_t avail;
    uint16_t state;
    sem_t* editable;

    // Identification
    char* id;
    uint16_t id_size;

    // Addressing
    char ip[16];
    uint16_t port;

    // NFS
    char* prefix;
    uint16_t prefix_size;

} member;

// Version Clock
uint16_t get_member_clock();
uint16_t inc_member_clock();

// Define methods
int initialize_metadata_members(member *m);
int remove_metadata_members();

char* generate_member_id();
void print_member(member* m);
size_t size_of_member(member* m);

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

uint16_t member_get_state(member* m, uint16_t state);
uint16_t member_set_state(member* m, uint16_t state);
uint16_t member_unset_state(member* m, uint16_t state);
