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

    char protocol[4];
    strncpy(protocol, content, 4);
    content += 4;

    if (strncmp("jreq", protocol, 4) == 0) protocol_join_req(content);
    else if (strncmp("jrep", protocol, 4) == 0) protocol_join_rep(content);

    else warning("Received unknown protocol '%s'\n", protocol);

}
