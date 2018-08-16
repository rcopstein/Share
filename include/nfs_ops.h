#pragma once

// Update NFS exports
int update_nfs();

// Remove a NFS directory
int remove_nfs_dir(char* path);

// Add NFS recipient
int add_nfs_recp(char* path, char* recipient);

// Remove NFS recipient
int remove_nfs_recp(char* path, char* recipient);
