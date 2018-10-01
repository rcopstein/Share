#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>
#include <protocol_mont.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>

#include "protocol_ping.h"
#include "background.h"
#include "nfs_ops.h"
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
static void check_connection(member* m) {

    // Check if Disconnected
    if (m->state & AVAIL && m->avail >= 4) {
        printf("%s Disconnected!\n", m->id);
        member_unset_state(m, AVAIL);

        // Unmount Inactive Member
        if (!unmount_nfs_dir(m)) member_unset_state(m, MOUNT);
    }

    // Check Reconnection
    if (!(m->state & AVAIL) && m->avail < 4) {
        member_set_state(m, AVAIL);
        printf("%s Reconnected!\n", m->id);
    }

    // Update avail
    m->avail = (uint8_t)(m->avail >= 4 ? 4 : m->avail + 1);

    // Send Ping
    send_ping(m);

}
static void check_mount(member* m) {

    if (!(m->state & AVAIL)) return;
    if (!(m->state & MOUNT)) send_mont_req(m);

}
static void* loop(void* _bg) {

    bg* item = (bg*) _bg;
    member* m = item->owner;

    while (!item->stop) {

        check_connection(m);
        check_mount(m);
        sleep(2); // Change this to 10

    }

    // Unmount and remove recipient
    if (member_get_state(m, MOUNT)) {
        printf("# Umounting %s\n", m->id);
        unmount_nfs_dir(m);
    }

    if (member_get_state(m, RECP)) {
        printf("# Removing Recipient %s\n", m->id);
        remove_nfs_recp(get_current_member(), m->ip);
    }

    printf("Stopped %s background!\n", m->id);
    return NULL;

}

// Background Management Methods
void stop_wait_all_background() {

    bg* aux = list;
    while (aux != NULL) {
        aux->stop = 1;
        printf("Waiting for %s background\n", aux->owner->id);

        pthread_join(aux->thread, NULL);
        aux = aux->next;
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
