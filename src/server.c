#include <stdint.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <ctype.h>
#include <errno.h>

#include "protocols.h"
#include "output.h"
#include "nops.h"

#include "members.h"

// Variables
static int stop = 0;
static uint16_t port;
static int server_socket;
static pthread_t server_thread = 0;

// Wait for the server to finish
void server_wait() {

    pthread_join(server_thread, NULL);

}

// Stop the Server
void server_stop() {

    stop = 1;
    nops_close_connection(server_socket);
    server_wait();

    remove_metadata_members();
    printf("Finished!\n");
    fflush(0);

}

// Server Cleanup
static void server_cleanup(int signal) {
    server_stop();
}

// Starts Server at Port
void* _server_start(void* vport) {

    int client_socket;
    struct sockaddr_in client;
    socklen_t socket_size = sizeof(struct sockaddr_in);

    uint16_t port = *((uint16_t*)vport);

    if ((server_socket = nops_listen_at(port)) < 0) {
        error("Failed to listen at port!\n", NULL);
        printf("Error: %d\n", errno);
        return NULL;
    }

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
int server_start(uint16_t _port) {

    port = _port;
    if (signal(SIGINT, &server_cleanup) == SIG_ERR) return 1;
    return pthread_create(&server_thread, NULL, _server_start, &port);

}

// Send a message using the server
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
