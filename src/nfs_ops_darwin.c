#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <output.h>
#include <members.h>

#include "fops.h"
#include "nfs_ops.h"

const char exportsFile[] = "/etc/exports";

// Update NFS exports
int update_nfs() {
    return system("sudo nfsd update");
}

// Remove a NFS directory
int remove_nfs_dir(char* path) {

    size_t size = strlen(path);
    size = (size_t) fmin(size, 254);

    char buffer[256];
    strncpy(buffer, path, 254);
    buffer[size] = ' ';
    buffer[size+1] = '\0';

    fops_update_line(exportsFile, buffer, NULL);

    return 0;

}

// Add NFS recipient
static char* _ip = NULL;
static char* _path = NULL;
static char* _add_nfs_recp(char* line) {

    if (line == NULL) line = _path;

    size_t size = strlen(line);
    if (line[size-1] == '\n') line[--size] = '\0';
    size += strlen(_ip) + 2; // Account for additional ' ' char

    char* nline = (char *) malloc(size * sizeof(char));
    sprintf(nline, "%s %s\n", line, _ip);

    return nline;

}
int add_nfs_recp(char* path, char* recipient) {

    _path = path;
    _ip = recipient;

    if (fops_update_line(exportsFile, path, _add_nfs_recp))
        return error("Failed to update line!\n", NULL);

    return 0;
}

// Remove NFS recipient
int remove_nfs_recp(char* path, char* recipient) {

    char buffer[1024];

    // Get expected line
    if (fops_read_line((char*)exportsFile, path, buffer, 1023))
        return error("Failed to get the line from '%s'\n", (char *) exportsFile);

    // Remove the \n from the end
    size_t size = strlen(buffer);
    if (buffer[size-1] == '\n') buffer[--size] = '\0';

    // Copy tokens into a new buffer
    char line[1024];
    uint16_t aux = 0;
    char* token = strtok(buffer, " ");

    while (token != NULL) {

        printf("Token: %s\n", token);

        if (strcmp(token, recipient) != 0) {
            if (aux != 0) line[aux++] = ' ';
            strcpy(line + aux, token);
            aux += strlen(token);
        }

        token = strtok(NULL, " ");

    }

    line[aux] = '\0';

    // Remove the line
    if (fops_update_line(exportsFile, path, NULL))
        return error("Failed to remove the line from exports\n", NULL);

    // Append new line
    if (fops_append_line(exportsFile, line))
        return error("Failed to append the new line into exports\n", NULL);

    return 0;

}

// Mount an NFS view
int mount_nfs_dir(member* m) {

    char* command;
    asprintf(&command, "sudo mount -t nfs -o retrycnt=0 %s:%s/%d %d/", m->ip, m->prefix, m->id, m->id);
    printf("> %s\n", command);
    int result = system(command);
    free(command);

    return result;

}
