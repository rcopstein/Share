#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <pthread.h>
#include <semaphore.h>
#include <nops.h>
#include <zconf.h>

#include "members.h"
#include "output.h"
#include "server.h"

// Member struct
typedef struct __member {

    member *content;
    struct __member *next;

    int stop;
    uint16_t state;
    sem_t* editable;
    int availability;
    int connection_socket;
    pthread_t detector_thread;

} _member;

// Variables
static const char filepath[] = "metadata/members.txt";

static member* self = NULL;
static _member* members = NULL;
static uint16_t child_count = 0;

// MOVE THIS SOMEWHERE ELSE!
void* _activity_detector(void* member) {

    _member* m = (_member *) member;

    for (int i = 0; !m->stop; ++i) {

        if (m->availability > 4) {
            printf("%s is down!\n", m->content->id);
        }

        printf("%s From Another Thread!\n", m->content->id);

        sem_wait(m->editable);
        server_send(m->content->ip, m->content->port, "ping", 4);
        m->availability += 1;
        sem_post(m->editable);

        sleep(10);
    }

    printf("'%s' finished successfully!\n", m->content->id);
    return NULL;

}

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

    self = (member *) malloc(sizeof(member));
    memcpy(self, m, sizeof(member));

    members_for_each(print_member);
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

    free(m->prefix);
    free(m->id);
    free(m);

}
static void _free_member(_member *m) {

    sem_wait(m->editable);

    m->stop = 1;
    nops_close_connection(m->connection_socket);
    pthread_join(m->detector_thread, NULL);

    sem_unlink(m->content->id);
    free_member(m->content);
    free(m);

}

// Load members from file
int member_load_from_file() {

    char buffer[256];
    FILE* file = fopen(filepath, "r");
    if (file == NULL) return error("Failed to open file!\n", NULL);

    _member** head = &members;

    while (fgets(buffer, 255, file)) {

        member* m = (member*) malloc(sizeof(member));
        deserialize_member(buffer, m);

        _member* next = (_member*) malloc(sizeof(_member));
        next->content = m;
        next->next = NULL;

        *head = next;
        *head = next->next;
    }

    if (*head) (*head)->next = NULL;
    return 0;
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

    _member** aux = &members;

    while (*aux != NULL) {
        if (strcmp((*aux)->content->id, id) == 0) {
            _member* m = *aux;
            *aux = (*aux)->next;
            _free_member(m);
            return;
        }
        *aux = (*aux)->next;
    }

}

// Add a member to the members list
void add_member(member* memb) {

    _member* m = (_member *) malloc(sizeof(_member));
    member* n = (member *) malloc(sizeof(member));

    memcpy(n, memb, sizeof(member));
    m->content = n;

    m->stop = 0;
    m->state = 0;
    m->availability = 0;
    m->connection_socket = 0;
    m->editable = sem_open(n->id, O_CREAT, 0200, 1);
    pthread_create(&(m->detector_thread), NULL, _activity_detector, m);

    _member** head = &members;
    m->next = *head;
    *head = m;

}

// Return the current member
member* get_current_member() {

    return self;

}

// Return a member with a given id
member* get_certain_member(char* id) {

    _member* aux = members;

    while (aux != NULL) {
        if (strcmp(aux->content->id, id) == 0) return aux->content;
        aux = aux->next;
    }

    return NULL;
}

// Execute an operation for each member
void members_for_each(void (*funct)(member*)) {

    _member* aux = members;

    while (aux != NULL) {
        member* curr = aux->content;
        aux = aux->next;

        funct(curr);
    }

}
void _members_for_each(void (*funct)(_member*)) {

    _member* aux = members;

    while (aux != NULL) {
        _member* curr = aux;
        aux = aux->next;
        funct(curr);
    }

}

// Removes the metadata members file
int remove_metadata_members() {

    _members_for_each(_free_member);
    free_member(self);
    members = NULL;

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

    return result;

}

// Build a message with all the known members
char* build_members_message() {

    uint16_t count = 0;
    size_t size = sizeof(uint16_t);

    _member* m = members;
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

uint8_t member_get_state(member* m, uint16_t* state) {

    // Get internal member
    _member* internal = members;
    while (internal != NULL && strcmp(internal->content->id, m->id) != 0) internal = internal->next;
    if (internal == NULL) return 0;

    // Set the result
    *state = internal->state & *state;

    return 0;

}
uint8_t member_set_state(member* m, uint16_t* state) {

    // Get internal member
    _member* internal = members;
    while (internal != NULL && strcmp(internal->content->id, m->id) != 0) internal = internal->next;
    if (internal == NULL) return 0;

    // Set the value of state
    internal->state = *state = *state | internal->state;

    return 0;

}
uint8_t member_unset_state(member* m, uint16_t* state) {

    // Get internal member
    _member* internal = members;
    while (internal != NULL && strcmp(internal->content->id, m->id) != 0) internal = internal->next;
    if (internal == NULL) return 0;

    // Set the value of state
    internal->state = *state = *state ^ internal->state; // THIS IS WRONG!

    return 0;

}