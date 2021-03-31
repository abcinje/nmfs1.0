#ifndef NMFS0_LEASE_UTIL_HPP
#define NMFS0_LEASE_UTIL_HPP

#include <sys/stat.h>
#include "../in_memory/dentry_table.hpp"
#include "../client/client.hpp"

shared_ptr<dentry_table> become_leader(ino_t directory_ino, shared_ptr<inode> dir_inode);
int retire_leader(ino_t directory_ino);
#endif //NMFS0_LEASE_UTIL_HPP
