#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <output.h>

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

    fops_delete_line(exportsFile, buffer);

    return 0;

}

// Add NFS recipient
int add_nfs_recp(char* path, char* recipient) {

    char buffer[1024];

    // Get the expected line
    if (fops_get_line_starts_with((char *)exportsFile, path, buffer, 1023)) {
        // Create a new line
        sprintf(buffer, "%s %s\n", path, recipient);
    } else {
        // Remove the \n from the end
        size_t size = strlen(buffer);
        if (buffer[size-1] == '\n') buffer[--size] = '\0';

        // Append the recipient
        if (size + strlen(recipient) + 2 > 1023) return error("Not enough space to append recipient!\n", NULL);
        sprintf(buffer + size, " %s\n", recipient);
    }

    // Remove the line
    if (fops_delete_line_starts_with(exportsFile, path))
        return error("Failed to remove the line from exports\n", NULL);

    // Append new line
    if (fops_append_line(exportsFile, buffer))
        return error("Failed to append the new line into exports\n", NULL);

    return 0;
}

// Remove NFS recipient
int remove_nfs_recp(char* path, char* recipient) {

    char buffer[1024];

    // Get expected line
    if (fops_get_line_starts_with((char*)exportsFile, path, buffer, 1023))
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
    if (fops_delete_line_starts_with(exportsFile, path))
        return error("Failed to remove the line from exports\n", NULL);

    // Append new line
    if (fops_append_line(exportsFile, line))
        return error("Failed to append the new line into exports\n", NULL);

    return 0;

}
