#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define NOPS_SUCCESS 0
#define NOPS_DISCONNECTED 1
#define NOPS_FAILURE 2

int nops_read_message(int conn, void** buffer, size_t* size);

int nops_send_message(int conn, void* content, size_t size);

int nops_open_connection(char* ip, uint16_t port);

int nops_close_connection(int conn);

int nops_listen_at(uint16_t port);
