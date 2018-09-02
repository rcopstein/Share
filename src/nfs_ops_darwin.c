#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fops.h"
#include "output.h"
#include "nfs_ops.h"
#include "members.h"

static const char exportsFile[] = "/etc/exports";
static const char unmountCmd[] = "sudo umount %s:%s";
static const char mountCmd[] = "sudo mount -t nfs -o retrycnt=0,resvport %s:%s %s/";

// Build NFS Path for member
static char* build_nfs_path(member* m, size_t *size) {

    *size = m->prefix_size + m->id_size + 2;
    char* path = (char *) malloc(*size);

    sprintf(path, "%s/%s", m->prefix, m->id);
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

    if (line == NULL) line = _path;

    size_t size = strlen(line);
    if (line[size-1] == '\n') line[--size] = '\0';
    size += strlen(_ip) + 2; // Account for additional ' ' char

    char* nline = (char *) malloc(size * sizeof(char));
    sprintf(nline, "%s %s\n", line, _ip);

    return nline;

}
int add_nfs_recp(member* m, char* recipient) {

    size_t size;
    _path = build_nfs_path(m, &size);
    _ip = recipient;

    if (fops_update_line(exportsFile, _path, _add_nfs_recp))
        return error("Failed to update line!\n", NULL);

    return update_nfs();
}

// Remove NFS recipient
static char* _recp = NULL;
static char* _remove_nfs_recp(char* line) {

    if (line == NULL) return NULL;
    if (_recp == NULL) return line;

    size_t size = strlen(line) + 1;
    char *nline = (char *) malloc(size * sizeof(char));

    char* aux = nline;
    uint16_t count = 0;
    char* token = strtok(line, " \n");

    while (token != NULL) {
        if (strcmp(token, _recp) != 0) {
            sprintf(aux, "%s ", token);
            aux += strlen(token) + 1;
            ++count;
        }
        token = strtok(NULL, " \n");
    }

    if (count <= 1) {
        free(nline);
        return NULL;
    }

    *(aux - 1) = '\n';
    return nline;
}
int remove_nfs_recp(char* path, char* recipient) {

    _recp = recipient;

    if (fops_update_line(exportsFile, path, _remove_nfs_recp))
        return error("Failed to update line!\n", NULL);

    return update_nfs();
}

// Mount an NFS view
int mount_nfs_dir(member* m) {

    size_t size;
    char* path = build_nfs_path(m, &size);

    char* command = (char *) malloc(size + strlen(mountCmd) + m->id_size);
    sprintf(command, mountCmd, m->ip, path, m->id);
    printf("> %s\n", command);

    int result = system(command);

    free(command);
    free(path);

    return result;

}
int unmount_nfs_dir(member* m) {

    size_t size;
    char* path = build_nfs_path(m, &size);

    char* command = (char *) malloc(size + strlen(unmountCmd) + 15);
    sprintf(command, unmountCmd, m->ip, path);
    printf("> %s\n", command);

    int result = system(command);

    free(command);
    free(path);

    return result;

}
