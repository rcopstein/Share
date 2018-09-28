#pragma once

#include "members.h"

#define FREQ_ADD "add"
#define FREQ_DEL "del"
#define FREQ_REN "ren"

int send_freq_req(const char* type, member* m, char* param1, char* param2);

void handle_freq_protocol(char* message, int socket);
