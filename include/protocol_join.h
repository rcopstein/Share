#pragma once

#include "members.h"

void handle_join_req(char *message);
void handle_join_rep(char *message);
void handle_join_ack(char *message);

int send_join_req(char* ip, uint16_t port, member* m);
int send_join_ack(char* ip, uint16_t port, char* id, size_t id_size);
int send_join_rep(char* ip, uint16_t port, char* id, size_t id_size, member* m);
