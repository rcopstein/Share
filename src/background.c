#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <protocol_mont.h>
#include <zconf.h>
#include <stdbool.h>

#include "protocol_ping.h"
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
static bool signal_on = false;

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
static void check_connection(member* m) {

    // Check if Disconnected
    if (m->state & AVAIL && m->avail >= 4) {
        printf("%s Disconnected!\n", m->id);
        member_unset_state(m, AVAIL);
    }

    // Check Reconnection
    if (!(m->state & AVAIL) && m->avail < 4) {
        member_set_state(m, AVAIL);
        printf("%s Reconnected!\n", m->id);
    }

    // Update avail
    m->avail = (uint8_t) fmin(m->avail + 1, 4);

    // Send Ping
    send_ping(m);

}
static void check_mount(member* m) {

    if (!(m->state & MOUNT) || !(m->state & RECP)) send_mont_req(m->ip, m->port, m);
    printf("Checked MOUNT for %s\n", m->id);

}
static void* loop(void* _bg) {

    bg* item = (bg*) _bg;
    member* m = item->owner;

    while (!item->stop) {

        check_connection(m);
        //check_mount(m);

        sleep(10);

    }

    printf("Stopped %s background!\n", m->id);
    return NULL;

}

// Background Management Methods
void stop_wait_all() {

    bg* aux = list;
    while (aux != NULL) {
        aux->stop = 1;
        pthread_join(aux->thread, NULL);
    }

}
void stop_background(member* m) {

    bg* item = get_bg(m->id);
    item->stop = 1;

}
void wait_background(member* m) {

    bg* item = get_bg(m->id);
    pthread_join(item->thread, NULL);

}
void start_background(member* m) {

    bg* item = (bg *) malloc(sizeof(bg));
    item->next = NULL;
    item->owner = m;
    item->stop = 0;

    printf("Started background for %s\n", m->id);

    pthread_create(&(item->thread), NULL, loop, item);
    add_bg(item);

}
