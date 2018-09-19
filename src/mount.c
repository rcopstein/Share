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

#include "mount.h"
#include "members.h"
#include "hierarchy.h"

#if defined(_POSIX_C_SOURCE)
typedef unsigned char  u_char;
typedef unsigned short u_short;
typedef unsigned int   u_int;
typedef unsigned long  u_long;
#endif

#define G_PREFIX                       "org"
#define G_KAUTH_FILESEC_XATTR G_PREFIX ".apple.system.Security"
#define A_PREFIX                       "com"
#define A_KAUTH_FILESEC_XATTR A_PREFIX ".apple.system.Security"
#define XATTR_APPLE_PREFIX             "com.apple."

// Structs
struct loopback
{
    int case_insensitive;
};

struct loopback_dirp
{
    DIR *dp;
    struct dirent *entry;
    off_t offset;
};

// Variables
static struct loopback loopback;
char buffer[256];

uid_t mount_uid;
gid_t mount_gid;

// Functions
static int loopback_getattr(const char *path, struct stat *stbuf)
{
    LogicalFile* file = get_logical_file((char *) path);
    if (file == NULL)
        return -ENOENT;

    if (file->isDir) {

        stbuf->st_uid = mount_uid; // The owner of the file/directory is the user who mounted the filesystem
        stbuf->st_gid = mount_gid; // The group of the file/directory is the same as the group of the user who mounted the filesystem
        stbuf->st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
        stbuf->st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now

        stbuf->st_mode = S_IFDIR | 0755; // The owner of the directory can read and write to it
        stbuf->st_nlink = (nlink_t)(file->num_links + 2);

    }
    else {

        if (lstat(file->realpath, stbuf) == -1) return -errno;

#if FUSE_VERSION >= 29
        stbuf->st_blksize = 0;
#endif
    }

    return 0;
}

static inline struct loopback_dirp *get_dirp(struct fuse_file_info *fi)
{
    return (struct loopback_dirp *)(uintptr_t)fi->fh;
}

static int loopback_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    LogicalFile** list = list_logical_files((char *) path);
    if (list == NULL) return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    int count = 0;
    LogicalFile* aux;

    while ((aux = list[count++]) != NULL) filler(buf, aux->name, NULL, 0);
    return 0;

    struct loopback_dirp *d = get_dirp(fi);
    if (d == NULL) return -ENOENT;

    (void)path;

    if (offset != d->offset) {
        seekdir(d->dp, offset);
        d->entry = NULL;
        d->offset = offset;
    }

    while (1) {
        struct stat st;
        off_t nextoff;

        if (!d->entry) {
            d->entry = readdir(d->dp);
            if (!d->entry) {
                break;
            }
        }

        memset(&st, 0, sizeof(st));
        st.st_ino = d->entry->d_ino;
        st.st_mode = d->entry->d_type << 12;
        nextoff = telldir(d->dp);
        if (filler(buf, d->entry->d_name, &st, nextoff)) {
            break;
        }

        d->entry = NULL;
        d->offset = nextoff;
    }

    return 0;
}

static int loopback_mkdir(const char *path, mode_t mode)
{
    char* _path = (char *) malloc(strlen(path) + 1); // Copy path to we don't mess with params
    strcpy(_path, path);

    char* lastsep = strrchr(_path, '/'); // Find the last occurrence of '/'
    *lastsep = '\0';

    LogicalFile* lfolder = create_logical_file(true, lastsep + 1, get_current_member()->id, "");
    int result = add_logical_file(_path, lfolder);

    free_logical_file(lfolder);
    free(_path);

    if (result) return -ENOENT;
    return 0;
}

static int loopback_rmdir(const char *path)
{
    return rem_logical_file((char *) path);
}

static int loopback_open(const char *path, struct fuse_file_info *fi)
{
    LogicalFile* file = get_logical_file((char *) path);
    if (file == NULL) return -ENOENT;

    int fd = open(file->realpath, fi->flags);
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
    char* _from_path = (char *) malloc(strlen(from) + 1); // Copy path to we don't mess with params
    strcpy(_from_path, from);

    char* _to_path = (char *) malloc(strlen(to) + 1); // Copy path to we don't mess with params
    strcpy(_to_path, to);

    char* _from_name = strrchr(_from_path, '/'); // Find the last occurrence of '/'
    *_from_name = '\0';
    _from_name++;

    char* _to_name = strrchr(_to_path, '/'); // Find the last occurrence of '/'
    *_to_name = '\0';
    _to_name++;

    int result;
    printf("Renamed paths %s %s\n", _from_path, _to_path);
    printf("From name %s to %s\n", _from_name, _to_name);

    if (strcmp(_to_path, _from_path) == 0) {

        LogicalFile* file = get_logical_file((char *) from);
        if (file == NULL) result = -ENOENT;
        else {
            free(file->name);
            file->name = (char *) malloc(sizeof(char) * (strlen(_to_name) + 1));
            strcpy(file->name, _to_name);
            result = 0;
        }

    } else result = -EPERM;

    free(_from_path);
    free(_to_path);

    return result;
}





static int
loopback_fgetattr(const char *path, struct stat *stbuf,
                  struct fuse_file_info *fi)
{
    int res;

    (void)path;

    res = fstat(fi->fh, stbuf);

#if FUSE_VERSION >= 29
    // Fall back to global I/O size. See loopback_getattr().
    stbuf->st_blksize = 0;
#endif

    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int
loopback_readlink(const char *path, char *buf, size_t size)
{
    ssize_t res = readlink(path, buf, size - 1);
    if (res == -1) return -errno;
    buf[res] = '\0';
    return 0;
}

static int
loopback_opendir(const char *path, struct fuse_file_info *fi)
{
    int res;

    struct loopback_dirp *d = malloc(sizeof(struct loopback_dirp));
    if (d == NULL) {
        return -ENOMEM;
    }

    d->dp = opendir(path);
    if (d->dp == NULL) {
        res = -errno;
        free(d);
        return res;
    }

    d->offset = 0;
    d->entry = NULL;

    fi->fh = (unsigned long)d;

    return 0;
}

static int
loopback_releasedir(const char *path, struct fuse_file_info *fi)
{
    struct loopback_dirp *d = get_dirp(fi);

    (void)path;

    closedir(d->dp);
    free(d);

    return 0;
}

static int
loopback_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;

    if (S_ISFIFO(mode)) {
        res = mkfifo(path, mode);
    } else {
        res = mknod(path, mode, rdev);
    }

    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int
loopback_unlink(const char *path)
{
    int res;

    res = unlink(path);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int
loopback_symlink(const char *from, const char *to)
{
    int res;

    res = symlink(from, to);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int
loopback_exchange(const char *path1, const char *path2, unsigned long options)
{
    int res;

    res = exchangedata(path1, path2, options);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int
loopback_link(const char *from, const char *to)
{
    int res;

    res = link(from, to);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

#if HAVE_FSETATTR_X

static int
loopback_fsetattr_x(const char *path, struct setattr_x *attr,
                    struct fuse_file_info *fi)
{
    int res;
    uid_t uid = -1;
    gid_t gid = -1;

    if (SETATTR_WANTS_MODE(attr)) {
        res = fchmod(fi->fh, attr->mode);
        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_UID(attr)) {
        uid = attr->uid;
    }

    if (SETATTR_WANTS_GID(attr)) {
        gid = attr->gid;
    }

    if ((uid != -1) || (gid != -1)) {
        res = fchown(fi->fh, uid, gid);
        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_SIZE(attr)) {
        res = ftruncate(fi->fh, attr->size);
        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_MODTIME(attr)) {
        struct timeval tv[2];
        if (!SETATTR_WANTS_ACCTIME(attr)) {
            gettimeofday(&tv[0], NULL);
        } else {
            tv[0].tv_sec = attr->acctime.tv_sec;
            tv[0].tv_usec = attr->acctime.tv_nsec / 1000;
        }
        tv[1].tv_sec = attr->modtime.tv_sec;
        tv[1].tv_usec = attr->modtime.tv_nsec / 1000;
        res = futimes(fi->fh, tv);
        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_CRTIME(attr)) {
        struct attrlist attributes;

        attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
        attributes.reserved = 0;
        attributes.commonattr = ATTR_CMN_CRTIME;
        attributes.dirattr = 0;
        attributes.fileattr = 0;
        attributes.forkattr = 0;
        attributes.volattr = 0;

        res = fsetattrlist(fi->fh, &attributes, &attr->crtime,
                           sizeof(struct timespec), FSOPT_NOFOLLOW);

        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_CHGTIME(attr)) {
        struct attrlist attributes;

        attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
        attributes.reserved = 0;
        attributes.commonattr = ATTR_CMN_CHGTIME;
        attributes.dirattr = 0;
        attributes.fileattr = 0;
        attributes.forkattr = 0;
        attributes.volattr = 0;

        res = fsetattrlist(fi->fh, &attributes, &attr->chgtime,
                           sizeof(struct timespec), FSOPT_NOFOLLOW);

        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_BKUPTIME(attr)) {
        struct attrlist attributes;

        attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
        attributes.reserved = 0;
        attributes.commonattr = ATTR_CMN_BKUPTIME;
        attributes.dirattr = 0;
        attributes.fileattr = 0;
        attributes.forkattr = 0;
        attributes.volattr = 0;

        res = fsetattrlist(fi->fh, &attributes, &attr->bkuptime,
                           sizeof(struct timespec), FSOPT_NOFOLLOW);

        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_FLAGS(attr)) {
        res = fchflags(fi->fh, attr->flags);
        if (res == -1) {
            return -errno;
        }
    }

    return 0;
}

#endif /* HAVE_FSETATTR_X */

static int
loopback_setattr_x(const char *path, struct setattr_x *attr)
{
    int res;
    uid_t uid = -1;
    gid_t gid = -1;

    if (SETATTR_WANTS_MODE(attr)) {
        res = lchmod(path, attr->mode);
        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_UID(attr)) {
        uid = attr->uid;
    }

    if (SETATTR_WANTS_GID(attr)) {
        gid = attr->gid;
    }

    if ((uid != -1) || (gid != -1)) {
        res = lchown(path, uid, gid);
        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_SIZE(attr)) {
        res = truncate(path, attr->size);
        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_MODTIME(attr)) {
        struct timeval tv[2];
        if (!SETATTR_WANTS_ACCTIME(attr)) {
            gettimeofday(&tv[0], NULL);
        } else {
            tv[0].tv_sec = attr->acctime.tv_sec;
            tv[0].tv_usec = attr->acctime.tv_nsec / 1000;
        }
        tv[1].tv_sec = attr->modtime.tv_sec;
        tv[1].tv_usec = attr->modtime.tv_nsec / 1000;
        res = lutimes(path, tv);
        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_CRTIME(attr)) {
        struct attrlist attributes;

        attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
        attributes.reserved = 0;
        attributes.commonattr = ATTR_CMN_CRTIME;
        attributes.dirattr = 0;
        attributes.fileattr = 0;
        attributes.forkattr = 0;
        attributes.volattr = 0;

        res = setattrlist(path, &attributes, &attr->crtime,
                          sizeof(struct timespec), FSOPT_NOFOLLOW);

        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_CHGTIME(attr)) {
        struct attrlist attributes;

        attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
        attributes.reserved = 0;
        attributes.commonattr = ATTR_CMN_CHGTIME;
        attributes.dirattr = 0;
        attributes.fileattr = 0;
        attributes.forkattr = 0;
        attributes.volattr = 0;

        res = setattrlist(path, &attributes, &attr->chgtime,
                          sizeof(struct timespec), FSOPT_NOFOLLOW);

        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_BKUPTIME(attr)) {
        struct attrlist attributes;

        attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
        attributes.reserved = 0;
        attributes.commonattr = ATTR_CMN_BKUPTIME;
        attributes.dirattr = 0;
        attributes.fileattr = 0;
        attributes.forkattr = 0;
        attributes.volattr = 0;

        res = setattrlist(path, &attributes, &attr->bkuptime,
                          sizeof(struct timespec), FSOPT_NOFOLLOW);

        if (res == -1) {
            return -errno;
        }
    }

    if (SETATTR_WANTS_FLAGS(attr)) {
        res = lchflags(path, attr->flags);
        if (res == -1) {
            return -errno;
        }
    }

    return 0;
}

static int
loopback_getxtimes(const char *path, struct timespec *bkuptime,
                   struct timespec *crtime)
{
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
    if (res == 0) {
        (void)memcpy(bkuptime, &(buf.xtime), sizeof(struct timespec));
    } else {
        (void)memset(bkuptime, 0, sizeof(struct timespec));
    }

    attributes.commonattr = ATTR_CMN_CRTIME;
    res = getattrlist(path, &attributes, &buf, sizeof(buf), FSOPT_NOFOLLOW);
    if (res == 0) {
        (void)memcpy(crtime, &(buf.xtime), sizeof(struct timespec));
    } else {
        (void)memset(crtime, 0, sizeof(struct timespec));
    }

    return 0;
}

static int
loopback_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int fd;

    fd = open(path, fi->flags, mode);
    if (fd == -1) {
        return -errno;
    }

    fi->fh = fd;
    return 0;
}

static int
loopback_statfs(const char *path, struct statvfs *stbuf)
{
    int res;

    res = statvfs(path, stbuf);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int
loopback_flush(const char *path, struct fuse_file_info *fi)
{
    int res;

    (void)path;

    res = close(dup(fi->fh));
    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int
loopback_release(const char *path, struct fuse_file_info *fi)
{
    (void)path;

    close(fi->fh);

    return 0;
}

static int
loopback_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
    int res;

    (void)path;

    (void)isdatasync;

    res = fsync(fi->fh);
    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int
loopback_setxattr(const char *path, const char *name, const char *value,
                  size_t size, int flags, uint32_t position)
{
    int res;

    if (!strncmp(name, XATTR_APPLE_PREFIX, sizeof(XATTR_APPLE_PREFIX) - 1)) {
        flags &= ~(XATTR_NOSECURITY);
    }

    if (!strcmp(name, A_KAUTH_FILESEC_XATTR)) {

        char new_name[MAXPATHLEN];

        memcpy(new_name, A_KAUTH_FILESEC_XATTR, sizeof(A_KAUTH_FILESEC_XATTR));
        memcpy(new_name, G_PREFIX, sizeof(G_PREFIX) - 1);

        res = setxattr(path, new_name, value, size, position, XATTR_NOFOLLOW);

    } else {
        res = setxattr(path, name, value, size, position, XATTR_NOFOLLOW);
    }

    if (res == -1) {
        return -errno;
    }

    return 0;
}

static int
loopback_getxattr(const char *path, const char *name, char *value, size_t size,
                  uint32_t position)
{
    int res;

    if (strcmp(name, A_KAUTH_FILESEC_XATTR) == 0) {

        char new_name[MAXPATHLEN];

        memcpy(new_name, A_KAUTH_FILESEC_XATTR, sizeof(A_KAUTH_FILESEC_XATTR));
        memcpy(new_name, G_PREFIX, sizeof(G_PREFIX) - 1);

        res = getxattr(path, new_name, value, size, position, XATTR_NOFOLLOW);

    } else {
        res = getxattr(path, name, value, size, position, XATTR_NOFOLLOW);
    }

    if (res == -1) {
        return -errno;
    }

    return res;
}

static int
loopback_listxattr(const char *path, char *list, size_t size)
{
    ssize_t res = listxattr(path, list, size, XATTR_NOFOLLOW);
    if (res > 0) {
        if (list) {
            size_t len = 0;
            char *curr = list;
            do {
                size_t thislen = strlen(curr) + 1;
                if (strcmp(curr, G_KAUTH_FILESEC_XATTR) == 0) {
                    memmove(curr, curr + thislen, res - len - thislen);
                    res -= thislen;
                    break;
                }
                curr += thislen;
                len += thislen;
            } while (len < res);
        } else {
            /*
            ssize_t res2 = getxattr(path, G_KAUTH_FILESEC_XATTR, NULL, 0, 0,
                                    XATTR_NOFOLLOW);
            if (res2 >= 0) {
                res -= sizeof(G_KAUTH_FILESEC_XATTR);
            }
            */
        }
    }

    if (res == -1) {
        return -errno;
    }

    return res;
}

static int
loopback_removexattr(const char *path, const char *name)
{
    int res;

    if (strcmp(name, A_KAUTH_FILESEC_XATTR) == 0) {

        char new_name[MAXPATHLEN];

        memcpy(new_name, A_KAUTH_FILESEC_XATTR, sizeof(A_KAUTH_FILESEC_XATTR));
        memcpy(new_name, G_PREFIX, sizeof(G_PREFIX) - 1);

        res = removexattr(path, new_name, XATTR_NOFOLLOW);

    } else {
        res = removexattr(path, name, XATTR_NOFOLLOW);
    }

    if (res == -1) {
        return -errno;
    }

    return 0;
}

#if FUSE_VERSION >= 29

static int
loopback_fallocate(const char *path, int mode, off_t offset, off_t length,
                   struct fuse_file_info *fi)
{
    fstore_t fstore;

    if (!(mode & PREALLOCATE)) {
        return -ENOTSUP;
    }

    fstore.fst_flags = 0;
    if (mode & ALLOCATECONTIG) {
        fstore.fst_flags |= F_ALLOCATECONTIG;
    }
    if (mode & ALLOCATEALL) {
        fstore.fst_flags |= F_ALLOCATEALL;
    }

    if (mode & ALLOCATEFROMPEOF) {
        fstore.fst_posmode = F_PEOFPOSMODE;
    } else if (mode & ALLOCATEFROMVOL) {
        fstore.fst_posmode = F_VOLPOSMODE;
    }

    fstore.fst_offset = offset;
    fstore.fst_length = length;

    if (fcntl(fi->fh, F_PREALLOCATE, &fstore) == -1) {
        return -errno;
    } else {
        return 0;
    }
}

#endif /* FUSE_VERSION >= 29 */

static int
loopback_setvolname(const char *name)
{
    return 0;
}

void *
loopback_init(struct fuse_conn_info *conn)
{
    FUSE_ENABLE_SETVOLNAME(conn);
    FUSE_ENABLE_XTIMES(conn);

#ifdef FUSE_ENABLE_CASE_INSENSITIVE
    if (loopback.case_insensitive) {
        FUSE_ENABLE_CASE_INSENSITIVE(conn);
    }
#endif

    return NULL;
}

void
loopback_destroy(void *userdata)
{
    /* nothing */
}

static struct fuse_operations loopback_oper = {
        .init        = loopback_init,
        .destroy     = loopback_destroy,
        .getattr     = loopback_getattr,
        .fgetattr    = loopback_fgetattr,
        .readdir     = loopback_readdir,
        .mknod       = loopback_mknod,
        .mkdir       = loopback_mkdir,
        .unlink      = loopback_unlink,
        .rmdir       = loopback_rmdir,
        .rename      = loopback_rename,
        .link        = loopback_link,
        .create      = loopback_create,
        .open        = loopback_open,
        .read        = loopback_read,
        .write       = loopback_write,
        .statfs      = loopback_statfs,
        .flush       = loopback_flush,
        .release     = loopback_release,
        //.readlink    = loopback_readlink,
        //.opendir     = loopback_opendir,
        //.releasedir  = loopback_releasedir,
        //.symlink     = loopback_symlink,
        //.fsync       = loopback_fsync,
        //.setxattr    = loopback_setxattr,
        //.getxattr    = loopback_getxattr,
        //.listxattr   = loopback_listxattr,
        //.removexattr = loopback_removexattr,
        //.exchange    = loopback_exchange,
        //.getxtimes   = loopback_getxtimes,
        //.setattr_x   = loopback_setattr_x,
        //.setvolname  = loopback_setvolname,

#if HAVE_FSETATTR_X
        //.fsetattr_x  = loopback_fsetattr_x,
#endif

#if FUSE_VERSION >= 29
        //.fallocate   = loopback_fallocate,
#endif

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

static const struct fuse_opt loopback_opts[] = {
        { "case_insensitive", offsetof(struct loopback, case_insensitive), 1 },
        FUSE_OPT_END
};

void mount_dir(int argc, char *argv[])
{
    LogicalFile* folder = create_logical_file(true, "Potatoes", "1", "/Users/rcopstein/Desktop/s1");
    LogicalFile* file = create_logical_file(false, "potato.xlsx", "1", "/Users/rcopstein/Desktop/Notas.xlsx");

    add_logical_file("/", folder);
    add_logical_file("/Potatoes/", file);

    int res;
    mount_gid = 20;
    mount_uid = 501;

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

    loopback.case_insensitive = 0;
    if (fuse_opt_parse(&args, &loopback, loopback_opts, NULL) == -1) exit(1);
    umask(0);

    res = fuse_main(args.argc, args.argv, &loopback_oper, NULL);
    fuse_opt_free_args(&args);
}
