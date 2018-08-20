#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int nops_open_connection(char* ip, uint16_t port);

int nops_close_connection(int conn);

int nops_listen_at(uint16_t port);
