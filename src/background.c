#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <protocol_mont.h>
#include <zconf.h>

#include "background.h"
#include "server.h"

// Define thread struct
typedef struct _bg {

    pthread_t thread;
    member* owner;
    uint8_t stop;

    struct _bg* next;

} bg;

// Define thread list
static bg* list = NULL;

// List Management Methods
static void add_bg(bg* item) {

    item->next = list;
    list = item;

}
static void rem_bg(char* item) {

    bg** aux = &list;

    while (*aux != NULL) {
        if (strcmp((*aux)->owner->id, item) == 0) {
            bg* rem = *aux;
            *aux = (*aux)->next;
            free(rem);
        }
        aux = &(*aux)->next;
    }

}

static bg* get_bg(char* item) {

    bg** aux = &list;

    while (*aux != NULL) {
        if (strcmp((*aux)->owner->id, item) == 0) return *aux;
        aux = &(*aux)->next;
    }

    return NULL;

}

// Looping Methods
static void check_connection(member* m, uint8_t* count) {

    // Check if Disconnected
    if (m->state & AVAIL && *count > 4) member_unset_state(m, AVAIL);

    // Check Reconnection
    if (m->state & AVAIL && *count > 4) *count = 0;

    // Update Count
    *count = (uint8_t) fmax(*count + 1, 5);

    // Send Ping
    char message[] = "ping";
    size_t message_size = strlen(message);
    server_send(m->ip, m->port, message, message_size);

    printf("Sent PING to %s\n", m->id);

}
static void check_mount(member* m) {

    if (!(m->state & MOUNT) || !(m->state & RECP)) send_mont_req(m->ip, m->port, m);
    printf("Checked MOUNT for %s\n", m->id);

}
static void* loop(void* _bg) {

    bg* item = (bg*) _bg;
    member* m = item->owner;

    uint8_t count = 0;

    while (!item->stop) {

        check_connection(m, &count);
        check_mount(m);

        sleep(10);

    }

    return NULL;

}

// Background Management Methods
void start_background(member* m) {

    bg* item = (bg *) malloc(sizeof(bg));
    item->next = NULL;
    item->owner = m;
    item->stop = 0;

    pthread_create(&(item->thread), NULL, loop, item);
    add_bg(item);

}
void stop_background(member* m) {

    bg* item = get_bg(m->id);
    item->stop = 1;

}
void wait_background(member* m) {

    bg* item = get_bg(m->id);
    pthread_join(item->thread, NULL);

}
void stop_wait_all() {

    bg* aux = list;
    while (aux != NULL) {
        aux->stop = 1;
        pthread_join(aux->thread, NULL);
    }

}
