#pragma once

#include "members.h"

int send_mont_req(char *ip, unsigned short port, member *m);
int send_mont_rep(char *ip, unsigned short port, member *m);

void handle_mont_protocol(char *message);

