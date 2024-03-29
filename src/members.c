#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <nops.h>
#include <sys/types.h>
#include <background.h>

#include "semaphores.h"
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
static int count = 0;
static member* self = NULL;
static mlist* members = NULL;
static uint16_t child_count = 0;

// Version Clock
static uint16_t member_clock = 0;
static semaphore* member_clock_sem = NULL;

uint16_t get_member_clock() {

    return member_clock;

}
uint16_t inc_member_clock() {

    if (member_clock_sem == NULL) {
        member_clock_sem = (semaphore *) malloc(sizeof(semaphore));
        portable_sem_init(member_clock_sem, 1);
    }

    portable_sem_wait(member_clock_sem);
    ++member_clock;
    portable_sem_post(member_clock_sem);

    return member_clock;

}

// Methods
static member* copy_member(member* m) {

    member* result = (member *) malloc(sizeof(member));
    memcpy(result, m, sizeof(member));

    result->id = (char *) malloc(result->id_size + 1);
    result->prefix = (char *) malloc(result->prefix_size + 1);

    strncpy(result->id, m->id, result->id_size + 1);
    strncpy(result->prefix, m->prefix, result->prefix_size + 1);

    return result;
}

int initialize_metadata_members(member *m) {

    count++;
    self = m;
    m->state = (uint16_t)(m->state | AVAIL);

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
            stop_background(m->content);
            inc_member_clock();
            _free_member(m);
            --count;
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

    ++count;
    start_background(n);
    inc_member_clock();

}

member* get_random_member() {

    long m;
    mlist* aux;

    do {
        aux = members;
        m = random() % count;
        if (m-- == 0) return self;
        while (aux != NULL && m--) aux = aux->next;
    }
    while (aux == NULL);

    return aux->content;
}

member* get_current_member() {

    return self;

}

member* get_certain_member(char* id) {

    if (id == NULL) return NULL;
    if (!strcmp(id, self->id)) return get_current_member();

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

    return 0;

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
    printf("Serialize ID Size: %d\n", param->id_size);

    memcpy(aux, param->id, param->id_size); // Copy the ID
    aux += param->id_size;
    printf("Serialize ID: %s\n", param->id);

    memcpy(aux, param->ip, 15 * sizeof(char)); // Copy the IP
    aux += 15 * sizeof(char);
    printf("Serialize IP: %s\n", param->ip);

    memcpy(aux, &(param->port), sizeof(uint16_t)); // Copy the Port
    aux += sizeof(uint16_t);
    printf("Serialize Port: %d\n", param->port);

    memcpy(aux, &(param->prefix_size), sizeof(uint16_t)); // Copy the prefix size
    aux += sizeof(uint16_t);

    memcpy(aux, param->prefix, param->prefix_size); // Copy the prefix
    //aux += param->prefix_size;
    printf("Serialize Prefix: %s\n", param->prefix);

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
    portable_sem_init(&container->editable, 1);

    container->member_clock = 0;
    container->lhier_clock = 0;

    return 0;
}

member* build_member(char* id, char* ip, uint16_t port, char* prefix) {

    member* result = (member *) malloc(sizeof(member));

    strncpy(result->ip, ip, 16);
    result->port = port;

    result->id_size = (uint16_t)(strlen(id) * sizeof(char));
    result->id = (char *) malloc(result->id_size + sizeof(char));
    strcpy(result->id, id);

    result->prefix_size = (uint16_t)(strlen(prefix) * sizeof(char));
    result->prefix = (char *) malloc(result->prefix_size + sizeof(char));
    strcpy(result->prefix, prefix);

    result->avail = 0;
    result->state = 0;
    portable_sem_init(&result->editable, 1);

    result->member_clock = 0;
    result->lhier_clock = 0;

    return result;

}

char* build_members_message(size_t prefix_size, char* prefix, size_t* size) {

    uint16_t count = 0;
    *size = prefix_size;
    *size += sizeof(uint16_t);
    *size += sizeof(uint16_t);

    mlist* m = members;
    while (m != NULL) {
        *size += size_of_member(m->content); // Count the size of the message
        m = m->next;
        ++count; // Count the number of members
    }

    char* message = (char *) malloc(prefix_size + *size);
    char* aux = message;

    if (message == NULL) {
        error("Failed to allocate memory!\n", NULL);
        return NULL;
    }

    memcpy(aux, prefix, prefix_size); // Copy Prefix
    aux += prefix_size;

    memcpy(aux, &member_clock, sizeof(uint16_t)); // Copy the member clock
    aux += sizeof(uint16_t);

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

    portable_sem_wait(&m->editable);

    uint16_t result = state | m->state;
    m->state = result;

    portable_sem_post(&m->editable);

    return result;

}
uint16_t member_unset_state(member* m, uint16_t state) {

    portable_sem_wait(&m->editable);

    uint16_t result = ~state & m->state;
    m->state = result;

    portable_sem_post(&m->editable);

    return result;

}
