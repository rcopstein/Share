#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol_mont.h"
#include "nfs_ops.h"
#include "server.h"
#include "output.h"
#include "fops.h"

static const uint8_t TYPE_REQ = 0;
static const uint8_t TYPE_REP = 1;

static const char protocol[] = "mont";
static const uint8_t protocol_size = 4;

int send_mont_req(char *ip, uint16_t port, member *m) {

    // Get current member state
    uint16_t state = member_get_state(m, RECP);

    // Check if member is a recipient
    if (!state) {

        // Add NFS recipient
        if (add_nfs_recp(get_current_member(), m->ip)) {
            return warning("Failed to add ", NULL);
        }

        member_set_state(m, RECP);

    }

    // Calculate size for message
    size_t size = protocol_size;
    size += size_of_member(m);
    size += sizeof(uint8_t);

    // Allocate size for message
    char* message = (char *) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, protocol_size); // Copy Protocol
    aux += protocol_size;

    memcpy(aux, &TYPE_REQ, sizeof(uint8_t)); // Copy Protocol Type
    aux += sizeof(uint8_t);

    serialize_member(m, &aux); // Serialize member

    return server_send(ip, port, message, size);

}
int send_mont_rep(char *ip, uint16_t port, member *m) {

    // Calculate size for message
    size_t size = protocol_size;
    size += size_of_member(m);
    size += sizeof(uint8_t);

    // Allocate size for message
    char* message = (char *) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, protocol_size); // Copy Protocol
    aux += protocol_size;

    memcpy(aux, &TYPE_REP, sizeof(uint8_t)); // Copy Protocol Type
    aux += sizeof(uint8_t);

    serialize_member(m, &aux); // Serialize member

    return server_send(ip, port, message, size);

}

void handle_mont_req(char *message) {

    // Read member
    member m;
    if (deserialize_member(message, &m)) {
        warning("Failed to read member from mount req message!\n", NULL);
        return;
    }

    // Get current member state
    uint16_t state = member_get_state(&m, MOUNT);

    // Check if member has been mounted
    if (!state) {

        // Attempt to create folder
        if (fops_make_dir(m.id)) {
            warning("Failed to create folder for %s\n", m.id);
            return;
        }

        // Attempt to mount NFS
        if (mount_nfs_dir(&m)) {
            warning("Failed to mount NFS for %s\n", m.id);
            fops_remove_dir(m.id);
            return;
        }

        member_set_state(&m, MOUNT);

    }

    // Get current member state
    state = member_get_state(&m, RECP);

    // Check if member is a recipient
    if (!state) {

        // Add NFS recipient
        if (add_nfs_recp(get_current_member(), m.ip)) {
            warning("Failed to add ", NULL);
            return;
        }

        member_set_state(&m, RECP);

    }

    send_mont_rep(m.ip, m.port, get_current_member());

}
void handle_mont_rep(char *message) {

    // Read member
    member m;
    if (deserialize_member(message, &m)) {
        warning("Failed to read member from mount req message!\n", NULL);
        return;
    }

    // Get current member state
    uint16_t state = member_get_state(&m, MOUNT);

    // Check if member has been mounted
    if (!state) {

        // Attempt to create folder
        if (fops_make_dir(m.id)) {
            warning("Failed to create folder for %s\n", m.id);
            return;
        }

        // Attempt to mount NFS
        if (mount_nfs_dir(&m)) {
            warning("Failed to mount NFS for %s\n", m.id);
            fops_remove_dir(m.id);
            return;
        }

        member_set_state(&m, MOUNT);

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
