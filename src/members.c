#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <nops.h>
#include <zconf.h>
#include <background.h>

#include "protocol_mont.h"
#include "members.h"
#include "output.h"
#include "server.h"


// Member struct
typedef struct _mlist {

    member *content;
    struct _mlist *next;

} mlist;

// Variables
static const char filepath[] = "metadata/members.txt";

static member* self = NULL;
static mlist* members = NULL;
static uint16_t child_count = 0;

// Version Clock
static uint16_t member_clock = 0;
static sem_t* member_clock_sem = NULL;

uint16_t get_member_clock() {

    return member_clock;

}
uint16_t inc_member_clock() {

    if (member_clock_sem == NULL) member_clock_sem = sem_open("", O_CREAT, 1);

    sem_wait(member_clock_sem);
    ++member_clock;
    sem_post(member_clock_sem);

    return member_clock;

}

// Methods
static member* copy_member(member* m) {

    member* result = (member *) malloc(sizeof(member));
    memcpy(result, m, sizeof(member));

    result->id = (char *) malloc(result->id_size);
    result->prefix = (char *) malloc(result->prefix_size);

    strncpy(result->id, m->id, result->id_size);
    strncpy(result->prefix, m->prefix, result->prefix_size);

    return result;

}

int initialize_metadata_members(member *m) {

    FILE* file;

    if ((file = fopen(filepath, "wb+")) == NULL)
        return error("Failed to open file '%s'!\n", (char *)filepath);

    char* line = NULL;
    size_t size = serialize_member(m, &line);
    if (line == NULL) {
        fclose(file);
        remove(filepath);
        return error("Failed to serialize member!\n", NULL);
    }

    fwrite(line, size, 1, file);
    fclose(file);
    free(line);

    self = m;

    return 0;
}

void print_member(member* m) {

    printf("ID: %s\n", m->id);
    printf("Prefix: %s\n", m->prefix);
    printf("IP: %s\n", m->ip);
    printf("Port: %d\n", m->port);
    printf("\n");

}

size_t size_of_member(member* m) {

    return 3 * sizeof(uint16_t) + 15 + m->prefix_size + m->id_size;

}

void free_member(member *m) {

    sem_unlink(m->id);
    free(m->prefix);
    free(m->id);
    free(m);

}
static void _free_member(mlist *m) {

    free_member(m->content);
    free(m);

}

char* generate_member_id() {

    ++child_count;
    // TO-DO Serialize in the members file

    member *current = get_current_member();
    char* id = malloc(sizeof(char) * (current->id_size + 6));
    sprintf(id, "%s-%d", current->id, child_count);

    return id;

}

void remove_member(char* id) {

    mlist** aux = &members;

    while (*aux != NULL) {
        if (strcmp((*aux)->content->id, id) == 0) {
            mlist* m = *aux;
            *aux = (*aux)->next;
            inc_member_clock();
            _free_member(m);
            return;
        }
        *aux = (*aux)->next;
    }

}

void add_member(member* memb) {

    mlist* m = (mlist *) malloc(sizeof(mlist));
    member* n = copy_member(memb);
    m->content = n;

    mlist** head = &members;
    m->next = *head;
    *head = m;

    start_background(n);
    inc_member_clock();

}

member* get_current_member() {

    return self;

}

member* get_certain_member(char* id) {

    mlist* aux = members;

    while (aux != NULL) {
        if (strcmp(aux->content->id, id) == 0) return aux->content;
        aux = aux->next;
    }

    return NULL;
}

void members_for_each(void (*funct)(member*)) {

    mlist* aux = members;

    while (aux != NULL) {
        member* curr = aux->content;
        aux = aux->next;

        funct(curr);
    }

}
void _members_for_each(void (*funct)(mlist*)) {

    mlist* aux = members;

    while (aux != NULL) {
        mlist* curr = aux;
        aux = aux->next;
        funct(curr);
    }

}

int remove_metadata_members() {

    _members_for_each(_free_member);
    free_member(self);
    members = NULL;

    return remove(filepath);

}

size_t serialize_member(member *param, char **buffer) {

    // Calculate the size of the output
    size_t size = size_of_member(param);

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

int deserialize_member(char *input, member *container) {

    uint16_t size = 0;
    char* aux = input;

    memcpy(&size, aux, sizeof(uint16_t)); // Read ID size
    container->id_size = size;
    aux += sizeof(uint16_t);

    container->id = (char *) malloc(size + 1); // Read the ID
    strncpy(container->id, aux, size);
    container->id[size] = '\0';
    aux += size;

    strncpy(container->ip, aux, 15); // Read the IP
    container->ip[15] = '\0';
    aux += 15;

    memcpy(&(container->port), aux, sizeof(uint16_t)); // Read the port
    aux += sizeof(uint16_t);

    memcpy(&size, aux, sizeof(uint16_t)); // Read the prefix size
    container->prefix_size = size;
    aux += sizeof(uint16_t);

    container->prefix = (char *) malloc(size + 1); // Read the prefix
    strncpy(container->prefix, aux, size);
    container->prefix[size] = '\0';
    // aux += size;

    container->state = 0;
    container->editable = sem_open(container->id, O_CREAT, 0200, 1);

    container->member_clock = 0;

    return 0;
}

member* build_member(char* id, char* ip, uint16_t port, char* prefix) {

    member* result = (member *) malloc(sizeof(member));

    strncpy(result->ip, ip, 15);
    result->port = port;

    result->id_size = (uint16_t)(strlen(id) * sizeof(char));
    result->id = (char *) malloc(result->id_size + sizeof(char));
    strcpy(result->id, id);

    result->prefix_size = (uint16_t)(strlen(prefix) * sizeof(char));
    result->prefix = (char *) malloc(result->prefix_size + sizeof(char));
    strcpy(result->prefix, prefix);

    result->avail = 0;
    result->state = 0;
    result->editable = sem_open(id, O_CREAT, 0200, 1);

    result->member_clock = 0;

    return result;

}

char* build_members_message() {

    uint16_t count = 0;
    size_t size = sizeof(uint16_t);

    mlist* m = members;
    while (m != NULL) {
        size += size_of_member(m->content); // Count the size of the message
        ++count; // Count the number of members
        m = m->next;
    }

    char* message = (char *) malloc(size);
    char* aux = message;

    if (message == NULL) {
        error("Failed to allocate memory!\n", NULL);
        return NULL;
    }

    memcpy(aux, &count, sizeof(uint16_t)); // Copy the number of members
    aux += sizeof(uint16_t);

    m = members;
    while (m != NULL) {
        size_t s = serialize_member(m->content, &aux); // Copy the member
        aux += s;

        m = m->next;
    }

    return message;

}

uint16_t member_get_state(member* m, uint16_t state) {

    return m->state & state;

}
uint16_t member_set_state(member* m, uint16_t state) {

    sem_wait(m->editable);

    uint16_t result = state | m->state;
    m->state = result;

    sem_post(m->editable);

    return result;

}
uint16_t member_unset_state(member* m, uint16_t state) {

    sem_wait(m->editable);

    uint16_t result = ~state & m->state;
    m->state = result;

    sem_post(m->editable);

    return result;

}
