//
// # Entry Module
//

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "fops.h"
#include "output.h"

const char METADATA_DIR[] = "/SHARE_EXAMPLE/";
const char METADATA_MEMBERS[] = "/MEMBERS.txt";
const char METADATA_HIERARCHY[] = "/LOGICAL_HIERARCHY.txt";

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

int create(char* name, char* description, char* ip, uint16_t port, char* path) {

    // TO-DO: Check if path exists before doing anything
    // TO-DO: Check a way to create files without sudo

    char* base;
    char* baseDir;
    char buffer[256];

    base = buffer + strlen(path);
    baseDir = base + strlen(METADATA_DIR);

    strcpy(buffer, path);
    strcpy(base, METADATA_DIR);

    // Create the .share directory
    fops_make_dir(buffer);
    printf("Created metadata directory: %s!\n", buffer);

    // Create the metadata files
    strcpy(baseDir, METADATA_MEMBERS);
    FILE* members = fops_make_file(buffer);
    printf("Created members file: %s!\n", buffer);

    strcpy(baseDir, METADATA_HIERARCHY);
    FILE* hierarchy = fops_make_file(buffer);
    printf("Created hierarchy file: %s!\n", buffer);

    // Write first member entry
    fprintf(members, "  ID              IP    SIZE (MB)   USAGE (MB)\n");
  //fprintf(members, "9999 192.168.000.111 100000000000 100000000000\n");
    fprintf(members, "%4d %15s %12d %12d", 1, ip, 0, 0);

    // Create the NFS directory
    strcpy(base, "/1");
    fops_make_dir(buffer);

    // Add directory to NFS Exports
    strcat(buffer, " 127.0.0.1");
    fops_append_line("/etc/exports", buffer);
    int result = system("sudo nfsd update");

    // Close resources
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

int main(int argc, char** argv) {

    // Check for minimum number of arguments
    if (argc < 2) return usage();

    // Read command
    char* command = argv[1];

    if (strcmp(command, "create") == 0) return parse_create(argc - 2, argv + 2);
    else if (strcmp(command, "join") == 0) {}
    else if (strcmp(command, "mount") == 0) {}
    else if (strcmp(command, "remove") == 0) {}
    else return error("Unknown command '%s'\n", command);

    return 0;

    // Declare arguments
    char ip[16] = {0};
    char name[51] = {0};
    uint16_t port = 4000;
    char description[257] = {0};

    // Read arguments
    for (uint8_t i = 1; i < argc; ++i) {
        if (argv[i][0] == '-') {

            if (strcmp(argv[i], "-ip") == 0) {
                if (++i < argc) strncpy(ip, argv[i], sizeof(char) * 15);
            }

            else if (strcmp(argv[i], "-p") == 0) {
                if (++i < argc) port = (uint16_t) atoi(argv[i]);
                else {
                    printf("Found '-p' flag but value is missing!\n");
                    return 1;
                }
            }

            else if (strcmp(argv[i], "-d") == 0) {
                if (++i < argc) strncpy(description, argv[i], sizeof(char) * 256);
                else {
                    printf("Found '-d' flag but value is missing!\n");
                    return 1;
                }
            }

            else warning("Flag '%s' not defined!", argv[i]);

        } else {
            if (name[0] == '\0') strncpy(name, argv[i], sizeof(char) * 50);
            else warning("Unknown parameter: '%s'", argv[i]);
        }
    }

    // Check if required values were provided

    if (name[0] == '\0') return error("\nRequired parameter 'name' was not provided!\n", NULL);
    if (ip[0] == '\0') return error("Required parameter 'ip' was not provided", NULL);

    // Print values
    printf("Name: %s\n", name);
    printf("Description: %s\n", description);
    printf("IP: %s\n", ip);
    printf("Port: %d\n", port);

}
