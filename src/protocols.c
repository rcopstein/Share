#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "server.h"
#include "output.h"
#include "protocol_name.h"
#include "protocol_mont.h"
#include "protocol_ping.h"
#include "protocol_sync.h"

#define PROTOCOL_SIZE 4

void protocol_handle(char* content, size_t size) {

    if (size < PROTOCOL_SIZE) {
        warning("Message is too small! Couldn't identify protocol!\n", NULL);
        return;
    }

    char protocol[PROTOCOL_SIZE];
    memcpy(protocol, content, PROTOCOL_SIZE);

    content += PROTOCOL_SIZE;

    if (strncmp("name", protocol, PROTOCOL_SIZE) == 0) handle_name_protocol(content);
    else if (strncmp("mont", protocol, PROTOCOL_SIZE) == 0) handle_mont_protocol(content);
    else if (strncmp("ping", protocol, PROTOCOL_SIZE) == 0) handle_ping_protocol(content);
    else if (strncmp("sync", protocol, PROTOCOL_SIZE) == 0) handle_sync_protocol(content);
    else warning("Received unknown protocol '%s'\n", protocol);
}
