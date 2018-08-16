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
#include "mount.h"
#include "hierarchy.h"
#include "nfs_ops.h"

const char METADATA_DIR[] = "metadata/";
const char METADATA_MEMBERS[] = "metadata/members.txt";
const char METADATA_HIERARCHY[] = "metadata/logical_hierarchy.txt";

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

// Metadata

uint8_t metadata_initialize() {

    // Create the metadata directory
    if (fops_make_dir(METADATA_DIR)) return 1;
    printf("Created the metadata directory\n");

    // Create the members file
    FILE* members;
    if ((members = fops_make_file(METADATA_MEMBERS)) == NULL) return 2;
    printf("Created the members file\n");
    fclose(members);

    // Create the hierarchy file
    FILE* hierarchy;
    if ((hierarchy = fops_make_file(METADATA_HIERARCHY)) == NULL) return 3;
    printf("Created the hierarchy file\n");
    fclose(hierarchy);

    return 0;

}

uint8_t metadata_append_member(member param) {

    // Append the line
    char line[256];
    print_member(&param, line);
    strcat(line, "\n");
    return (uint8_t) fops_append_line(METADATA_MEMBERS, line);

}

// Create

int clean_create(uint8_t depth, char* nfs_directory) {
    switch (depth) {
        case 6:
            printf("Removing NFS export entry\n");
            fops_delete_line_starts_with("/etc/exports", nfs_directory);
            printf("Updating NFS daemon\n");
            system("sudo nfsd update");
        case 5:
            printf("Removing NFS directory\n");
            fops_remove_dir(nfs_directory);
        case 4:
            printf("Removing '.logical_hierarchy' file\n");
            fops_remove_file(METADATA_HIERARCHY);
        case 3:
            printf("Removing '.members' file\n");
            fops_remove_file(METADATA_MEMBERS);
        case 2:
            printf("Removing '.share' folder\n");
            fops_remove_dir(METADATA_DIR);
        default:
            return 1;
    }
}

int create(char* name, char* description, char* ip, uint16_t port) {

    // TO-DO: Allow user to specify owner of the files

    char nfs_dir[256];
    char nfs_exports_entry[256];
    char nfs_dir_rel[256] = "1/";

    nfs_exports_entry[0] = '\n';
    nfs_exports_entry[1] = '\0';
    realpath(nfs_dir_rel, nfs_dir);

    strncat(nfs_exports_entry, nfs_dir, 255);
    strncat(nfs_exports_entry, " 127.0.0.1", 255 - strlen(nfs_exports_entry));

    // Lower privileges to create files
    setegid(20);
    seteuid(501);

    // Initialize Metadata
    uint8_t result = metadata_initialize();
    if (result) return clean_create(result, NULL);

    // Write first members entry
    member m = build_member(1, ip, port);
    if (metadata_append_member(m)) return clean_create(4, NULL);
    printf("Added member entry\n");

    // Create the NFS directory
    if (fops_make_dir(nfs_dir)) return clean_create(5, NULL);
    printf("Created the NFS directory\n");

    // Retake super user privileges
    setegid(0);
    seteuid(0);

    // Modify the NFS exports file
    if (add_nfs_recp(nfs_dir, "127.0.0.1")) return clean_create(5, nfs_dir);
    printf("Updated the exports file\n");

    // Restart NFS service
    if (update_nfs()) return clean_create(6, nfs_dir);
    printf("Updated the NFS service\n");

    return 0;
}

int parse_create(int argc, char** argv) {

    // Declare arguments
    char ip[16];
    char name[51];
    uint16_t port = 4000;
    char description[257];

    // Reset memory
    ip[0] = 0;
    name[0] = 0;
    description[0] = 0;

    // Read arguments
    for (uint8_t i = 0; i < argc; ++i) {
        if (argv[i][0] == '-') {

            if (strcmp(argv[i] + 1, "p") == 0) {
                if (++i < argc) {
                    port = (uint16_t) strtol(argv[i], NULL, 10);
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
            else warning("Unknown parameter '%s'\n", argv[i]);
        }
    }

    // Check required arguments
    if (name[0] == '\0' || ip[0] == 0) return usage();

    // Print parameters
    printf("Name: %s\n", name);
    printf("Description: %s\n", description);
    printf("IP: %s\n", ip);
    printf("Port: %d\n", port);

    // Call Create
    return create(name, description, ip, port);
}

// Up

member* members = NULL;

int send_message(char* ip, uint16_t port, void* message, size_t size) {

    int sock;
    struct sockaddr_in addr;

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) return error("Failed to create socket!\n", NULL);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)))
        return error("Failed to connect to socket!\n", NULL);

    if (send(sock, message, size, 0) != size) {
        error("Failed to send message!", NULL);
        close(sock);
        return 1;
    }

    close(sock);
    return 0;

}

int proto_join(int sock) {

    char ip[16];
    char buffer[256];
    uint16_t port = 4000;

    // Receive IP and PORT
    if (recv(sock, buffer, 21, 0) != 21)
        return error("Failed to receive message! Incomplete join protocol!", NULL);

    // Parse received data
    char* token = strtok(buffer, ":");
    strncpy(ip, token, 15);

    token = strtok(NULL, ":");
    if (token != NULL) port = (uint16_t) strtol(token, NULL, 10);

    // Determine an ID
    uint16_t count = 0;
    uint16_t id = 2;
    member* aux = members;
    while (aux != NULL) {
        if (aux->id >= id) id = (uint16_t)(aux->id + 1);
        aux = aux->next;
        ++count;
    }

    // Send ID
    if (send(sock, &id, 2, 0) != 2)
        return error("Failed to send id! Incomplete join protocol!", NULL);

    // Send Members Count
    if (send(sock, &count, 2, 0) != 2)
        return error("Failed to send count! Incomplete join protocol!", NULL);

    // Send Members Entries
    aux = members;
    while (aux != NULL) {
        print_member(aux, buffer);
        printf("%s\n", buffer);

        if (send(sock, &buffer, 21, 0) != 21)
            return error("Failed to send a member! Incomplete join protocol!", NULL);

        aux = aux->next;
    }

    // Receive confirmation
    if (recv(sock, buffer, 2, 0) != 2)
        return error("Failed to receive confirmation! Incomplete join protocol!", NULL);

    printf("Received confirmation!\n");

    // Broadcast to every other machine
    char message[256];
    member m = build_member(id, ip, port);
    print_member(&m, message + 4);
    sprintf(message, "addm");

    aux = members;
    while (aux != NULL) {
        send_message(aux->ip, aux->port, message, 21);
        aux = aux->next;
    }

    return 0;
}

int proto_addm(int sock) {

    char ip[16];
    uint16_t id = 0;
    char buffer[256];
    uint16_t port = 4000;

    ip[0] = '\0';

    // Receive the data
    if (recv(sock, &buffer, 21, 0) < 0) {
        printf("%s\n", buffer);
        return error("Failed to read 'addm' data!\n", NULL);
    }

    printf("Addm: %s\n", buffer);

    // Parse the input
    char* token = strtok(buffer, " ");
    id = (uint16_t) strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    strncpy(ip, token, 15);

    token = strtok(NULL, " ");
    if (token != NULL) port = (uint16_t) strtol(token, NULL, 10);

    // Check values
    if (ip[0] == 0 || id == 0)
        return error("Invalid parameters for 'addm'", NULL);

    // Add member to metadata
    member m = build_member(id, ip, port);
    return metadata_append_member(m);

}

int proto_remm(int sock) {

    char buffer[25];

    // Read the id of the machine
    if (recv(sock, &buffer, 25, 0) < 0)
        return error("Failed to read machine ID!\n", NULL);

    printf("Removing machine %s!\n", buffer);
    uint16_t id = (uint16_t) strtol(buffer, NULL, 10);

    // Remove machine from the file
    sprintf(buffer, "%d ", id);
    fops_delete_line_starts_with(METADATA_MEMBERS, buffer);

    return 0;
}

int up(bool foreground) {

    // Go into background
    if (!foreground && fork() != 0) return 0;

    // Read members into memory
    members = read_members((char *) METADATA_MEMBERS);
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
    server.sin_port = htons( members->port );
    server.sin_addr.s_addr = INADDR_ANY;

    // Bind Server Socket
    if (bind(socket_desc, (const struct sockaddr*)&server, sizeof(server)))
        return error("Failed to bind socket!\n", NULL);

    // Listen
    listen(socket_desc, 10);
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
        if (strcmp(buffer, "join") == 0) proto_join(client_sock);
        else if (strcmp(buffer, "addm") == 0) proto_addm(client_sock);
        else if (strcmp(buffer, "remm") == 0) proto_remm(client_sock);

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

int clean_join(int socket) {

    close(socket);
    return 1;

}

int join(char* ip, uint16_t port) {

    // TO-DO Determine NFS directory and update entries

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
    if (result < 0) {
        error("Failed to connect to server\n", NULL);
        return clean_join(client_sock);
    }

    // Create message
    char message[] = "join127.0.0.1:5000";
    size_t size = 25; // Join = 4, IP = 15, :, Port = 5

    // Send messages
    if (send(client_sock, message, size, 0) != size) {
        error("Message was sent with wrong number of bytes!\n", NULL);
        return clean_join(client_sock);
    }

    // Receive ID
    uint16_t aux;
    if (recv(client_sock, &aux, 2, 0) != 2) {
        error("Failed to receive correctly sized message!\n", NULL);
        return clean_join(client_sock);
    }

    printf("Received ID: %d\n", aux);

    // Create metadata
    if (metadata_initialize()) {
        error("Failed to create metadata files!\n", NULL);
        return clean_join(client_sock);
    }
    member m = build_member(aux, "127.0.0.1", 5000);
    if (metadata_append_member(m)) {
        error("Failed to append member!\n", NULL);
        return clean_join(client_sock);
    }

    // Receive number of members
    if (recv(client_sock, &aux, 2, 0) != 2) {
        error("Failed to receive correctly sized message!\n", NULL);
        return clean_join(client_sock);
    }
    printf("$ of Members: %d\n", aux);

    // Read all members
    char buffer[21];

    while (aux--) {
        if (recv(client_sock, buffer, 21, 0) != 21) {
            error("Failed to receive correctly sized message!\n", NULL);
            return clean_join(client_sock);
        }

        m = parse_member(buffer);
        metadata_append_member(m);
    }

    // Send acknowledgement
    if (send(client_sock, "OK", 2, 0) != 2) {
        error("Failed to send 'OK'!\n", NULL);
        return clean_join(client_sock);
    }

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
    if (tok != NULL) port = (uint16_t) (uint16_t) strtol(tok, NULL, 10);

    // Check required arguments
    if (ip[0] == '\0') return error("Failed to parse ip\n", NULL);

    warning("The ip is %s\n", ip);
    printf("The port is %d\n", port);

    return join(ip, port);
}

// Mount

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
    member* aux = members;
    while (aux != NULL) {
        send_message(aux->ip, aux->port, message, 25);
        aux = aux->next;
    }

    return 0;
}

int parse_delete(int argc, char** argv) {

    uint16_t id = (uint16_t) strtol(argv[0], NULL, 10);
    return delete(id);

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
    else if (strcmp(command, "mount") == 0) return parse_mount(argc - 2, argv + 2);
    else if (strcmp(command, "remove") == 0) return parse_delete(argc - 2, argv + 2);
    else return error("Unknown command '%s'\n", command);

}
