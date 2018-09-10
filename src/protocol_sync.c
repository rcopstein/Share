#include <memory.h>
#include <stdlib.h>

#include "protocol_sync.h"
#include "members.h"
#include "output.h"
#include "server.h"

static const char protocol[] = "sync";
static const uint8_t protocol_size = 4;

// MEMBERS
static int sync_memb(char* source, member* memb) {

    // Try to find member in registries
    member* other = get_certain_member(memb->id);

    // If couldn't find it, add it
    if (other == NULL) {
        printf("Added Member '%s'\n", memb->id);
        add_member(memb);
        return 1;
    }

    // Compare to see if it changed
    int flag = 0;
    printf("Compared Member '%s'\n", memb->id);
    return flag;

}

static int send_memb_sync_rep(char* ip, uint16_t port) {

    uint8_t rep = 1;
    uint8_t type = SYNC_MEMB;
    member* current = get_current_member();

    size_t size = protocol_size;
    size += current->id_size;
    size += sizeof(uint16_t);
    size += sizeof(uint8_t);
    size += sizeof(uint8_t);

    char* message = (char *) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, protocol_size); // Copy Protocol
    aux += protocol_size;

    memcpy(aux, &type, sizeof(uint8_t)); // Copy Type
    aux += sizeof(uint8_t);

    memcpy(aux, &rep, sizeof(uint8_t)); // Copy Packet Type
    aux += sizeof(uint8_t);

    memcpy(aux, &current->id_size, sizeof(uint16_t)); // Copy ID Size
    aux += sizeof(uint16_t);

    memcpy(aux, current->id, current->id_size); // Copy ID
    //aux += current->id_size;

    char* message_complete = build_members_message(size, message, &size); // Append list of members
    free(message);

    return server_send(ip, port, message_complete, size);

}
static int send_memb_sync_req(char* ip, uint16_t port) {

    uint8_t req = 0;
    uint8_t type = SYNC_MEMB;
    member* current = get_current_member();

    size_t size = protocol_size;
    size += current->id_size;
    size += sizeof(uint16_t);
    size += sizeof(uint8_t);
    size += sizeof(uint8_t);

    char* message = (char *) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, protocol_size); // Copy Protocol
    aux += protocol_size;

    memcpy(aux, &type, sizeof(uint8_t)); // Copy Type
    aux += sizeof(uint8_t);

    memcpy(aux, &req, sizeof(uint8_t)); // Copy Packet Type
    aux += sizeof(uint8_t);

    memcpy(aux, &current->id_size, sizeof(uint16_t)); // Copy ID Size
    aux += sizeof(uint16_t);

    memcpy(aux, current->id, current->id_size); // Copy ID
    // aux += current->id_size;

    return server_send(ip, port, message, size);

}

static void handle_sync_memb_rep(char* message) {

    printf("Received reply message %s\n", message);

    // Read ID Size
    uint16_t size;
    memcpy(&size, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    // Read ID
    char* id = (char *) malloc(sizeof(char) * (size + 1));
    memcpy(id, message, size * sizeof(char));
    id[size * sizeof(char)] = '\0';

    // Find member
    member* memb = get_certain_member(id);
    if (memb == NULL) {
        free(id);
        return;
    }

    // Read Member clock
    uint16_t clock;
    memcpy(&clock, message, sizeof(uint16_t));
    message += sizeof(uint16_t);
    memb->member_clock = clock;

    printf("%s's clock is now %d\n", id, clock);

    // Read Members Number
    uint16_t number;
    memcpy(&number, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    printf("%s's number of members is %d\n", id, number);
    return;

    // Read All Members
    int flag = 0;
    int msize = 0;
    member* container = (member *) malloc(sizeof(member));
    for (int i = 0; i < number; ++i) {

        msize = deserialize_member(message, container);
        message += msize;

        printf("Received member %s\n", container->id);

        //if (sync_memb(id, container)) flag = 1;

    }

    if (flag) inc_member_clock();
    free(container);
    free(id);

}
static void handle_sync_memb_req(char* message) {

    printf("Received request message %s\n", message);

    // Read ID Size
    uint16_t size;
    memcpy(&size, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    // Read ID
    char* id = (char *) malloc(sizeof(char) * (size + 1));
    memcpy(id, message, size * sizeof(uint16_t));
    // message += size * sizeof(char);
    id[size * sizeof(char)] = '\0';

     printf("Sync Request from '%s' with size %d\n", id, size);

    // Find Member with that ID
    member* memb = get_certain_member(id);
    if (memb == NULL) {
        printf("Failed to find member with id '%s'\n", id);
        free(id);
        return;
    }

    // Send a reply
    send_sync_rep(memb->ip, memb->port, SYNC_MEMB);

}

int send_sync_req(char* ip, uint16_t port, uint8_t type) {

    if (type == SYNC_MEMB) return send_memb_sync_req(ip, port);
    return 1;

}
int send_sync_rep(char* ip, uint16_t port, uint8_t type) {

    if (type == SYNC_MEMB) return send_memb_sync_rep(ip, port);
    return 1;

}

void handle_sync_protocol(char* message) {

    uint8_t type;
    uint8_t packet_type;

    // Read Type
    memcpy(&type, message, sizeof(uint8_t)); // Either MEMB or LHIE
    message += sizeof(uint8_t);

    // Read Packet Type
    memcpy(&packet_type, message, sizeof(uint8_t)); // Either 0 (Req) or 1 (Rep)
    message += sizeof(uint8_t);

    // Send to proper place
    if (type == SYNC_MEMB) {
        if (packet_type == 0) handle_sync_memb_req(message); // Request
        if (packet_type == 1) handle_sync_memb_rep(message); // Reply
    }

}
