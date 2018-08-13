//
// # Entry Module
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "fops.h"
#include "output.h"
#include "members.h"

const char METADATA_DIR[] = "/metadata/";
const char METADATA_MEMBERS[] = "/members.txt";
const char METADATA_HIERARCHY[] = "/logical_hierarchy.txt";

int usage() {
    // BOLD BLACK = \033[1m\033[30m
    // RESET = \033[0m

    printf("Usage:\n\n");

    printf("\033[1m\033[30m\tshare create <name> [-d <description>] -ip <ip address> [-p <port>] <path>\033[0m\n");
    printf("\tCreates a new network at a given path and listens for connection at a specified port (defaults to 4000)\n\n");

    printf("\033[1m\033[30m\tshare join -ip <ip address> [-p <port>]\033[0m\n");
    printf("\tRequests a machine at <ip address> to join a network at a specified port (defaults to 4000)\n\n");

    printf("\033[1m\033[30m\tshare mount <path>\033[0m\n");
    printf("\tMounts the network in a specified <path> using FUSE\n\n");

    printf("\033[1m\033[30m\tshare remove <id>\033[0m\n");
    printf("\tRemove a machine from the network\n");

    return 1;
}

// Create

int clean_create(uint8_t depth, char* metadata_dir, char* members_file, char* hierarchy_file, char* nfs_directory) {
    switch (depth) {
        case 5:
            printf("Removing NFS export entry\n");
            fops_delete_line_starts_with("/etc/exports", nfs_directory);
            printf("Updating NFS daemon\n");
            system("sudo nfsd update");
        case 4:
            printf("Removing NFS directory\n");
            fops_remove_dir(nfs_directory);
        case 3:
            printf("Removing '.logical_hierarchy' file\n");
            fops_remove_file(hierarchy_file);
        case 2:
            printf("Removing '.members' file\n");
            fops_remove_file(members_file);
        case 1:
            printf("Removing '.share' folder\n");
            fops_remove_dir(metadata_dir);
        default:
            return 1;
    }
}

int create(char* name, char* description, char* ip, uint16_t port, char* path) {

    // TO-DO: Check if path exists before doing anything
    // TO-DO: Check a way to create files without sudo
    // TO-DO: Allow user to specify owner of the files

    char nfs_dir[256];
    char metadata_dir[256];
    char members_file[256];
    char hierarchy_file[256];
    char nfs_exports_entry[256];

    nfs_exports_entry[0] = '\n';
    nfs_exports_entry[1] = '\0';

    strncpy(nfs_dir, path, 255);
    strncpy(metadata_dir, path, 255);

    strncat(nfs_dir, "/1/", 255 - strlen(nfs_dir));
    strncat(metadata_dir, METADATA_DIR, 255 - strlen(metadata_dir));

    strncat(nfs_exports_entry, nfs_dir, 255);
    strncpy(members_file, metadata_dir, 255);
    strncpy(hierarchy_file, metadata_dir, 255);

    strncat(members_file, METADATA_MEMBERS, 255 - strlen(members_file));
    strncat(hierarchy_file, METADATA_HIERARCHY, 255 - strlen(hierarchy_file));
    strncat(nfs_exports_entry, " 127.0.0.1", 255 - strlen(nfs_exports_entry));

    // Lower privileges to create files
    setegid(20);
    seteuid(501);

    // Create the metadata directory
    if (fops_make_dir(metadata_dir))
        return clean_create(0, NULL, NULL, NULL, NULL);
    printf("Created the metadata directory\n");

    // Create the members file
    FILE* members;
    if ((members = fops_make_file(members_file)) == NULL)
        return clean_create(1, metadata_dir, NULL, NULL, NULL);
    printf("Created the members file\n");

    // Create the hierarchy file
    FILE* hierarchy;
    if ((hierarchy = fops_make_file(hierarchy_file)) == NULL)
        return clean_create(2, metadata_dir, members_file, NULL, NULL);
    printf("Created the hierarchy file\n");

    // Write first members entry
    int result = fprintf(members, "%d %s %d %d %d\n", 1, ip, port, 0, 0);
    if (result < 0)
        return clean_create(3, metadata_dir, members_file, hierarchy_file, NULL);
    printf("Added member entry\n");

    // Create the NFS directory
    if (fops_make_dir(nfs_dir))
        return clean_create(3, metadata_dir, members_file, hierarchy_file, NULL);
    printf("Created the NFS directory\n");

    // Retake super user privileges
    setegid(0);
    seteuid(0);

    // Modify the NFS exports file
    if (fops_append_line("/etc/exports", nfs_exports_entry))
        return clean_create(4, metadata_dir, members_file, hierarchy_file, nfs_dir);
    printf("Updated the exports file\n");

    // Restart NFS service
    if (system("sudo nfsd update"))
        return clean_create(5, metadata_dir, members_file, hierarchy_file, nfs_dir);
    printf("Updated the NFS service\n");

    // Close files
    fclose(hierarchy);
    fclose(members);

    return 0;
}

int parse_create(int argc, char** argv) {

    // Declare arguments
    char ip[16];
    char name[51];
    char path[257];
    uint16_t port = 4000;
    char description[257];

    // Reset memory
    ip[0] = 0;
    name[0] = 0;
    path[0] = 0;
    description[0] = 0;

    // Read arguments
    for (uint8_t i = 0; i < argc; ++i) {
        if (argv[i][0] == '-') {

            if (strcmp(argv[i] + 1, "p") == 0) {
                if (++i < argc) {
                    port = (uint16_t) atoi(argv[i]);
                } else return error("Found flag '-p' but value is missing\n", NULL);
            }

            else if (strcmp(argv[i] + 1, "d") == 0) {
                if (++i < argc) {
                    strncpy(description, argv[i], 256);
                }
                else return error("Found flag '-d' but value is missing\n", NULL);
            }

            else return error("Unknown flag '%s'\n", argv[i]);
        } else {
            if (name[0] == '\0') strncpy(name, argv[i], 50);
            else if (ip[0] == '\0') strncpy(ip, argv[i], 15);
            else if (path[0] == '\0') strncpy(path, argv[i], 256);
            else warning("Unknown parameter '%s'\n", argv[i]);
        }
    }

    // Check required arguments
    if (name[0] == '\0' || ip[0] == 0 || path[0] == 0) return usage();

    // Print parameters
    printf("Name: %s\n", name);
    printf("Description: %s\n", description);
    printf("IP: %s\n", ip);
    printf("Port: %d\n", port);

    // Call Create
    return create(name, description, ip, port, path);
}

// Up
member* members = NULL;

int proto_join(int server_socket, int client_socket) {

    char ip[16];
    char buffer[256];
    uint16_t port = 4000;

    // Receive IP and PORT
    if (recv(client_socket, buffer, 21, 0) != 21) {
        return error("Failed to receive message! Incomplete join protocol!", NULL);
    }

    // Parse received data
    char* token = strtok(buffer, ":");
    strncpy(ip, token, 15);

    token = strtok(NULL, ":");
    if (token != NULL) port = atoi(token);

    // Determine an IP
    uint16_t id = 2;
    member* aux = members;
    while (aux != NULL) {
        if (aux->id >= id) id = (uint16_t)(aux->id + 1);
        aux = aux->next;
    }

    // Send IP
    if (send(client_socket, &id, 2, 0) != 2) {
        return error("Failed to send id! Incomplete join protocol!", NULL);
    }

    return 0;
}

int up(bool foreground) {

    // TO-DO Move port argument to this method

    // Go into background
    if (!foreground && fork() != 0) return 0;

    // Read members into memory
    char path[25];
    strcpy(path, "./");
    strcat(path, METADATA_DIR);
    strcat(path, METADATA_MEMBERS);
    members = parse_members(path);

    if (members == NULL) return error("Failed to read members file!\n", NULL);

    member* aux = members;
    while (aux != NULL) { printf("%s:%d\n", aux->ip, aux->port); aux = aux->next; }

    // Declare variables
    char buffer[2000];
    ssize_t read_size;
    int socket_desc, client_sock;
    struct sockaddr_in server, client;
    socklen_t socket_size = sizeof(struct sockaddr_in);

    // Create Socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) return error("Failed to create socket!\n", NULL);

    // Prepare Server Address
    server.sin_family = AF_INET;
    server.sin_port = htons( 4000 );
    server.sin_addr.s_addr = INADDR_ANY;

    // Bind Server Socket
    if (bind(socket_desc, (const struct sockaddr*)&server, sizeof(server)))
        return error("Failed to bind socket!\n", NULL);

    // Listen
    listen(socket_desc, 3);
    printf("Waiting for incoming connections!\n");

    // Accept incoming connections
    while (1) {

        client_sock = accept(socket_desc, (struct sockaddr*)&client, &socket_size);
        printf("Client Socket: %d\n", client_sock);

        if (client_sock < 0) { error("Failed to accept connection\n", NULL); continue; }
        else printf("Accepted a connection!\n");

        // Read the protocol operation
        read_size = recv(client_sock, buffer, 4, 0);
        buffer[4] = '\0';
        printf("> %s\n", buffer);

        // Check the protocol
        if (strcmp(buffer, "join") == 0) proto_join(socket_desc, client_sock);

        // Close the client socket
        close(client_sock);
    }

    // Close the socket
    close(socket_desc);
    return 0;
}

int parse_up(int argc, char** argv) {

    // Declare arguments
    bool foreground = false;

    // Read arguments
    for (uint8_t i = 0; i < argc; ++i) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i] + 1, "f") == 0) foreground = true;
            else return error("Unknown flag '%s'\n", argv[i]);
        }
    }

    // Call Up
    return up(foreground);
}

// Join

int join(char* ip, uint16_t port) {

    // Declare variables
    int client_sock;
    struct sockaddr_in server_addr;

    // Initialize socket
    client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_sock < 0) return error("Failed to create socket\n", NULL);

    // Initialize server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    // Connect to server
    int result = connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (result < 0) return error("Failed to connect to server\n", NULL);

    // Create message
    char message[] = "join127.0.0.1:5000";
    size_t size = 25; // Join = 4, IP = 15, :, Port = 5

    // Send messages
    if (send(client_sock, message, size, 0) != size)
        return error("Message was sent with wrong number of bytes!\n", NULL);

    // Receive ID
    uint16_t id;
    if (recv(client_sock, &id, 2, 0) != 2)
        return error("Failed to receive correctly sized message!\n", NULL);

    printf("Received ID: %d\n", id);

    close(client_sock);
    return 0;
}

int parse_join(int argc, char** argv) {

    // Declare arguments
    char ip[16];
    uint16_t port = 4000;

    // Initialize Arguments
    ip[0] = '\0';

    // Read arguments
    printf("Arg: %s\n", argv[0]);

    char* tok = strtok(argv[0], ":");
    printf("%s\n", tok);
    strncpy(ip, tok, 15);

    tok = strtok(NULL, ":");
    if (tok != NULL) port = (uint16_t) atoi(tok);

    // Check required arguments
    if (ip[0] == '\0') return error("Failed to parse ip\n", NULL);

    warning("The ip is %s\n", ip);
    printf("The port is %d\n", port);

    return join(ip, port);
}

// Main

int main(int argc, char** argv) {

    // Check for minimum number of arguments
    if (argc < 2) return usage();

    // Read command
    char* command = argv[1];

    if (strcmp(command, "create") == 0) return parse_create(argc - 2, argv + 2);
    else if (strcmp(command, "up") == 0) return parse_up(argc - 2, argv + 2);
    else if (strcmp(command, "join") == 0) return parse_join(argc - 2, argv + 2);
    else if (strcmp(command, "mount") == 0) {}
    else if (strcmp(command, "remove") == 0) {}
    else return error("Unknown command '%s'\n", command);

    return 0;

}