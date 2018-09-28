#pragma once

#include "members.h"

#define FREQ_ADD "add"
#define FREQ_DEL "del"
#define FREQ_REN "ren"

int send_freq_req_add(member *m, char *path, char *name, int flags);
int send_freq_req_ren(member* m, const char* from, const char* to);
int send_freq_req_del(member* m, const char* path);

void handle_freq_protocol(char* message, int socket);
