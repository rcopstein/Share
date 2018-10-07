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
#include <ctype.h>
#include <time.h>

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
#include "system.h"

const char METADATA_DIR[] = "metadata/";

// Auxiliary

uid_t read_uid(char* param) {

    char* comm = (char *) malloc(sizeof(char) * (strlen(param) + 7));
    sprintf(comm, "id -u %s", param);

    FILE* result = popen(comm, "r");
    fgets(comm, (int)(strlen(param) + 6), result);

    char* end;
    long uid = strtol(comm, &end, 10);

    if (end == comm) return (uid_t) -1;
    return (uid_t) uid;
}
gid_t read_gid(char* param) {

    char* comm = (char *) malloc(sizeof(char) * (strlen(param) + 7));
    sprintf(comm, "id -g %s", param);

    FILE* result = popen(comm, "r");
    fgets(comm, (int)(strlen(param) + 6), result);

    char* end;
    long gid = strtol(comm, &end, 10);

    if (end == comm) return (gid_t) -1;
    return (gid_t) gid;
}
int confirm_root() {

    warning("You are creating/joining this share as a super user. \n"
            "This means the filesystem will only be available "
            "to users with super user capabilities. \n"
            "Would you like to continue? (Y/n) \n\n> ", NULL);

    char res[256];
    scanf("%s", res);

    return strlen(res) == 0 || res[0] == 'y' || res[0] == 'Y' ? 0 : 1;
}
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

    char id[] = "1";
    char nfs[] = "1/";

    // Lower privileges to create files
    become_user();

    // Initialize Metadata
    if (initialize(id, ip, port)) return 1;

    // Create the NFS directory
    if (fops_make_dir(nfs)) return clean_create(0, nfs);

    // Mount Filesystem
    if (mount_dir(mountpoint)) return clean_create(1, nfs);
    // add_sample_mount_info_1();

    // Retake super user privileges
    become_root();

    // Wait for filesystem to finish operating
    mount_loop();
    stop_wait_all_background();

    return 0;
}
int parse_create(int argc, char** argv) {

    // Declare arguments
    uid_t uid = 0;
    gid_t gid = 0;
    char ip[16] = "\0";
    uint16_t port = 4000;

    char* mountpoint = NULL;

    // Read arguments
    for (uint8_t i = 0; i < argc; ++i) {

        if (argv[i][0] == '-') {

            // It's a flag
            if (argv[i][1] == 'u') {
                if (++i >= argc) return error("Expected flag value!\n", NULL);
                uid = read_uid(argv[i]);
                gid = read_gid(argv[i]);
            }

        }

        // Read IP First

        else if (ip[0] == 0) {

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
    if (uid < 0) return error("User not found!\n", NULL);

    // Update System Info
    set_uid(uid);
    set_gid(gid);
    set_root_perm(uid);
    if (uid == 0 && confirm_root()) return 1;
    printf("UID: %d GID: %d\n", uid, gid);

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
    become_user();

    // Initialize Metadata
    if (initialize("", client_ip, client_port)) return 1;

    // Mount Filesystem
    if (mount_dir(mountpoint)) return clean_join(0);
    // add_sample_mount_info_2();

    // Retake super user privileges
    become_root();

    // Send Join Request
    if (send_name_req(server_ip, server_port, get_current_member())) return clean_join(1);

    // Wait for filesystem to finish operating
    mount_loop();
    stop_wait_all_background();

    return 0;

}
int parse_join(int argc, char** argv) {

    // Declare arguments
    uid_t uid = 0;
    gid_t gid = 0;
    char server[16] = "\0";
    char client[16] = "\0";
    char* mountpoint = NULL;
    uint16_t server_port = 4000;
    uint16_t client_port = 4000;

    // Read arguments
    for (uint8_t i = 0; i < argc; ++i) {

        if (argv[i][0] == '-') {

            // It's a flag
            if (argv[i][1] == 'u') {
                if (++i >= argc) return error("Expected flag value!\n", NULL);
                uid = read_uid(argv[i]);
                gid = read_gid(argv[i]);
            }

        }

        // Read Server Address

        else if (server[0] == 0) {

            // Read server address
            char* tok = strtok(argv[0], ":");
            strncpy(server, tok, 15);

            tok = strtok(NULL, ":");
            if (tok != NULL) server_port = (uint16_t) strtol(tok, NULL, 10);

        }

        // Read Client Address

        else if (client[0] == 0) {

            char* tok = strtok(argv[1], ":");
            strncpy(client, tok, 15);

            tok = strtok(NULL, ":");
            if (tok != NULL) client_port = (uint16_t) strtol(tok, NULL, 10);

        }

        // Read Mount Point Then

        else if (mountpoint == NULL) {

            mountpoint = (char *) malloc(sizeof(char) * (strlen(argv[2]) + 1));
            strcpy(mountpoint, argv[2]);

        }

        // Fail for unknown arguments

        else {
            free(mountpoint);
            return error("Unknown parameter '%s'\n", argv[i]);
        }
    }

    // Check required arguments
    if (server[0] == '\0' || client[0] == '\0' || mountpoint == NULL) {
        free(mountpoint);
        return usage();
    }
    if (uid < 0) { free(mountpoint); return error("User not found!\n", NULL); }

    // Set System Info
    set_uid(uid);
    set_gid(gid);
    set_root_perm(uid);
    if (uid == 0 && confirm_root()) { free(mountpoint); return 1; }
    printf("UID: %d GID: %d\n", uid, gid);

    // Call Join
    return join(server, server_port, client, client_port, mountpoint);
}


// Main

// TODO: Make function for recover

int main(int argc, char** argv) {

    /*
    LogicalFile* lf1 = create_lf(false, "a.txt", "1", "r.txt");
    LogicalFile* lf2 = create_lf(false, "b.txt", "1", "r.txt");
    LogicalFile* lf3 = create_lf(false, "c.txt", "1", "r.txt");

    add_lf(lf1, "/A", true);
    add_lf(lf2, "/B", true);
    add_lf(lf3, "/C", true);

    size_t size;
    build_hierarchy_message(0, NULL, &size);
    printf("Message size: %zu\n", size);
    return 0;
     */

    test();
    return 0;

    // Check for minimum number of arguments
    if (argc < 2) return usage();

    srand(time(NULL));

    // Read command
    char* command = argv[1];

    if (strcmp(command, "create") == 0) parse_create(argc - 2, argv + 2);
    else if (strcmp(command, "join") == 0) parse_join(argc - 2, argv + 2);
    else return error("Unknown command '%s'\n", command);
}
