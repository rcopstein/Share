#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol_join.h"
#include "nfs_ops.h"
#include "members.h"
#include "output.h"
#include "server.h"

// Handle Join Request
static void clean_protocol_join_req(int depth, char *id) {
    switch (depth) {
        case 1:
            free(id);
            break;
        default:
            break;
    }
}
void protocol_join_req(char *message) {

    // Parse member in the message
    member m;
    if (deserialize_member(message, &m)) {
        error("Invalid member format in join protocol!\n", NULL);
        clean_protocol_join_req(0, NULL);
        return;
    }

    // Add it to NFS permissions
    if (add_nfs_recp(members->prefix, m.ip)) {
        error("Failed to add ip as NFS recipient!\n", NULL);
        clean_protocol_join_req(0, NULL);
        return;
    }

    // Assign an ID for it
    char* id = generate_member_id();
    size_t id_size = strlen(id);

    // Create message with data
    uint16_t size = members->id_size + members->prefix_size + sizeof(member);
    size += id_size + sizeof(uint16_t);

    char* reply = (char *) malloc(size);
    memcpy(reply, &id_size, sizeof(uint16_t));
    memcpy(reply + sizeof(uint16_t), id, id_size);

    char* aux = reply + id_size + sizeof(uint16_t);
    serialize_member(members, &(aux));

    // Reply the join
    /*if (server_send(m.ip, m.port, message, size)) {
        error("Failed to reply to message!\n", NULL);
        return;
    }*/

}

// Handle Join Reply
static void clean_protocol_join_rep(int depth) {

}
void protocol_join_rep(char *message) {

    // Read the ID size
    uint16_t size;
    memcpy(&size, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    // Read the ID
    char *id = (char *) malloc(size);
    memcpy(message, id, size);
    message += size;

    // Read the member replied
    member m;
    if (deserialize_member(message, &m)) {
        error("Failed to parse member from input!\n", NULL);
        free(id);
        return;
    }

    // Mount NFS
    if (mount_nfs_dir(&m)) {
        error("Failed to mount NFS folder!\n", NULL);
        free(id);
        return;
    }

    // Add NFS recipient
    if (add_nfs_recp(members->prefix, m.ip)) {
        error("Failed to add NFS recipient!\n", NULL);
        free(id);
        return;
    }

    // Send acknowledgement
    char ack[19];
    memcpy(ack, "jack", 4);
    memcpy(ack + 4, m.ip, 15);

    if (server_send(m.ip, m.port, ack, 19)) {
        error("Failed to send acknowledge!\n", NULL);
        free(id);
        return;
    }

}

// Handle Join Acknowledge
static void clean_protocol_join_ack(int depth) {

}
void protocol_join_ack(char *message) {

    // Read ID size
    uint16_t size;
    memcpy(&size, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    // Read ID
    char* id = (char *) malloc(size);
    memcpy(id, message, size);

    // Find member with that ID

    // Mount member NFS dir

}