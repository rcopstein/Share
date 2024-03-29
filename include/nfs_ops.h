#pragma once

#include "members.h"

// Update NFS exports
int update_nfs();

// Remove a NFS directory
int remove_nfs_dir(char* path);

// Add/Remove NFS recipient
int add_nfs_recp(member* m, char* recipient);
int check_nfs_recp(member* m, char* recipient);
int remove_nfs_recp(member* m, char* recipient);

// Mount/Unmount an NFS view
int mount_nfs_dir(member* m);
int unmount_nfs_dir(member* m);

// Set user permissions
void set_root_perm(uid_t uid);
