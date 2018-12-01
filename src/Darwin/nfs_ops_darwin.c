#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "fops.h"
#include "output.h"
#include "nfs_ops.h"
#include "members.h"

static const char definitionOptions[] = "-fspath=/";
static const char       exportsFile[] = "/etc/exports";
static const char        unmountCmd[] = "sudo umount -f %s:%s";
static const char          mountCmd[] = "sudo mount -t nfs -o retrycnt=0,resvport %s:%s %s/";

static char* root_perm = NULL;

// Build NFS Path for member
static char* build_nfs_path(member* m, size_t *size) {

    *size = m->prefix_size + m->id_size + 2;
    char* path = (char *) malloc(*size);

    sprintf(path, "%s/%s", m->prefix, m->id);
    path[*size] = '\0';

    return path;
}

// Update NFS exports
int update_nfs() {
    return system("sudo nfsd update");
}

// Remove a NFS directory
int remove_nfs_dir(char* path) {
    fops_update_line(exportsFile, path, NULL);
    return 0;
}

// Add NFS recipient
static char* _ip = NULL;
static char* _path = NULL;
static char* _add_nfs_recp(char* line) {

    if (_ip == NULL || _path == NULL) return line;

    char* preline = NULL;

    if (line == NULL) {
        preline = (char *) malloc(strlen(_path) + strlen(definitionOptions) + strlen(root_perm) + 3);
        sprintf(preline, "%s %s %s", _path, root_perm, definitionOptions);
        line = preline;
    }

    size_t size = strlen(line);
    if (line[size-1] == '\n') line[--size] = '\0';
    size += strlen(_ip) + 2; // Account for additional ' ' char

    char* nline = (char *) malloc(size * sizeof(char));
    sprintf(nline, "%s %s\n", line, _ip);

    if (preline != NULL) free(preline);
    return nline;

}
int add_nfs_recp(member* m, char* recipient) {

    size_t size;
    _path = build_nfs_path(m, &size);
    _ip = recipient;

    int result = fops_update_line(exportsFile, _path, _add_nfs_recp);
    free(_path);

    if (result) return error("Failed to update line!\n", NULL);
    return update_nfs();
}

// Find NFS recipient
int check_nfs_recp(member* m, char* recipient) {

    size_t size;
    char* path = build_nfs_path(m, &size);

    char* buffer = (char *) malloc(sizeof(char) * 256); // Don't worry about it being small, it will get realloced if it doesn't fit
    int res = fops_read_line(exportsFile, path, buffer, 256);

    if (res) {
        printf("Didn't find line starting with %s\n", path);
        free(buffer);
        return 1;
    }

    bool found = false;
    char* aux = buffer;
    char* token = strsep(&aux, " ");

    while (token != NULL) {
        if (!strncmp(token, recipient, strlen(recipient))) { found = true; break; }
        token = strsep(&aux, " ");
    }

    printf("%s, Found: %d\n", token, found);

    free(buffer);
    if (found) return 0;
    return 1;
}

// Remove NFS recipient
static char* _recp = NULL;
static char* _remove_nfs_recp(char* line) {

    if (line == NULL) return NULL;
    if (_recp == NULL) return line;

    bool found = false;
    size_t size = strlen(line) + 1;
    char *nline = (char *) malloc(size * sizeof(char));

    char* aux = nline;
    uint16_t count = 0;
    char* token = strtok(line, " \n");

    while (token != NULL) {
        if (strcmp(token, _recp) != 0 || found) {
            sprintf(aux, "%s ", token);
            aux += strlen(token) + 1;
            ++count;
        } else found = true;
        token = strtok(NULL, " \n");
    }

    if (count <= 3) {
        free(nline);
        return NULL;
    }

    *(aux - 1) = '\n';
    return nline;
}
int remove_nfs_recp(member* m, char* recipient) {

    size_t size;
    char* path = build_nfs_path(m, &size);
    _recp = recipient;

    int result = fops_update_line(exportsFile, path, _remove_nfs_recp);
    free(path);

    if (result) return error("Failed to update line!\n", NULL);
    return update_nfs();
}

// Mount an NFS view
int mount_nfs_dir(member* m) {

    size_t size;
    char* path = build_nfs_path(m, &size);

    char* command = (char *) malloc(size + strlen(mountCmd) + m->id_size);
    sprintf(command, mountCmd, m->ip, path, m->id);
    printf("> %s\n", command);

    int result = 0; // system(command);

    free(command);
    free(path);

    return result;
}
int unmount_nfs_dir(member* m) {

    size_t size;
    char* path = build_nfs_path(m, &size);

    size += strlen(unmountCmd) - 2; // Remove placeholder
    size += strlen(m->ip) - 2; // Remove Placeholder
    size += 1; // Terminator

    char* command = (char *) malloc(size);
    sprintf(command, unmountCmd, m->ip, path);
    command[size] = '\0';

    printf("> %s\n", command);

    int result = 0; //system(command);

    free(command);
    free(path);

    return result;

}

// Set user permissions
static int num_chars(int num) {

    if (num == 0) return 1;

    int count = 0;
    while (num > 0) { num /= 10; ++count; }
    return count;

}
void set_root_perm(uid_t uid) {

    root_perm = (char *) malloc(sizeof(char) * (num_chars(uid) + 10));
    sprintf(root_perm, "-maproot=%d", uid);

}
