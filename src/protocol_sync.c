#include <memory.h>
#include <stdlib.h>

#include "protocol_sync.h"
#include "members.h"
#include "output.h"
#include "server.h"

static const char protocol[] = "sync";
static const uint8_t protocol_size = 4;

static int send_memb_sync_rep(char* ip, uint16_t port) {

    uint8_t rep = 1;
    member* current = get_current_member();

    size_t size = protocol_size;
    size += current->id_size;
    size += sizeof(uint16_t);
    size += sizeof(uint8_t);

    char* message = (char *) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, protocol_size); // Copy Protocol
    aux += protocol_size;

    memcpy(aux, &rep, sizeof(uint8_t)); // Copy Type
    aux += sizeof(uint8_t);

    memcpy(aux, &current->id_size, sizeof(uint16_t)); // Copy ID Size
    aux += sizeof(uint16_t);

    memcpy(aux, current->id, current->id_size); // Copy ID
    //aux += current->id_size;

    char* message_complete = build_members_message(size, message, &size); // Append list of members
    free(message);

    return server_send(ip, port, message_complete, size);

}

int send_sync_req(char* ip, uint16_t port, uint8_t type) {

    uint8_t req = 0;
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

    memcpy(aux, &req, sizeof(uint8_t)); // Copy Request Int
    aux += sizeof(uint8_t);

    memcpy(aux, &current->id_size, sizeof(uint16_t)); // Copy ID Size
    aux += sizeof(uint16_t);

    memcpy(aux, current->id, current->id_size); // Copy ID
    // aux += current->id_size;

    return server_send(ip, port, message, size);

}
int send_sync_rep(char* ip, uint16_t port, uint8_t type) {

    if (type == SYNC_MEMB) { return send_memb_sync_rep(ip, port); }
    else return 1;

}

void handle_sync_protocol(char* message) {



}
