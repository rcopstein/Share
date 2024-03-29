#include <stdint.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <errno.h>

#include "semaphores.h"
#include "protocols.h"
#include "output.h"
#include "nops.h"

#include "members.h"

// #define _DEBUG_SERVER_MESSAGES

// Variables
static int stop = 0;
static uint16_t port;
static int server_socket;
static pthread_t server_thread = 0;

static semaphore* sem = NULL;

// Wait for the server to finish
void server_wait() {

    pthread_join(server_thread, NULL);

}

// Stop the Server
void server_stop() {

    stop = 1;
    nops_close_connection(server_socket);
    server_wait();

    printf("Finished!\n");
    fflush(0);

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

        uint32_t size = 0;
        char* buffer = NULL;
        int result = nops_read_message(client_socket, &buffer, &size);

        if (result == NOPS_DISCONNECTED) {
            warning("Socket disconnected!\n", NULL);
            continue;
        } else if (result == NOPS_FAILURE) {
            error("Failed to read from socket!\n", NULL);
            continue;
        }

#ifdef _DEBUG_SERVER_MESSAGES

        if (sem == NULL) {
            sem = (semaphore *) malloc(sizeof(semaphore));
            portable_sem_init(sem, 1);
        }
        portable_sem_wait(sem);

        printf("\nSize is: '%u'\n", size);
        for (int i = 0; i < size; ++i) {
            if (isprint(buffer[i])) printf("%c", buffer[i]);
            else printf("'%u'", buffer[i]);
        }
        printf("\n\n");

        portable_sem_post(sem);

#endif

        protocol_handle(buffer, size, client_socket);
        free(buffer);

        nops_close_connection(client_socket);
    }

    nops_close_connection(server_socket);
    return NULL;
}
int server_start(uint16_t _port) {

    port = _port;
    return pthread_create(&server_thread, NULL, _server_start, &port);

}

// Send a message using the server
int server_send(char* ip, uint16_t port, void* message, size_t size) {

    // Check for stop
    if (stop) { printf("Server is stopped!"); return 1; }

    // Open the connection socket
    int sock = nops_open_connection(ip, port);
    if (sock < 0) return error("Failed to open connection!\n", NULL);

    // Test the size being passed
    if (size > UINT32_MAX) {
        printf("!!! SIZE WILL OVERFLOW !!!");
    }

    // Send the message
    if (nops_send_message(sock, message, (uint32_t) size)) {
        error("Failed to send message!\n", NULL);
        nops_close_connection(sock);
        return 1;
    }

#ifdef _DEBUG_SERVER_MESSAGES

    if (sem == NULL) {
        sem = (semaphore *) malloc(sizeof(semaphore));
        portable_sem_init(sem, 1);
    }
    portable_sem_wait(sem);

    char* buffer = (char *) message;
    printf("\nSent: ");
    for (int i = 0; i < size; ++i) {
        if (isprint(buffer[i])) printf("%c", buffer[i]);
        else printf("'%u'", buffer[i]);
    }
    printf("\n\n");

    portable_sem_post(sem);

#endif

    nops_close_connection(sock);
    return 0;

}
