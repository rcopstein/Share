#pragma once

#include <stdint.h>
#include "members.h"

#define SYNC_MEMB 0
#define SYNC_LHIE 1

int send_sync_req(member* memb, uint8_t type);
int send_sync_rep(member* memb, uint8_t type);

void handle_sync_protocol(char* message);
