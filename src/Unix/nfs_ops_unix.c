#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <system.h>

#include "fops.h"
#include "output.h"
#include "nfs_ops.h"
#include "members.h"

static const char exportsFile[] = "/etc/exports";
static const char  unmountCmd[] = "sudo umount %s:%s";
static const char    mountCmd[] = "sudo mount -t nfs %s:%s %s/";
static const char     options[] = "%s %s(rw,sync,no_subtree_check,insecure,all_squash,anonuid=%d,anongid=%d)\n";

// Set user permissions
static int num_chars(int num) {

    if (num == 0) return 1;

    int count = 0;
    while (num > 0) { num /= 10; ++count; }
    return count;

}
void set_root_perm(uid_t uid) {

    /* Not used for UNIX */

}

// Build NFS Path for member
static char* build_nfs_path(member* m, size_t *size) {

    *size = m->prefix_size + m->id_size + 2;
    char* path = (char *) malloc(*size);

    printf("The Prefix is: %s\n", m->prefix);
    printf("The ID is: %s\n", m->id);

    sprintf(path, "%s/%s", m->prefix, m->id);
    return path;
}

// Update NFS exports
int update_nfs() {
    return system("exportfs -ra");
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
        preline = (char *) malloc(strlen(_path) + 1);
        sprintf(preline, "%s", _path);
        line = preline;
    }

    size_t size = strlen(line);
    if (line[size-1] == '\n') line[--size] = '\0';

    size += num_chars(get_uid()) - 2; // UID minus placeholder
    size += num_chars(get_gid()) - 2; // GID minus placeholder
    size += strlen(_ip) - 2; // IP minus placeholder
    size += strlen(options); // Line Template
    size -= 2; // Minus Line Placeholder
    size += 1; // Line Terminator

    char* nline = (char *) malloc(size * sizeof(char));
    sprintf(nline, options, line, _ip, get_uid(), get_gid());

    if (preline != NULL) free(preline);
    return nline;

}
int add_nfs_recp(member* m, char* recipient) {

    size_t size;
    _path = build_nfs_path(m, &size);
    _ip = recipient;

    if (fops_update_line(exportsFile, _path, _add_nfs_recp)) return error("Failed to update line!\n", NULL);
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
    size_t rsize = strlen(_recp);
    char* token = strtok(line, " \n");

    while (token != NULL) {
        if (strncmp(token, _recp, rsize) != 0 || found) {
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
