#pragma once

#include <ntsid.h>

uid_t get_uid();
gid_t get_gid();

void set_uid(uid_t uid);
void set_gid(gid_t gid);

void become_user();
void become_root();
