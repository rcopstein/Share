#pragma once

#include <stdint.h>

void* server_start(void* vport);

int server_send(char* ip, uint16_t port, void* message, size_t size);
