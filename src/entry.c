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

const char METADATA_DIR[] = "metadata/";
const char METADATA_MEMBERS[] = "metadata/members.txt";
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

// Metadata

uint8_t metadata_initialize() {

    // Create the metadata directory
    if (fops_make_dir(METADATA_DIR)) return 1;
    printf("Created the metadata directory\n");

    // Create the members file
    FILE* members;
    if ((members = fopen(METADATA_MEMBERS, "w+")) == NULL) return 2;
    printf("Created the members file\n");
    fclose(members);

    // Create the hierarchy file
    FILE* hierarchy;
    if ((hierarchy = fopen(METADATA_HIERARCHY, "w+")) == NULL) return 3;
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

int clean_create(uint8_t depth, char* pwd, char* nfs_directory) {
    switch (depth) {
        case 6:
            printf("Removing NFS export entry\n");
            fops_remove_line("/etc/exports", nfs_directory);
            printf("Updating NFS daemon\n");
            system("sudo nfsd update");
        case 5:
            printf("Removing NFS directory\n");
            fops_remove_dir(nfs_directory);
        case 4:
            printf("Removing '.logical_hierarchy' file\n");
            remove(METADATA_HIERARCHY);
        case 3:
            printf("Removing '.members' file\n");
            remove(METADATA_MEMBERS);
        case 2:
            printf("Removing '.share' folder\n");
            fops_remove_dir(METADATA_DIR);
        default:
            break;
    }

    if (nfs_directory != NULL) free(nfs_directory);
    if (pwd != NULL) free(pwd);

    return 1;
}

int create(char* name, char* description, char* ip, uint16_t port) {

    // TO-DO: Allow user to specify owner of the files

    char nfs[] = "1/";
    char* pwd = realpath(".", NULL);

    // Lower privileges to create files
    setegid(20);
    seteuid(501);

    // Initialize Metadata
    uint8_t result = metadata_initialize();
    if (result) return clean_create(result, pwd, NULL);

    // Write first members entry
    member m = build_member(1, ip, port, pwd);
    if (metadata_append_member(m)) return clean_create(4, pwd, NULL);
    printf("Added member entry\n");

    // Create the NFS directory
    if (fops_make_dir(nfs)) return clean_create(5, pwd, NULL);
    printf("Created the NFS directory\n");

    char* nfs_path = realpath(nfs, NULL);

    // Retake super user privileges
    setegid(0);
    seteuid(0);

    // Modify the NFS exports file
    printf("> %s\n", nfs_path);
    if (add_nfs_recp(nfs_path, "127.0.0.1")) return clean_create(5, pwd, nfs_path);
    printf("Updated the exports file\n");

    // Restart NFS service
    if (update_nfs()) return clean_create(6, pwd, nfs_path);
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

int proto_join(int sock) {

    char ip[16];
    char pwd[256];
    char buffer[256];
    uint16_t port = 4000;

    // Receive IP and PORT
    if (recv(sock, buffer, 256, 0) < 0)
        return error("1) Failed to receive message! Incomplete join protocol: '%s'!", buffer);

    printf("Received buffer '%s'\n", buffer);

    // Parse received data
    char* token = strtok(buffer, " ");
    strncpy(ip, token, 15);

    token = strtok(NULL, " ");
    if (token != NULL) port = (uint16_t) strtol(token, NULL, 10);

    token = strtok(NULL, " ");
    strncpy(pwd, token, 255);

    printf("Received path '%s'\n", pwd);

    // Determine an ID
    uint16_t id = 2;
    uint16_t count = 0;
    member* aux = members;
    while (aux != NULL) {
        if (aux->id >= id) id = (uint16_t)(aux->id + 1);
        aux = aux->next;
        ++count;
        printf("Current ID is %d\n", count);
    }

    // Send ID
    if (send(sock, &id, 2, 0) != 2)
        return error("2) Failed to send id! Incomplete join protocol!\n", NULL);

    // Send Members Count
    if (send(sock, &count, 2, 0) != 2)
        return error("3) Failed to send count! Incomplete join protocol!\n", NULL);

    // Send Members Entries
    aux = members;
    while (aux != NULL) {
        print_member(aux, buffer);
        printf("%s\n", buffer);

        ssize_t res = send(sock, buffer, 255, 0);
        if (res < 0)
            return error("4) Failed to send a member! Incomplete join protocol!\n", NULL);
        else if (res == 0)
            return error("Socket has been closed!\n", NULL);

        printf("Sent %zu bytes!\n", res);
        aux = aux->next;
    }

    // Receive confirmation
    ssize_t res = recv(sock, buffer, 2, 0);

    if (res < 0 || strncmp(buffer, "OK", 2) != 0)
        return error("5) Failed to receive confirmation! Incomplete join protocol!", NULL);
    else if (res == 0)
        return error("Socket has been closed!\n", NULL);

    printf("Received confirmation!\n");

    // Broadcast to every other machine
    char message[256];
    sprintf(message, "addm");

    member m = build_member(id, ip, port, pwd);
    print_member(&m, message + 4);

    printf("Sending message %s\n", message);

    int other;
    aux = members;
    while (aux != NULL) {

        other = nops_open_connection(aux->ip, aux->port);

        if (other < 0) {
            error("6) Failed to send message to %s\n", aux->ip);
        } else {
            res = send(other, message, 255, 0);

            if (res < 0)
                error("Failed to send message to %s\n", aux->ip);
            else if (res == 0)
                error("Socket for %s has been closed!\n", aux->ip);

            nops_close_connection(other);
        }

        aux = aux->next;
    }

    return 0;
}

int proto_addm(int sock) {

    char buffer[256];

    // Receive the data
    if (recv(sock, &buffer, 255, 0) <= 0)
        return error("Failed to read 'addm' data: '%s'!\n", buffer);

    printf("Addm: %s\n", buffer);

    // Build the member
    member m;
    if (parse_member(buffer, &m))
        return error("Failed to parse member!\n", NULL);

    printf("Member built!\n");

    // Add member to metadata
    if (metadata_append_member(m))
        return error("Failed to append member to metadata!\n", NULL);

    printf("Member added to metadata!\n");

    // Add recipient
    sprintf(buffer, "%s/%d", members->prefix, members->id);
    if (add_nfs_recp(buffer, m.ip))
        return error("Failed to add NFS recipient!\n", NULL);

    // Create mount point
    char* path;
    asprintf(&path, "%d/", m.id);
    printf("Path is %s\n", path);

    // Lower privileges to create files
    setegid(20);
    seteuid(501);

    if (fops_make_dir(path)) {
        free(path);
        return error("Failed to create mount point for member!\n", NULL);
    }
    free(path);

    printf("Created mount point!\n");

    // Mount the NFS view
    if (mount_nfs_dir(&m)) {
        error("Failed to mount NFS view!\n", NULL);
        return 1;
    }

    // Recover privileges
    setegid(0);
    seteuid(0);

    printf("View Mounted!\n");

    // Send confirmation
    printf("Openning socket at %s:%d\n", m.ip, m.port);
    fflush(stdout);

    int ssock = nops_open_connection(m.ip, m.port);
    print_member(members, buffer);

    if (send(ssock, buffer, 255, 0) < 0) {
        printf("Failed to send confirmation: %d!\n", errno);
        close(ssock);
        return 1;
    }

    printf("OK!\n");
    close(ssock);
    return 0;

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
    fops_remove_line(METADATA_MEMBERS, buffer);

    return 0;
}

int up(bool foreground) {

    // Go into background
    if (!foreground && fork() != 0) return 0;

    // Read members into memory
    members = read_members((char *) METADATA_MEMBERS);
    if (members == NULL) return error("Failed to read members file!\n", NULL);

    // Declare variables
    char buffer[4];
    struct sockaddr_in client;
    int socket_desc, client_sock;
    socklen_t socket_size = sizeof(struct sockaddr_in);

    // Create socket
    socket_desc = nops_listen_at(members->port);
    if (socket_desc < 0) return 1;

    // Accept incoming connections
    while (1) {

        client_sock = accept(socket_desc, (struct sockaddr*)&client, &socket_size);

        if (client_sock < 0) { error("Failed to accept connection\n", NULL); continue; }
        else printf("> CONNECTION\n");

        if (recv(client_sock, buffer, 4, 0) < 0) {
            error("Failed to receive protocol operation!\n", NULL);
            continue;
        }

        // Check the protocol
             if (strncmp(buffer, "join", 4) == 0) proto_join(client_sock);
        else if (strncmp(buffer, "addm", 4) == 0) proto_addm(client_sock);
        else if (strncmp(buffer, "remm", 4) == 0) proto_remm(client_sock);

        // Close the client socket
        close(client_sock);
        printf("Closed!\n");
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

int join(char* server_ip, uint16_t server_port, char* client_ip, uint16_t client_port) {

    member m;
    uint16_t aux;
    char buffer[256];
    char* pwd = realpath(".", NULL);

    int sock = nops_open_connection(server_ip, server_port);
    if (sock < 0) return error("Failed to connect to server!\n", NULL);

    char message[256] = "join";
    sprintf(message + 4, "%s %d %s", client_ip, client_port, pwd);

    if (send(sock, message, 255, 0) < 0) {
        error("Failed to send join protocol message!\n", NULL);
        nops_close_connection(sock);
        free(pwd);
        return 1;
    }

    // Receive ID
    if (recv(sock, &aux, 2, 0) < 0) {
        error("Failed to receive ID!\n", NULL);
        nops_close_connection(sock);
        free(pwd);
        return 1;
    }

    printf("Received ID: %d\n", aux);

    // Lower privileges to create files
    setegid(20);
    seteuid(501);

    // Create metadata
    if (metadata_initialize()) {
        error("Failed to create metadata files!\n", NULL);
        nops_close_connection(sock);
        free(pwd);
        return 1;
    }

    // Append self as first member
    m = build_member(aux, client_ip, client_port, pwd);
    free(pwd);

    if (metadata_append_member(m)) {
        error("Failed to append member!\n", NULL);
        nops_close_connection(sock);
        return 1;
    }

    // Create the NFS directory
    sprintf(buffer, "%d/", aux);
    if (fops_make_dir(buffer)) return error("Failed to create NFS directory!\n", NULL);
    printf("Created the NFS directory\n");

    // Recover privileges
    setegid(0);
    seteuid(0);

    // Get full NFS path
    char* nfs_path = realpath(buffer, NULL);

    printf("The Buffer is: %s\n", buffer);
    printf("The NFS Path is: %s\n", nfs_path);

    // Receive number of members
    if (recv(sock, &aux, 2, 0) < 0) {
        error("Failed to receive correctly sized message!\n", NULL);
        nops_close_connection(sock);
        free(nfs_path);
        return 1;
    }
    printf("$ of Members: %d\n", aux);

    // Read all members

    int mems = aux;
    member* list_members = NULL;

    while (aux--) {

        if (recv(sock, buffer, 255, 0) < 0) {
            error("Failed to receive correctly sized message: '%s'!\n", buffer);
            nops_close_connection(sock);
            free(nfs_path);
            return 1;
        }

        if (parse_member(buffer, &m)) {
            error("Failed to parse '%s' to a member!\n", buffer);
            nops_close_connection(sock);
            free(nfs_path);
            return 1;
        }

        m.next = list_members;
        list_members = &m;

        metadata_append_member(m);
        add_nfs_recp(nfs_path, m.ip);
    }

    if (update_nfs()) {
        error("Failed to update NFS!\n", NULL);
        nops_close_connection(sock);
        free(nfs_path);
        return 1;
    }

    // Start to listen
    struct sockaddr_in host;
    int ssock = nops_listen_at(client_port);
    socklen_t size = sizeof(struct sockaddr_in);

    printf("Here!\n");

    if (ssock <= 0)
        return error("Failed to listen at port!\n", NULL);

    // Send acknowledgement
    if (send(sock, "OK", 2, 0) < 0) {
        error("Failed to send 'OK'!\n", NULL);
        nops_close_connection(sock);
        free(nfs_path);
        return 1;
    }

    // Accept acknowledgement connections
    int cl_socket;
    while (mems--) {

        printf(".");
        fflush(0);

        if ((cl_socket = accept(ssock, (struct sockaddr*)&host, &size)) < 0) {
            warning("Failed to accept connection!\n", NULL);
            ++mems;
            continue;
        }

        if (recv(cl_socket, buffer, 255, 0) < 0) {
            printf("Failed to read content. Error: %d!\n", errno);
            ++mems;
            continue;
        }

        if (parse_member(buffer, &m)) {
            error("Couldn't parse member from message: '%s'!\n", NULL);
            ++mems;
            continue;
        }

        printf("Received confirmation from '%s:%d'\n", m.ip, m.port);
        sprintf(buffer, "%d/", m.id);

        printf("Trying to create folder for ID %d\n", m.id);

        // Lower privileges to create files
        setegid(20);
        seteuid(501);

        if (fops_make_dir(buffer)) {
            printf("Failed to create mount point for member, error: %d!", errno);
            ++mems;
            continue;
        }

        if (mount_nfs_dir(&m)) {
            error("Failed to mount NFS dir for member", NULL);
            ++mems;
            continue;
        }

        // Recover privileges
        setegid(0);
        seteuid(0);

    }

    nops_close_connection(sock);
    free(nfs_path);
    return 0;
}

int parse_join(int argc, char** argv) {

    // Declare arguments
    char server[16];
    char client[16];
    uint16_t server_port = 4000;
    uint16_t client_port = 4000;

    server[0] = '\0';
    client[0] = '\0';

    // Read server address
    char* tok = strtok(argv[0], ":");
    strncpy(server, tok, 15);
    printf("IP: %s\n", tok);

    tok = strtok(NULL, ":");
    if (tok != NULL) server_port = (uint16_t) strtol(tok, NULL, 10);

    // Read client address
    tok = strtok(argv[1], ":");
    strncpy(client, tok, 15);
    printf("IP: %s\n", tok);

    tok = strtok(NULL, ":");
    if (tok != NULL) client_port = (uint16_t) strtol(tok, NULL, 10);

    // Check required arguments
    if (server[0] == '\0' || client[0] == '\0') return error("Failed to parse ips\n", NULL);

    printf("Server: %s:%d\nClient: %s:%d\n", server, server_port, client, client_port);
    return join(server, server_port, client, client_port);
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
