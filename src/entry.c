#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include "fops.h"
#include "nops.h"
#include "output.h"
#include "members.h"
#include "mount.h"
#include "hierarchy.h"
#include "nfs_ops.h"
#include "server.h"
#include "protocol_name.h"
#include "background.h"

const char METADATA_DIR[] = "metadata/";
const char METADATA_HIERARCHY[] = "metadata/logical_hierarchy.txt";

int usage() {

    printf("Usage:\n\n");

    printf("\tshare create <name> [-d <description>] -ip <ip address> [-p <port>] <path>\n");
    printf("\tCreates a new network at a given path and listens for connection at a specified port (defaults to 4000)\n\n");

    printf("\tshare join -ip <ip address> [-p <port>]\n");
    printf("\tRequests a machine at <ip address> to join a network at a specified port (defaults to 4000)\n\n");

    printf("\tshare mount <path>\n");
    printf("\tMounts the network in a specified <path> using FUSE\n\n");

    printf("\tshare remove <id>\n");
    printf("\tRemove a machine from the network\n");

    return 1;
}

// Handle SIGINT
static void on_interrupt(int signal) {

    server_stop();
    stop_wait_all_background();

}

// Initialize

int initialize(char* id, char* ip, uint16_t port) {

    member* m;
    char* pwd = realpath(".", NULL);
    m = build_member(id, ip, port, pwd);

    if (fops_make_dir(METADATA_DIR)) {
        free(pwd);
        free_member(m);
        return error("Failed to create metadata folder!\n", NULL);
    }

    if (initialize_metadata_members(m)) {
        free(pwd);
        free_member(m);
        fops_remove_dir(METADATA_DIR);
        return error("Failed to initialize metadata members!\n", NULL);
    }

    if (server_start(port)) {
        free(pwd);
        free_member(m);
        remove_metadata_members();
        fops_remove_dir(METADATA_DIR);
        return error("Failed to start the server!\n", NULL);
    }

    free(pwd);
    return 0;

}

// Create

int clean_create() {

    server_stop();
    remove_metadata_members();
    fops_remove_dir(METADATA_DIR);

    return 1;
}
int create(char* ip, uint16_t port) {

    // TO-DO: Allow user to specify owner of the files

    char id[] = "1";
    char nfs[] = "1/";

    // Lower privileges to create files
    setegid(20);
    seteuid(501);

    // Initialize Metadata
    if (initialize(id, ip, port)) return 1;

    // Create the NFS directory
    if (fops_make_dir(nfs)) return clean_create();

    // Retake super user privileges
    setegid(0);
    seteuid(0);

    // REMOVE THIS LATER
    //add_nfs_recp(get_current_member(), "111.111.111.111");

    server_wait();
    return 0;
}
int parse_create(int argc, char** argv) {

    // Declare arguments
    char ip[16] = "\0";
    uint16_t port = 4000;

    // Read arguments
    for (uint8_t i = 0; i < argc; ++i) {

        if (ip[0] == 0) {

            char* token = strtok(argv[i], ":");
            strncpy(ip, token, 15);

            token = strtok(NULL, ":");
            if (token != NULL) port = (uint16_t) strtol(token, NULL, 10);

        } else
            return error("Unknown parameter '%s'\n", argv[i]);
    }

    // Check required arguments
    if (ip[0] == 0) return usage();

    // Call Create
    return create(ip, port);
}

// Join

int clean_join() {

    server_stop();
    remove_metadata_members();
    fops_remove_dir(METADATA_DIR);

    return 1;

}
int join(char* server_ip, uint16_t server_port, char* client_ip, uint16_t client_port) {

    // Lower privileges to create files
    setegid(20);
    seteuid(501);

    // Initialize Metadata
    if (initialize("", client_ip, client_port)) return 1;

    // Retake super user privileges
    setegid(0);
    seteuid(0);

    // Send Join Request
    if (send_name_req(server_ip, server_port, get_current_member())) return clean_join();

    // Wait for server to complete
    server_wait();

    return 0;

}
int parse_join(int argc, char** argv) {

    // Declare arguments
    char server[16] = "\0";
    char client[16] = "\0";
    uint16_t server_port = 4000;
    uint16_t client_port = 4000;

    // Read server address
    char* tok = strtok(argv[0], ":");
    strncpy(server, tok, 15);

    tok = strtok(NULL, ":");
    if (tok != NULL) server_port = (uint16_t) strtol(tok, NULL, 10);

    // Read client address
    tok = strtok(argv[1], ":");
    strncpy(client, tok, 15);

    tok = strtok(NULL, ":");
    if (tok != NULL) client_port = (uint16_t) strtol(tok, NULL, 10);

    // Check required arguments
    if (server[0] == '\0' || client[0] == '\0') return error("Failed to parse ips\n", NULL);
    return join(server, server_port, client, client_port);
}

// Mount

/*
int mount(char* path) {

    // Load hierarchy
    hierarchy_load(METADATA_HIERARCHY);

    // Create a list of char*
    char** list = malloc(sizeof(char*) * 3);
    char foreground[] = "-f";

    // First pointer is ignored, Second pointer is the path
    list[0] = list[1] = path;
    list[2] = foreground;

    // Call mount
    mount_dir(3, list);
    return 0;
}

int parse_mount(int argc, char** argv) {
    if (argc < 1) return error("Missing path!\n", NULL);
    return mount(argv[0]);
}

// Delete

int delete(uint16_t id) {

    // Read members
    members = read_members((char *)METADATA_MEMBERS);
    if (members == NULL)
        return error("Failed to read members file!\n", NULL);

    // Build message
    char message[25];
    sprintf(message, "remm%d", id);
    printf("> %s\n", message);

    // Send removal to all members
    int other;
    member* aux = members;
    while (aux != NULL) {

        if ((other = nops_open_connection(aux->ip, aux->port))) {
            error("Failed to send message to %s\n", aux->ip);
            continue;
        }

        send(other, message, 7, 0);
        nops_close_connection(other);
        aux = aux->next;
    }

    return 0;
}

int parse_delete(int argc, char** argv) {

    uint16_t id = (uint16_t) strtol(argv[0], NULL, 10);
    return delete(id);

}
*/

// Main

int main(int argc, char** argv) {

    //sem_unlink("etc_exports_mutex");

    // Check for minimum number of arguments
    if (argc < 2) return usage();

    // Register Interruption Handler
    signal(SIGINT, on_interrupt);

    // Read command
    char* command = argv[1];

    if (strcmp(command, "create") == 0) return parse_create(argc - 2, argv + 2);
    else if (strcmp(command, "join") == 0) return parse_join(argc - 2, argv + 2);
    // else if (strcmp(command, "mount") == 0) return parse_mount(argc - 2, argv + 2);
    // else if (strcmp(command, "remove") == 0) return parse_delete(argc - 2, argv + 2);
    else return error("Unknown command '%s'\n", command);

}
