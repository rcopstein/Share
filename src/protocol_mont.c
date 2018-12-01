#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "protocol_mont.h"
#include "system.h"
#include "nfs_ops.h"
#include "server.h"
#include "output.h"
#include "fops.h"

static const uint8_t TYPE_REQ = 0;
static const uint8_t TYPE_REP = 1;

static const char protocol[] = "mont";
static const uint8_t protocol_size = 4;

static int send_mont(member* m, uint8_t type) {

    // Get Current Member
    member* current = get_current_member();

    // Calculate size for message
    size_t size = current->id_size;
    size += sizeof(uint16_t);
    size += sizeof(uint8_t);
    size += protocol_size;

    // Allocate size for message
    char* message = (char *) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, sizeof(char) * protocol_size); // Copy Protocol
    aux += sizeof(char) * protocol_size;

    memcpy(aux, &type, sizeof(uint8_t)); // Copy Protocol Type
    aux += sizeof(uint8_t);

    memcpy(aux, &(current->id_size), sizeof(uint16_t)); // Copy ID Size
    aux += sizeof(uint16_t);

    memcpy(aux, current->id, sizeof(char) * current->id_size); // Copy ID
    // aux += get_current_member()->id_size;

    return server_send(m->ip, m->port, message, size);
}

int send_mont_req(member *m) {

    return send_mont(m, TYPE_REQ);

}
int send_mont_rep(member *m) {

    return send_mont(m, TYPE_REP);

}

static int read_memb(char** message, member** m) {

    // Read ID Size
    uint16_t size;
    memcpy(&size, *message, sizeof(uint16_t));
    *message += sizeof(uint16_t);

    // Allocate and read ID
    char* id = (char *) malloc(size + 1);
    memcpy(id, *message, size);
    *message += size;
    id[size] = '\0';

    // Find member
    *m = get_certain_member(id);
    free(id);

    // Return result
    if (*m == NULL) return 1;
    return 0;

}

void handle_mont_req(char *message) {

    member* m;
    read_memb(&message, &m);

    if (m == NULL) {
        warning("Failed to read or find member!\n", NULL);
        return;
    }

    // Check if member is a recipient
    if (!(m->state & RECP)) {

        // Add NFS recipient
        if (add_nfs_recp(get_current_member(), m->ip)) {
            warning("Failed to add NFS recipient '%s'\n", m->ip);
            remove_nfs_recp(get_current_member(), m->ip);
            return;
        }

        member_set_state(m, RECP);
        printf("ADDED RECIPIENT '%s'\n", m->ip);

    } else if (check_nfs_recp(get_current_member(), m->ip)) {

        warning("Found inconsistent NFS recipient state. Attempting to fix!\n", NULL);
        member_unset_state(m, RECP);
        return;
    }

    send_mont_rep(m);

}
void handle_mont_rep(char *message) {

    member* m;
    read_memb(&message, &m);

    if (m == NULL) {
        warning("Failed to read member!\n", NULL);
        return;
    }

    // Check if member has been mounted
    if (!(m->state & MOUNT)) {

        // Attempt to create folder
        int res = 0;

        if (!fops_exists_dir(m->id)) {
            become_user();
            res = fops_make_dir(m->id);
            become_root();
        }

        if (res) {
            warning("Failed to create folder for %s\n", m->id);
            return;
        }

        // Attempt to mount NFS
        if (mount_nfs_dir(m)) {
            warning("Failed to mount NFS for %s\n", m->id);
            fops_remove_dir(m->id);
            return;
        }

        member_set_state(m, MOUNT);

    }

}

void handle_mont_protocol(char *message) {

    // Read Protocol Type
    uint8_t type;
    memcpy(&type, message, sizeof(uint8_t));
    message += sizeof(uint8_t);

    // Check Type
    if (type == TYPE_REQ) handle_mont_req(message);
    else if (type == TYPE_REP) handle_mont_rep(message);
    else warning("Discarding '%s' protocol with invalid type!\n", (char *) protocol);

}
