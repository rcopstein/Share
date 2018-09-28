#pragma once

#include <stdio.h>
#include <stdint.h>

void server_stop();
void server_wait();
int server_start(uint16_t port);
int server_send(char* ip, uint16_t port, void* message, size_t size);
