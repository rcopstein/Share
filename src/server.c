#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "protocols.h"
#include "output.h"
#include "nops.h"

#include "members.h"

int server_start(uint16_t port) {

    int sock;
    int client_sock;
    struct sockaddr_in client;
    socklen_t socket_size = sizeof(struct sockaddr_in);

    if ((sock = nops_listen_at(port)) < 0)
        return error("Failed to listen at port!", NULL);

    while (1) {

        client_sock = accept(sock, (struct sockaddr*)&client, &socket_size);

        if (client_sock < 0) { error("Failed to accept connection\n", NULL); continue; }
        else printf("> CONNECTION\n");

        uint16_t size = 0;
        char* buffer = NULL;
        int result = nops_read_message(client_sock, (void**)&buffer, &size);

        if (result == NOPS_DISCONNECTED) {
            warning("Socket disconnected!\n", NULL);
            continue;
        } else if (result == NOPS_FAILURE) {
            error("Failed to read from socket!\n", NULL);
            continue;
        }

        printf("Received message: '%s'\n", buffer);
        printf("Size is: '%d'\n\n", size);

        // TO-DO Do this in separate thread
        protocol_handle(buffer, size);
        free(buffer);

    }

    nops_close_connection(sock);
    return 0;

}

int server_send(char* ip, uint16_t port, void* message, uint16_t size) {

    // Open the connection socket
    int sock = nops_open_connection(ip, port);
    if (sock < 0) return error("Failed to open connection!\n", NULL);

    // Send the message
    if (nops_send_message(sock, message, size)) {
        error("Failed to send message!\n", NULL);
        nops_close_connection(sock);
        return 1;
    }

    nops_close_connection(sock);
    return 0;

}

int main(int argc, char** argv) {

    // Temporary
    members = build_member("AA", "127.000.000.111", 4000, "/Users/rcopstein/Desktop");

    char sample[256] = "jreq";
    char *aux = (char *)(&sample) + 4;
    serialize_member(members, &aux);

    //server_start(4000);
    protocol_handle(sample, 40);
    return 0;

}
