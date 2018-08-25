#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "protocol_join.h"
#include "nfs_ops.h"
#include "members.h"
#include "output.h"
#include "server.h"

// Handle Join Request
static void clean_protocol_join_req(int depth, char* path, char* ip, char* id) {
    switch (depth) {
        case 1:
            if (remove_nfs_recp(path, ip) || update_nfs()) error("Failed to clean up NFS exports!\n", NULL);
            remove_member(id);
        default:
            break;
    }
}
void protocol_join_req(char *message) {

    // Parse member in the message
    member m;
    if (deserialize_member(message, &m)) {
        error("Invalid member format in join protocol!\n", NULL);
        clean_protocol_join_req(0, NULL, NULL, NULL);
        return;
    }

    // Add it to NFS permissions
    member* current = get_current_member();
    if (add_nfs_recp(current->prefix, m.ip) || update_nfs()) {
        error("Failed to add ip as NFS recipient!\n", NULL);
        clean_protocol_join_req(0, NULL, NULL, NULL);
        return;
    }

    // Assign an ID for it
    char* id = generate_member_id();
    size_t id_size = strlen(id);

    free(m.id);
    m.id = id;
    m.id_size = (uint16_t) id_size;
    add_member(&m);

    // Create message with data
    size_t size = size_of_member(current);
    size += id_size + sizeof(uint16_t) + 4 * sizeof(char);

    char* reply = (char *) malloc(size);
    memcpy(reply, "jrep", 4 * sizeof(char));
    memcpy(reply + 4, &id_size, sizeof(uint16_t));
    memcpy(reply + 4 + sizeof(uint16_t), id, id_size);

    char* aux = reply + 4 + id_size + sizeof(uint16_t);
    serialize_member(current, &aux);

    for (int i = 0; i < size; ++i) {
        if (isprint(reply[i])) printf("%c", reply[i]);
        else printf("'%u'", reply[i]);
    }
    printf("\n");

    // Reply the join
    if (server_send(m.ip, m.port, reply, size)) {
        error("Failed to reply to message!\n", NULL);
        clean_protocol_join_req(1, current->prefix, m.ip, m.id);
        return;
    }

}

// Handle Join Reply
static void clean_protocol_join_rep(int depth, char* path, char* recp, char* id) {
    switch (depth) {
        case 3:
            if (remove_nfs_recp(path, recp)) error("Failed to remove recipient!\n", NULL);
        case 2:
            // Unmount NFS dir
        case 1:
            remove_member(id);
        default:
            break;
    }
}
void protocol_join_rep(char *message) {

    // Read the ID size
    uint16_t size;
    memcpy(&size, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    // Read the ID
    char *id = (char *) malloc(size + 1);
    memcpy(id, message, size);
    id[size + 1] = '\0';
    message += size;

    // Assign ID to self
    member* current = get_current_member();
    current->id_size = size;
    current->id = id;

    // Read the member replied
    member m;
    if (deserialize_member(message, &m)) {
        error("Failed to parse member from input!\n", NULL);
        return;
    }
    add_member(&m);

    printf("%s %s %d %s\n", m.id, m.ip, m.port, m.prefix);

    // Mount NFS
    /*if (mount_nfs_dir(&m)) {
        error("Failed to mount NFS folder!\n", NULL);
        clean_protocol_join_rep(1, NULL, NULL, id);
        return;
    }

    // Add NFS recipient
    if (add_nfs_recp(current->prefix, m.ip)) {
        error("Failed to add NFS recipient!\n", NULL);
        clean_protocol_join_rep(2, current->prefix, m.ip, id);
        return;
    }*/

    // Send acknowledgement
    char ack[19 + sizeof(uint16_t)];
    memcpy(ack, "jack", 4);
    memcpy(ack + 4, &(m.id_size), sizeof(uint16_t));
    memcpy(ack + 4 + sizeof(uint16_t), m.id, 15);

    if (server_send(m.ip, m.port, ack, 19)) {
        error("Failed to send acknowledge!\n", NULL);
        clean_protocol_join_rep(3, current->prefix, m.ip, id);
        return;
    }
}

// Handle Join Acknowledge
void protocol_join_ack(char *message) {

    // Read ID size
    uint16_t size;
    memcpy(&size, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    // Read ID
    char* id = (char *) malloc(size);
    memcpy(id, message, size);

    // Find member with that ID
    member* m = get_certain_member(id);
    if (m == NULL) {
        error("Failed to find member with id '%s'\n", id);
        free(id);
        return;
    }

    // Mount member NFS dir
    if (mount_nfs_dir(m)) {
        error("Failed to mount NFS dir!\n", NULL);
        free(id);
        return;
    }

}
