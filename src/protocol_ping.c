#include <stdlib.h>
#include <string.h>

#include "protocol_ping.h"
#include "output.h"
#include "server.h"

static const char protocol[] = "ping";
static const uint8_t protocol_size = 4;

int send_ping(member* m) {

    // Get Current Member
    member* current = get_current_member();

    // Calculate Message Size
    size_t size = current->id_size;
    size += sizeof(uint16_t);
    size += protocol_size;

    // Allocate Message
    char* message = (char *) malloc(size);
    char* aux = message;

    memcpy(aux, protocol, protocol_size); // Copy Protocol
    aux += protocol_size;

    memcpy(aux, &(current->id_size), sizeof(uint16_t)); // Copy ID Size
    aux += sizeof(uint16_t);

    memcpy(aux, current->id, current->id_size); // Copy ID
    //aux += current->id_size;

    // Send Message
    return server_send(m->ip, m->port, message, size);

}

void handle_ping_protocol(char* message) {

    // Read ID Size
    uint16_t size;
    memcpy(&size, message, sizeof(uint16_t));
    message += sizeof(uint16_t);

    // Read Member ID
    char* id = (char *) malloc(size + 1);
    memcpy(id, message, size);
    id[size] = '\0';

    printf("Read size %d and ID %s\n", size, id);

    // Find Member with ID
    member* m = get_certain_member(id);
    if (m == NULL) {
        warning("Failed to find member with ID %s\n", id);
        free(id);
        return;
    }

    // Update Member State
    m->avail = 0;
    free(id);

}
