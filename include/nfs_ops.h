#pragma once

#include "members.h"

// Update NFS exports
int update_nfs();

// Remove a NFS directory
int remove_nfs_dir(char* path);

// Add NFS recipient
int add_nfs_recp(member* m, char* recipient);

// Remove NFS recipient
int remove_nfs_recp(char* path, char* recipient);

// Mount an NFS view
int mount_nfs_dir(member* m);
