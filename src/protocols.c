#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "output.h"
#include "protocol_join.h"

void protocol_handle(char* content, uint16_t size) {

    if (size < 4) {
        warning("Message is too small! Couldn't identify protocol!\n", NULL);
        return;
    }

    printf("Here!\n");

    char protocol[4];
    strncpy(protocol, content, 4);

    if (strncmp("join", protocol, 4) == 0 || 1) protocol_join_received(content + 4);
    else warning("Received unknown protocol '%s'\n", protocol);

}
