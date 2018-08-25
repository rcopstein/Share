#pragma once

#include <stdint.h>

int server_start(uint16_t port);

int server_send(char* ip, uint16_t port, void* message, uint16_t size);
