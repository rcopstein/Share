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
#include <stdint.h>
#include <errno.h>

#include "output.h"
#include "nops.h"

int nops_read_message(int conn, void** buffer, size_t * size) {

    uint16_t msg_size;
    ssize_t result = recv(conn, &msg_size, sizeof(uint16_t), 0);

    if (result == 0) return NOPS_DISCONNECTED;
    else if (result < 0) return NOPS_FAILURE;

    if (*buffer == NULL || *size < msg_size) {
        *buffer = realloc(*buffer, msg_size);
        *size = msg_size;
    }

    if (*buffer == NULL) return NOPS_FAILURE;

    result = recv(conn, *buffer, msg_size, 0);

    if (result == 0) return NOPS_DISCONNECTED;
    else if (result < 0) return NOPS_FAILURE;

    return NOPS_SUCCESS;
}

int nops_send_message(int conn, void* content, size_t size) {

    void* message = malloc(size + sizeof(uint16_t));
    uint16_t* message_content = (uint16_t *)message + 1;

    memcpy(message, &size, sizeof(uint16_t));
    memcpy(message_content, content, size);

    size_t message_size = size + sizeof(uint16_t);
    ssize_t result = send(conn, message, message_size, 0);

    free(message);

    if (result > 0) return NOPS_SUCCESS;
    return result == 0 ? NOPS_DISCONNECTED : NOPS_FAILURE;
}

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

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr))) {
        // printf("Failed to connect to socket! Error: %d\n", errno);
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

    return conn;
}
