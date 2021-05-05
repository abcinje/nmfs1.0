#ifndef NMFS0_LEASE_UTIL_HPP
#define NMFS0_LEASE_UTIL_HPP

#include <sys/stat.h>
#include "../in_memory/dentry_table.hpp"
#include "../client/client.hpp"

shared_ptr<dentry_table> become_leader(ino_t ino);
shared_ptr<dentry_table> become_leader_of_new_dir(ino_t parnet_ino, ino_t ino);

#endif //NMFS0_LEASE_UTIL_HPP
