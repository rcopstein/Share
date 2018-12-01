#include <AvailabilityMacros.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
#error "This file system requires Leopard and above."
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
#define HAVE_FSETATTR_X 0
#else
#define HAVE_FSETATTR_X 1
#endif

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
#include <sys/attr.h>
#include <sys/param.h>
#include <sys/vnode.h>
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
    LogicalFile* file;
    int err = _lf_get((char *) path, &file);
    if (err != 0) { /* error("Didn't find %s\n", (void *) path);*/ return -err; }
    // printf("Realpath for %s is %s\n", path, file->realpath);

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

        bool error = false;

        member* owner = get_certain_member(file->owner);
        if (owner != NULL && !member_get_state(owner, AVAIL)) error = true;
        else {

            char* realpath = resolve_path(file);
            int result = lstat(realpath, stbuf);

            if (result == -1) { error = true; }
            free(realpath);

        }

        if (error) {

            stbuf->st_size = 0;
            stbuf->st_uid = mount_uid; // The owner of the file/directory is the user who mounted the filesystem
            stbuf->st_gid = mount_gid; // The group of the file/directory is the same as the group of the user who mounted the filesystem
            stbuf->st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
            stbuf->st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now

            stbuf->st_mode = S_IFREG | 0444;
            stbuf->st_nlink = (nlink_t)(1);

        }

#if FUSE_VERSION >= 29
        stbuf->st_blksize = 0;
#endif
    }

    return 0;
}

static int loopback_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    (void) fi;
    (void) offset;

    int* conflicts;
    LogicalFile** list;
    int err = _lf_list(path, &list, &conflicts);
    if (err != 0) return -err;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    int count = 0;
    LogicalFile* aux;

    while ((aux = list[count]) != NULL) {

        if (conflicts[count]) {

            char* name = resolved_name(aux);
            filler(buf, name, NULL, 0);
            free(name);

        } else {

            char* name = (char *) malloc(sizeof(char) * (strlen(aux->name) + 1));
            strcpy(name, aux->name);

            filler(buf, name, NULL, 0);
            free(name);

        }
        ++count;
    }

    free(conflicts);
    free(list);

    return 0;
}

static int loopback_mkdir(const char *path, mode_t mode)
{
    (void) mode;

    char* _path = (char *) malloc(strlen(path) + 1); // Copy path so we don't mess with params
    strcpy(_path, path);

    char* lastsep = strrchr(_path, '/'); // Find the last occurrence of '/'
    *lastsep = '\0';

    LogicalFile* lfolder = create_lf(true, lastsep + 1, "", "");
    int result = _lf_add(lfolder, _path, false);

    free_lf(lfolder);
    free(_path);

    if (result) return -ENOENT;
    return 0;
}

/*
static int loopback_rmdir(const char *path)
{
    printf("Removing directory %s\n", path);
    return _lf_rem((char *) path, false) * -1;
}
*/

static int loopback_open(const char *path, struct fuse_file_info *fi)
{
    LogicalFile* file;
    int error = _lf_get((char *) path, &file);
    if (error != 0) return -error;

    char* realpath = resolve_path(file);
    int fd = open(realpath, fi->flags);
    free(realpath);

    if (fd == -1) return -errno;
    fi->fh = (uint64_t) fd;
    return 0;
}

static int loopback_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void) path;

    ssize_t res = pread((int) fi->fh, buf, size, offset);
    if (res == -1) {
        res = -errno;
    }

    return (int) res;
}

static int loopback_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void) path;

    ssize_t res = pwrite((int) fi->fh, buf, size, offset);
    if (res == -1) res = -errno;
    return (int) res;
}

static int loopback_rename(const char *from, const char *to)
{
    LogicalFile* lf;
    int error = _lf_get((char *) from, &lf); // Find File
    if (error != 0) return -error;

    member* owner = get_certain_member(lf->owner); // Find Owner
    if (owner == NULL || !(owner->state & AVAIL)) return -EINVAL;

    int res = send_freq_req_ren(owner, from, to); // Request owner to rename it

    return res;
}

static int loopback_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    // I assume that if I return an EEXIST error, the OS will attempt to getattr of it...
    // If it doesn't work, it will try again, leading to a loop until I can sync...

    (void) mode;

    char* _path = (char *) malloc(strlen(path) + 1); // Copy path so we don't mess with params
    strcpy(_path, path);

    char* _name;
    split_path(_path, &_name);

    member* assigned_owner = get_random_member();
    int res = send_freq_req_add(assigned_owner, _path, _name, fi->flags);

    if (res > 0) { fi->fh = (uint64_t) res; res = 0; }

    free(_path);
    return res;
}

static int loopback_truncate(const char *path, off_t size)
{
    LogicalFile* file;
    int error = _lf_get((char *) path, &file);
    if (error != 0) return -error;

    char* realpath = resolve_path(file);
    int result = truncate(realpath, size);
    free(realpath);

    if (result < 0) return -errno;
    return result;
}

static int loopback_unlink(const char *path)
{
    printf("Calling unlink for %s\n", path);

    LogicalFile* lf;
    int error = _lf_get((char *) path, &lf); // Find File
    if (error != 0) return -error;

    member* owner = get_certain_member(lf->owner); // Find Owner
    if (owner == NULL || !(owner->state & AVAIL)) return -EINVAL;

    int res = send_freq_req_del(owner, path); // Request owner to delete it

    return res;
}

static int loopback_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    int res;

    (void)path;

    res = fstat((int) fi->fh, stbuf);

#if FUSE_VERSION >= 29
    // Fall back to global I/O size. See loopback_getattr().
    stbuf->st_blksize = 0;
#endif

    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int loopback_flush(const char *path, struct fuse_file_info *fi)
{
    int res;

    (void)path;

    res = close(dup((int) fi->fh));
    if (res == -1) return -errno;
    return 0;
}

static int loopback_release(const char *path, struct fuse_file_info *fi)
{
    (void)path;

    close((int) fi->fh);

    return 0;
}

static int loopback_getxtimes(const char *npath, struct timespec *bkuptime, struct timespec *crtime)
{
    LogicalFile* lf;
    int error = _lf_get((char *) npath, &lf); // Find File
    if (error != 0) return -error;

    char* path = resolve_path(lf);

    int res = 0;
    struct attrlist attributes;

    attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
    attributes.reserved    = 0;
    attributes.commonattr  = 0;
    attributes.dirattr     = 0;
    attributes.fileattr    = 0;
    attributes.forkattr    = 0;
    attributes.volattr     = 0;

    struct xtimeattrbuf {
        uint32_t size;
        struct timespec xtime;
    } __attribute__ ((packed));

    struct xtimeattrbuf buf;

    attributes.commonattr = ATTR_CMN_BKUPTIME;
    res = getattrlist(path, &attributes, &buf, sizeof(buf), FSOPT_NOFOLLOW);
    if (res == 0) (void)memcpy(bkuptime, &(buf.xtime), sizeof(struct timespec));
    else (void)memset(bkuptime, 0, sizeof(struct timespec));

    attributes.commonattr = ATTR_CMN_CRTIME;
    res = getattrlist(path, &attributes, &buf, sizeof(buf), FSOPT_NOFOLLOW);
    if (res == 0) (void)memcpy(crtime, &(buf.xtime), sizeof(struct timespec));
    else (void)memset(crtime, 0, sizeof(struct timespec));

    free(path);
    return 0;
}

void *loopback_init(struct fuse_conn_info *conn)
{
    FUSE_ENABLE_SETVOLNAME(conn);
    // FUSE_ENABLE_XTIMES(conn);
    FUSE_ENABLE_CASE_INSENSITIVE(conn);

    return NULL;
}

void loopback_destroy(void *userdata)
{
    (void) userdata;

    /* nothing */
}

static struct fuse_operations loopback_oper = {
        .init        = loopback_init,
        .destroy     = loopback_destroy,
        .getxtimes   = loopback_getxtimes,
        .getattr     = loopback_getattr,
        .fgetattr    = loopback_fgetattr,
        .readdir     = loopback_readdir,
        .mkdir       = loopback_mkdir,
        .unlink      = loopback_unlink,
        //.rmdir       = loopback_rmdir,
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
    int num = 6;
    int aux = num;
    char** list = malloc(sizeof(char*) * num);

    char sync[]       = "-s";
    char options[]    = "-o";
    char foreground[] = "-f";
    char option_details[] = "volname=Shared\\ Folder,allow_other,noappledouble";

    // First pointer is ignored, Second pointer is the path

    list[--aux] = option_details;
    list[--aux] = options;
    list[--aux] = foreground;
    list[--aux] = sync;
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
