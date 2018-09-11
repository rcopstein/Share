#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <members.h>
#include <errno.h>

#include "protocol_name.h"
#include "nfs_ops.h"
#include "output.h"
#include "server.h"
#include "fops.h"

static const uint8_t TYPE_REQ = 0;
static const uint8_t TYPE_REP = 1;

static const char protocol[] = "name";
static const uint8_t protocol_size = 4;

int send_name_req(char *ip, uint16_t port, member *m) {

    size_t size = size_of_member(m);
    size += sizeof(uint8_t);
    size += protocol_size;

    char* message = (char*) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, protocol_size); // Copy protocol
    aux += protocol_size;

    memcpy(aux, &TYPE_REQ, sizeof(uint8_t)); // Copy protocol type
    aux += sizeof(uint8_t);

    serialize_member(m, &aux); // Copy member

    printf("# Sent Name Request to %s:%d\n", ip, port);
    return server_send(ip, port, message, size);

}
int send_name_rep(char *ip, uint16_t port, char *id, size_t id_size, member *m) {

    size_t size = size_of_member(m);
    size_t protocol_size = strlen(protocol);

    size += sizeof(uint16_t);
    size += sizeof(uint8_t);
    size += protocol_size;
    size += id_size;

    char* message = (char *) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, protocol_size); // Copy Protocol
    aux += protocol_size;

    memcpy(aux, &TYPE_REP, sizeof(uint8_t)); // Copy protocol type
    aux += sizeof(uint8_t);

    memcpy(aux, &id_size, sizeof(uint16_t)); // Copy ID size
    aux += sizeof(uint16_t);

    memcpy(aux, id, id_size); // Copy ID
    aux += id_size;

    serialize_member(m, &aux); // Serialize member

    printf("# Sent Name Reply to %s:%d\n", ip, port);
    return server_send(ip, port, message, size);

}

void handle_name_req(char *message) {

    member* current = get_current_member();

    // Attempt to read the member
    member m;
    if (deserialize_member(message, &m)) {
        error("Failed to read member from name request message!\n", NULL);
        return;
    }

    // Generate an ID for member
    char* id = generate_member_id();
    size_t id_size = strlen(id);

    free(m.id);
    m.id = id;
    m.id_size = (uint16_t) id_size;
    add_member(&m);

    printf("# Received Name Request from %s\n", m.id);

    // Reply the request
    if (send_name_rep(m.ip, m.port, id, id_size, current)) {
        error("Failed to send join reply!\n", NULL);
        fops_remove_dir(id);
        remove_member(m.id);
        free_member(&m);
        return;
    }

}
void handle_name_rep(char *message) {

    // Read the ID size
    uint16_t size;
    memcpy(&size, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    // Read the ID
    char *id = (char *) malloc(size + 1);
    memcpy(id, message, size);
    id[size] = '\0';
    message += size;

    printf("# Received Name Reply from %s\n", id);

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
        fops_remove_dir(m.id);
        return;
    }
    add_member(&m);

    // Set state to active
    uint16_t flags = AVAIL;
    member_set_state(current, flags);

}

void handle_name_protocol(char *message) {

    // Read Protocol Type
    uint8_t type;
    memcpy(&type, message, sizeof(uint8_t));
    message += sizeof(uint8_t);

    // Check Type
    if (type == TYPE_REQ) handle_name_req(message);
    else if (type == TYPE_REP) handle_name_rep(message);
    else warning("Discarding '%s' protocol with invalid type!\n", (char *) protocol);

}
