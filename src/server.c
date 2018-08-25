#include <stdint.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <ctype.h>

#include "protocols.h"
#include "output.h"
#include "nops.h"

#include "members.h"

// Variables
static int stop = 0;
static int server_socket;
static pthread_t server_thread;

// Server Cleanup
void server_cleanup(int signal) {
    stop = 1;
    nops_close_connection(server_socket);
}

// Starts Server at Port
void* server_start(void* vport) {

    int client_socket;
    struct sockaddr_in client;
    socklen_t socket_size = sizeof(struct sockaddr_in);

    uint16_t port = *((uint16_t*)vport);

    if ((server_socket = nops_listen_at(port)) < 0) {
        error("Failed to listen at port!", NULL);
        return NULL;
    }

    signal(SIGINT, &server_cleanup);

    for (;;) {

        client_socket = accept(server_socket, (struct sockaddr*)&client, &socket_size);
        if (stop) break;

        if (client_socket < 0) { error("Failed to accept connection\n", NULL); continue; }
        else printf("> CONNECTION\n");

        size_t size = 0;
        char* buffer = NULL;
        int result = nops_read_message(client_socket, (void**)&buffer, &size);

        if (result == NOPS_DISCONNECTED) {
            warning("Socket disconnected!\n", NULL);
            continue;
        } else if (result == NOPS_FAILURE) {
            error("Failed to read from socket!\n", NULL);
            continue;
        }

        printf("Received message: '%s'\n", buffer);
        printf("Size is: '%zu'\n\n", size);

        for (int i = 0; i < size; ++i) {
            if (isprint(buffer[i])) printf("%c", buffer[i]);
            else printf("'%u'", buffer[i]);
        }
        printf("\n");

        protocol_handle(buffer, size);
        free(buffer);

        nops_close_connection(client_socket);
        printf("> CLOSED\n");
    }

    return NULL;
}

int server_send(char* ip, uint16_t port, void* message, size_t size) {

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

    if (argc < 2) return 1;
    uint16_t port = (uint16_t) strtol(argv[1], NULL, 10);

    printf("My port is %d\n", port);
    pthread_create(&server_thread, NULL, server_start, &port);

    if (port == 5000) {

        member* m = build_member("", "127.0.0.1", port, "/Users/rcopstein/Desktop/s2");
        add_member(m);

        char sample[256] = "jreq";
        char *aux = (char *) (&sample) + 4;
        size_t size = serialize_member(m, &aux);

        server_send("127.0.0.1", 4000, sample, (uint16_t)(size + 4));
    }
    else {

        member* m = build_member("AA", "127.0.0.1", port, "/Users/rcopstein/Desktop/s1");
        add_member(m);

    }

    pthread_join(server_thread, NULL);
    printf("\nClosing gracefully!\n");
    return 0;

}
