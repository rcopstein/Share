#define FUSE_USE_VERSION 26
#define _GNU_SOURCE

#include <fuse.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <sys/param.h>
#include <pthread.h>
#include <system.h>

#include "mount.h"
#include "members.h"
#include "hierarchy.h"
#include "output.h"
#include "protocol_freq.h"

#define G_PREFIX                       "org"
#define G_KAUTH_FILESEC_XATTR G_PREFIX ".apple.system.Security"
#define A_PREFIX                       "com"
#define A_KAUTH_FILESEC_XATTR A_PREFIX ".apple.system.Security"
#define XATTR_APPLE_PREFIX             "com.apple."

// Variables

uid_t mount_uid;
gid_t mount_gid;

// Functions
static int loopback_getattr(const char *path, struct stat *stbuf)
{
    LogicalFile* file = get_lf((char *) path);
    if (file == NULL) return -ENOENT;

    if (file->isDir) {

        stbuf->st_size = 0;
        stbuf->st_uid = mount_uid; // The owner of the file/directory is the user who mounted the filesystem
        stbuf->st_gid = mount_gid; // The group of the file/directory is the same as the group of the user who mounted the filesystem
        stbuf->st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
        stbuf->st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now

        stbuf->st_mode = S_IFDIR | 0755; // The owner of the directory can read and write to it
        stbuf->st_nlink = (nlink_t)(file->num_links + 2);

    }
    else {

        char* realpath = resolve_path(file);
        int result = lstat(realpath, stbuf);
        free(realpath);

        if (result == -1) return -errno;

#if FUSE_VERSION >= 29
        stbuf->st_blksize = 0;
#endif
    }

    return 0;
}

static int loopback_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    // TODO: Check if I can free the names!

    int* conflicts;
    LogicalFile** list = list_lf((char *) path, &conflicts);
    if (list == NULL) return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    int count = 0;
    LogicalFile* aux;

    while ((aux = list[count]) != NULL) {

        if (conflicts[count]) {

            char* name = resolved_name(aux);
            filler(buf, name, NULL, 0);
            //free(name);

        } else {

            char* name = (char *) malloc(sizeof(char) * (strlen(aux->name) + 1));
            strcpy(name, aux->name);

            filler(buf, name, NULL, 0);
            //free(name);

        }
        ++count;
    }

    free(conflicts);
    free(list);

    return 0;
}

static int loopback_mkdir(const char *path, mode_t mode)
{
    char* _path = (char *) malloc(strlen(path) + 1); // Copy path so we don't mess with params
    strcpy(_path, path);

    char* lastsep = strrchr(_path, '/'); // Find the last occurrence of '/'
    *lastsep = '\0';

    LogicalFile* lfolder = create_lf(true, lastsep + 1, "", "");
    int result = add_lf(lfolder, _path);

    free_lf(lfolder);
    free(_path);

    if (result) return -ENOENT;
    return 0;
}

static int loopback_rmdir(const char *path)
{
    return rem_lf((char *) path);
}

static int loopback_open(const char *path, struct fuse_file_info *fi)
{
    LogicalFile* file = get_lf((char *) path);
    if (file == NULL) return -ENOENT;

    char* realpath = resolve_path(file);
    int fd = open(realpath, fi->flags);
    free(realpath);

    if (fd == -1) return -errno;
    fi->fh = (uint64_t) fd;
    return 0;
}

static int loopback_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    ssize_t res = pread((int) fi->fh, buf, size, offset);
    if (res == -1) {
        res = -errno;
    }

    return (int) res;
}

static int loopback_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    ssize_t res = pwrite((int) fi->fh, buf, size, offset);
    if (res == -1) res = -errno;
    return (int) res;
}

static int loopback_rename(const char *from, const char *to)
{
    LogicalFile* lf = get_lf((char *) from); // Find File
    if (lf == NULL) return -ENOENT;

    member* owner = get_certain_member(lf->owner); // Find Owner
    if (owner == NULL || !(owner->state & AVAIL)) return -EINVAL;

    int res = send_freq_req_ren(owner, from, to); // Request owner to rename it

    return res;
}

static int loopback_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    char* _path = (char *) malloc(strlen(path) + 1); // Copy path so we don't mess with params
    strcpy(_path, path);

    char* _name;
    split_path(_path, &_name);

    // TODO: CHANGE THIS FOR SOMETHING SMARTER

    member* current = get_current_member();
    int res = send_freq_req_add(current, _path, _name, fi->flags);

    if (res > 0) { fi->fh = (uint64_t) res; res = 0; }

    free(_path);
    return res;
}

static int loopback_truncate(const char *path, off_t size)
{
    LogicalFile* file = get_lf((char *) path);
    if (file == NULL) return -ENOENT;

    char* realpath = resolve_path(file);
    int result = truncate(realpath, size);
    free(realpath);

    if (result < 0) return -errno;
    return result;
}

static int loopback_unlink(const char *path)
{
    LogicalFile* lf = get_lf((char *) path); // Find File
    if (lf == NULL) return -ENOENT;

    member* owner = get_certain_member(lf->owner); // Find Owner
    if (owner == NULL || !(owner->state & AVAIL)) return -EINVAL;

    int res = send_freq_req_del(owner, path); // Request owner to delete it

    return res;
}

static int loopback_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    int res;
    (void)path;

    res = fstat(fi->fh, stbuf);

#if FUSE_VERSION >= 29
    // Fall back to global I/O size. See loopback_getattr().
    stbuf->st_blksize = 0;
#endif

    if (res == -1) return -errno;
    return 0;
}

static int loopback_flush(const char *path, struct fuse_file_info *fi)
{
    int res;
    (void)path;

    res = close(dup(fi->fh));
    if (res == -1) return -errno;
    return 0;
}

static int loopback_release(const char *path, struct fuse_file_info *fi)
{
    (void)path;

    close(fi->fh);

    return 0;
}

static struct fuse_operations loopback_oper = {
        .getattr     = loopback_getattr,
        .fgetattr    = loopback_fgetattr,
        .readdir     = loopback_readdir,
        .mkdir       = loopback_mkdir,
        .unlink      = loopback_unlink,
        .rmdir       = loopback_rmdir,
        .rename      = loopback_rename,
        .create      = loopback_create,
        .open        = loopback_open,
        .read        = loopback_read,
        .write       = loopback_write,
        .flush       = loopback_flush,
        .release     = loopback_release,
        .truncate    = loopback_truncate,

#if FUSE_VERSION >= 29
#if HAVE_FSETATTR_X
.flag_nullpath_ok = 1,
        .flag_nopath = 1,
#else
        .flag_nullpath_ok = 0,
        .flag_nopath = 0,
#endif
#endif
};

// FUSE Session Management

static struct fuse* filesystem = NULL;
static pthread_t loop_thread = 0;
static char* mountpoint = NULL;

int mount_dir(char* mp) {

    mount_gid = getegid();
    mount_uid = geteuid();

    mountpoint = (char *) malloc(sizeof(char) * (strlen(mp) + 1));
    strcpy(mountpoint, mp);

    // Create Parameters
    int num = 3;
    int aux = num;
    char** list = malloc(sizeof(char*) * num);

    //char options[]    = "-o";
    char foreground[] = "-f";
    //char allowother[] = "allow_other";

    // First pointer is ignored, Second pointer is the path

    //list[--aux] = allowother;
    //list[--aux] = options;
    list[--aux] = foreground;
    list[--aux] = mountpoint;
    list[--aux] = mountpoint;

    // Attempt to initialize FUSE

    int mt;
    char* res_dir = NULL;
    filesystem = fuse_setup(num, list, &loopback_oper, sizeof(loopback_oper), &res_dir, &mt, NULL);

    return filesystem == NULL ? 1 : 0;
}
int unmount_dir() {
    if (filesystem == NULL) return error("Failed to retrieve FUSE struct!\n", NULL);
    fuse_teardown(filesystem, mountpoint);
    free(mountpoint);
    return 0;
}

static void* _mount_loop() {
    if (filesystem == NULL) { error("Failed to retrieve FUSE struct!\n", NULL); return NULL; }
    fuse_loop(filesystem);
    return NULL;
}
int mount_loop() {

    pthread_create(&loop_thread, NULL, _mount_loop, NULL);
    pthread_join(loop_thread, NULL);
    return 0;

}
