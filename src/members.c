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


// Initializes members file and information
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
    self->state = 0;
    self->editable = sem_open(m->id, O_CREAT, 0200, 1);

    return 0;
}

// Prints a member to stdout
void print_member(member* m) {

    printf("ID: %s\n", m->id);
    printf("Prefix: %s\n", m->prefix);
    printf("IP: %s\n", m->ip);
    printf("Port: %d\n", m->port);
    printf("\n");

}

// Return the size of the contents of a member (without string terminators)
size_t size_of_member(member* m) {

    return 3 * sizeof(uint16_t) + 15 + m->prefix_size + m->id_size;

}

// Frees a member from memory
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

// Generate a new unique ID following the pattern
char* generate_member_id() {

    ++child_count;
    // TO-DO Serialize in the members file

    member *current = get_current_member();
    char* id = malloc(sizeof(char) * (current->id_size + 6));
    sprintf(id, "%s-%d", current->id, child_count);

    return id;

}

// Remove a member from the members list
void remove_member(char* id) {

    mlist** aux = &members;

    while (*aux != NULL) {
        if (strcmp((*aux)->content->id, id) == 0) {
            mlist* m = *aux;
            *aux = (*aux)->next;
            _free_member(m);
            return;
        }
        *aux = (*aux)->next;
    }

}

// Add a member to the members list
void add_member(member* memb) {

    mlist* m = (mlist *) malloc(sizeof(mlist));
    m->content = memb;

    mlist** head = &members;
    m->next = *head;
    *head = m;

    start_background(memb);

}

// Return the current member
member* get_current_member() {

    return self;

}

// Return a member with a given id
member* get_certain_member(char* id) {

    mlist* aux = members;

    while (aux != NULL) {
        if (strcmp(aux->content->id, id) == 0) return aux->content;
        aux = aux->next;
    }

    return NULL;
}

// Execute an operation for each member
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

// Removes the metadata members file
int remove_metadata_members() {

    printf("1");
    _members_for_each(_free_member);

    printf("2");
    free_member(self);

    printf("3");
    members = NULL;

    printf("4");
    return remove(filepath);

}

// Print a member to a string (assume buffer is big enough)
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

// Read a line and build a 'member' struct
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

    return 0;
}

// Build a member struct based on the parameters (Assume ip is decently sized)
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

    result->state = 0;
    result->editable = sem_open(id, O_CREAT, 0200, 1);

    return result;

}

// Build a message with all the known members
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

// Manage Member State
uint16_t member_get_state(member* m, uint16_t state) {

    return m->state & state;

}
uint16_t member_set_state(member* m, uint16_t state) {

    sem_wait(m->editable);
    uint16_t result = state | m->state;
    sem_post(m->editable);

    return result;

}
uint16_t member_unset_state(member* m, uint16_t state) {

    sem_wait(m->editable);
    uint16_t result = ~state & m->state;
    sem_post(m->editable);

    return result;

}
