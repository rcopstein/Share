#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "server.h"
#include "output.h"
#include "protocol_name.h"
#include "protocol_join.h"
#include "protocol_mont.h"

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
    else if (strncmp("jrep", protocol, PROTOCOL_SIZE) == 0) handle_join_rep(content);
    else if (strncmp("jack", protocol, PROTOCOL_SIZE) == 0) handle_join_ack(content);
    else warning("Received unknown protocol '%s'\n", protocol);
}
