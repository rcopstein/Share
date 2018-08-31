#pragma once

#include <stdint.h>
#include "members.h"

int send_name_req(char *ip, uint16_t port, member *m);
int send_name_rep(char *ip, uint16_t port, char *id, size_t id_size, member *m);

void handle_name_protocol(char *message);
