#include <sys/types.h>
#include <unistd.h>

#include "system.h"

// Variables

static uid_t _uid = 0;
static gid_t _gid = 0;

// Methods

uid_t get_uid() {
    return _uid;
}
gid_t get_gid() {
    return _gid;
}

void set_uid(uid_t uid) {
    _uid = uid;
}
void set_gid(gid_t gid) {
    _gid = gid;
}

void become_user() {
    setegid(_gid);
    seteuid(_uid);
}
void become_root() {
    seteuid(0);
    setegid(0);
}
