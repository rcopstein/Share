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

    unmount_dir();

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


// Mount

void add_sample_mount_info() {

    LogicalFile* folder = create_logical_file(true, "Potatoes", "1", "/Users/rcopstein/Desktop/s1");
    LogicalFile* file = create_logical_file(false, "potato.xlsx", "1", "/Users/rcopstein/Desktop/Notas.xlsx");

    add_logical_file("/", folder);
    add_logical_file("/Potatoes/", file);
}


// Create

int clean_create(int level, char* nfs) {

    switch (level) {
        case 1:
            fops_remove_dir(nfs);
        case 0:
            server_stop();
            remove_metadata_members();
            fops_remove_dir(METADATA_DIR);
            break;
        default:
            break;
    }

    return 1;
}
int create(char* ip, uint16_t port, char* mountpoint) {

    // TO-DO: Allow user to specify owner of the files

    char id[] = "1";
    char nfs[] = "1/";

    // Lower privileges to create files
    setegid(20);
    seteuid(501);

    // Initialize Metadata
    if (initialize(id, ip, port)) return 1;

    // Create the NFS directory
    if (fops_make_dir(nfs)) return clean_create(0, nfs);

    // Mount Filesystem
    if (mount_dir(mountpoint)) return clean_create(1, nfs);
    add_sample_mount_info();

    // Retake super user privileges
    setegid(0);
    seteuid(0);

    // Wait for filesystem to finish operating
    mount_loop();
    stop_wait_all_background();

    return 0;
}
int parse_create(int argc, char** argv) {

    // Declare arguments
    char ip[16] = "\0";
    uint16_t port = 4000;

    char* mountpoint = NULL;

    // Read arguments
    for (uint8_t i = 0; i < argc; ++i) {

        // Read IP First

        if (ip[0] == 0) {

            char *token = strtok(argv[i], ":");
            strncpy(ip, token, 15);

            token = strtok(NULL, ":");
            if (token != NULL) port = (uint16_t) strtol(token, NULL, 10);

        }

        // Read Mount Point Then

        else if (mountpoint == NULL) {

            mountpoint = (char *) malloc(sizeof(char) * (strlen(argv[i]) + 1));
            strcpy(mountpoint, argv[i]);

        }

        // Fail for unknown arguments

        else {
            free(mountpoint);
            return error("Unknown parameter '%s'\n", argv[i]);
        }
    }

    // Check required arguments
    if (ip[0] == 0 || mountpoint == NULL) return usage();

    // Call Create
    return create(ip, port, mountpoint);
}


// Join

int clean_join(int level) {

    switch (level) {
        case 1:
            unmount_dir();
        case 0:
            server_stop();
            remove_metadata_members();
            fops_remove_dir(METADATA_DIR);
        default:
            break;
    }

    return 1;
}
int join(char* server_ip, uint16_t server_port, char* client_ip, uint16_t client_port, char* mountpoint) {

    // Lower privileges to create files
    setegid(20);
    seteuid(501);

    // Initialize Metadata
    if (initialize("", client_ip, client_port)) return 1;

    // Mount Filesystem
    if (mount_dir(mountpoint)) return clean_join(0);
    add_sample_mount_info();

    // Retake super user privileges
    setegid(0);
    seteuid(0);

    // Send Join Request
    if (send_name_req(server_ip, server_port, get_current_member())) return clean_join(1);

    // Wait for filesystem to finish operating
    mount_loop();
    stop_wait_all_background();

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

    // Read Mount Point
    char* mountpoint = (char *) malloc(sizeof(char) * (strlen(argv[2]) + 1));
    strcpy(mountpoint, argv[2]);

    // Check required arguments
    if (server[0] == '\0' || client[0] == '\0') return error("Failed to parse ips\n", NULL);
    return join(server, server_port, client, client_port, mountpoint);
}


// Main

int main(int argc, char** argv) {

    //sem_unlink("etc_exports_mutex");

    // Check for minimum number of arguments
    if (argc < 2) return usage();

    // Register Interruption Handler
    signal(SIGINT, on_interrupt);

    // Read command
    char* command = argv[1];

    if (strcmp(command, "create") == 0) parse_create(argc - 2, argv + 2);
    else if (strcmp(command, "join") == 0) parse_join(argc - 2, argv + 2);
    else return error("Unknown command '%s'\n", command);
}
