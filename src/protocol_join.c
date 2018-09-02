#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "protocol_join.h"
#include "nfs_ops.h"
#include "members.h"
#include "output.h"
#include "server.h"
#include "fops.h"

// Send Join Request
int send_join_req(char* ip, uint16_t port, member* m) {

    char protocol[] = "jreq";
    size_t size = size_of_member(m);
    size_t protocol_size = strlen(protocol);

    char* message = (char*) malloc(size + protocol_size);
    memcpy(message, protocol, protocol_size);

    char* aux = message + protocol_size;
    serialize_member(m, &aux);

    return server_send(ip, port, message, size + protocol_size);
}

// Send Join Reply
int send_join_rep(char* ip, uint16_t port, char* id, size_t id_size, member* m) {

    char protocol[] = "jrep";
    size_t size = size_of_member(m);
    size_t protocol_size = strlen(protocol);

    size += sizeof(uint16_t);
    size += protocol_size;
    size += id_size;

    char* message = (char *) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, protocol_size); // Copy Protocol
    aux += protocol_size;

    memcpy(aux, &id_size, sizeof(uint16_t)); // Copy ID size
    aux += sizeof(uint16_t);

    memcpy(aux, id, id_size); // Copy ID
    aux += id_size;

    serialize_member(m, &aux); // Serialize member

    return server_send(ip, port, message, size);
}

// Send Join Ack
int send_join_ack(char* ip, uint16_t port, char* id, size_t id_size) {

    char protocol[] = "jack";
    size_t protocol_size = strlen(protocol);

    size_t size = protocol_size + sizeof(uint16_t) + id_size;

    char* message = (char *) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, protocol_size);
    aux += protocol_size;

    memcpy(aux, &id_size, sizeof(uint16_t));
    aux += sizeof(uint16_t);

    memcpy(aux, id, id_size);
    // aux += id_size;

    return server_send(ip, port, message, size);
}


// Handle Join Request
static void clean_handle_join_req(int depth, char *path, char *ip, char *id) {
    switch (depth) {
        case 2:
            if (fops_remove_dir(id)) error("Failed to remove directory for member '%s'\n", id);
        case 1:
            if (remove_nfs_recp(path, ip)) error("Failed to clean up NFS exports!\n", NULL);
            remove_member(id);
        default:
            break;
    }
}
void handle_join_req(char *message) {

    // Parse member in the message
    member m;
    if (deserialize_member(message, &m)) {
        error("Invalid member format in join protocol!\n", NULL);
        clean_handle_join_req(0, NULL, NULL, NULL);
        return;
    }

    // Add it to NFS permissions
    member* current = get_current_member();
    if (add_nfs_recp(current, m.ip)) {
        error("Failed to add ip as NFS recipient!\n", NULL);
        clean_handle_join_req(0, NULL, NULL, NULL);
        return;
    }

    // Assign an ID for it
    char* id = generate_member_id();
    size_t id_size = strlen(id);

    free(m.id);
    m.id = id;
    m.id_size = (uint16_t) id_size;
    add_member(&m);

    // Create folder for it
    if (fops_make_dir(m.id)) {
        error("Failed to create folder for member '%s'\n", m.id);
        clean_handle_join_req(1, current->prefix, m.ip, m.id);
        return;
    }

    // Reply the join
    if (send_join_rep(m.ip, m.port, id, id_size, current)) {
        error("Failed to send join reply!\n", NULL);
        clean_handle_join_req(2, current->prefix, m.ip, m.id);
        return;
    }
}

// Handle Join Reply
static void clean_handle_join_rep(int depth, char *path, char *recp, char *id, char *id2) {
    switch (depth) {
        case 5:
            if (remove_nfs_recp(path, recp)) error("Failed to remove recipient!\n", NULL);
        case 4:
            //if (unmount_nfs_dir(m)) error("Failed to unmount NFS dir!\n", NULL);
            // Unmount NFS dir
        case 3:
            fops_remove_dir(id2);
        case 2:
            remove_member(id2);
        case 1:
            fops_remove_dir(id);
        default:
            break;
    }
}
void handle_join_rep(char *message) {

    // Read the ID size
    uint16_t size;
    memcpy(&size, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    // Read the ID
    char *id = (char *) malloc(size + 1);
    memcpy(id, message, size);
    id[size] = '\0';
    message += size;

    // Assign ID to self
    member* current = get_current_member();
    current->id_size = size;
    current->id = id;

    printf("My id is: %s\n", id);

    // Create folder for itself
    if (fops_make_dir(id)) {
        error("Failed to create folder to self!\n", NULL);
        printf("%d\n", errno);
        return;
    }
    printf("Created folder '%s'\n", id);

    // Read the member replied
    member m;
    if (deserialize_member(message, &m)) {
        error("Failed to parse member from input!\n", NULL);
        clean_handle_join_rep(1, NULL, NULL, id, m.id);
        return;
    }
    add_member(&m);

    printf("%s %s %d %s\n", m.id, m.ip, m.port, m.prefix);

    // Create folder for other member
    if (fops_make_dir(m.id)) {
        error("Failed to create folder for '%s'\n", m.id);
        clean_handle_join_rep(2, NULL, NULL, id, m.id);
        return;
    }

    // Mount NFS
    if (mount_nfs_dir(&m)) {
        error("Failed to mount NFS folder!\n", NULL);
        clean_handle_join_rep(3, NULL, NULL, id, m.id);
        return;
    }

    // Add NFS recipient
    if (add_nfs_recp(current, m.ip)) {
        error("Failed to add NFS recipient!\n", NULL);
        clean_handle_join_rep(4, current->prefix, m.ip, id, m.id);
        return;
    }

    // Send acknowledgement
    if (send_join_ack(m.ip, m.port, id, size)) {
        error("Failed to send join acknowledgement!\n", NULL);
        clean_handle_join_rep(5, current->prefix, m.ip, id, m.id);
        return;
    }

    members_for_each(print_member);
}

// Handle Join Acknowledge
void handle_join_ack(char *message) {

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

    members_for_each(print_member);
}
