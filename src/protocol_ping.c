#include <stdlib.h>
#include <string.h>

#include "protocol_ping.h"
#include "protocol_sync.h"
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
    aux += current->id_size;

    uint16_t member_clock = get_member_clock(); // Copy Member Clock
    memcpy(aux, &member_clock, sizeof(uint16_t));
    // aux += sizeof(uint16_t);

    // Send Message
    printf("Ping to '%s'\n", m->id);
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
    message += size;

    // Find Member with ID
    member* m = get_certain_member(id);
    free(id);

    if (m == NULL) {
        warning("Failed to find member with ID %s\n", id);
        return;
    }

    // Update Member State
    m->avail = 0;

    // Read Member Clock
    uint16_t member_clock;
    memcpy(&member_clock, message, sizeof(uint16_t));
    printf("PING from '%s'\n", id);
    // printf("> Clock for '%s' is %d. Current registry is %d\n", m->id, member_clock, m->member_clock);

    if (member_clock > m->member_clock) {
        printf("%s's clock was %d but received %d\n!\n", m->id, m->member_clock, member_clock);
        send_sync_req(m->ip, m->port, SYNC_MEMB);
    }

}
