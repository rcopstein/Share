#pragma once

#include "members.h"

int send_mont_req(member *m);
int send_mont_rep(member *m);

void handle_mont_protocol(char *message);

