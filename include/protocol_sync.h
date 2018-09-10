#pragma once

#include <stdint.h>

const uint8_t SYNC_MEMB = 0;
const uint8_t SYNC_LHIE = 1;

int send_sync_req(char* ip, uint16_t port, uint8_t type);
int send_sync_rep(char* ip, uint16_t port, uint8_t type);

void handle_sync_protocol(char* message);
