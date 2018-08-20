#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "output.h"
#include "nops.h"

int nops_open_connection(char* ip, uint16_t port) {

    int sock;
    struct sockaddr_in addr;

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock < 0) {
        error("Failed to create socket!\n", NULL);
        return -1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    printf("Waiting for %s:%d to accept!\n", ip, port);
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr))) {
        error("Failed to connect to socket!\n", NULL);
        return -1;
    }

    return sock;
}

int nops_close_connection(int conn) {

    return close(conn);

}

int nops_listen_at(uint16_t port) {

    // Create socket
    int conn = socket(PF_INET, SOCK_STREAM, 0);
    if (conn < 0) return error("Failed to create socket!\n", NULL);

    // Prepare Server Address
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    // Bind Server Socket
    if (bind(conn, (const struct sockaddr*)&server, sizeof(server))) {
        close(conn);
        error("Failed to bind socket!\n", NULL);
        return -1;
    }

    // Listen
    if (listen(conn, 10)) {
        close(conn);
        error("Failed to bind socket!\n", NULL);
        return -1;
    }

    printf("Waiting for incoming connections!\n");
    return conn;
}
