#ifndef NMFS0_REMOTE_OPS_HPP
#define NMFS0_REMOTE_OPS_HPP

#include "util/path.hpp"

#include "../in_memory/directory_table.hpp"
#include "../meta/remote_inode.hpp"
#include "../meta/file_handler.hpp"
#include "../rpc/rpc_client.hpp"

int remote_getattr(shared_ptr<remote_inode> i, struct stat* stat);
int remote_access(shared_ptr<remote_inode> i, int mask);
int remote_opendir(shared_ptr<remote_inode> i, struct fuse_file_info* file_info);
int remote_readdir(shared_ptr<remote_inode> i, void* buffer, fuse_fill_dir_t filler);
int remote_mkdir(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, std::shared_ptr<inode>& new_dir_inode, std::shared_ptr<dentry>& new_dir_dentry);
int remote_rmdir_top(shared_ptr<remote_inode> target_i, uuid target_ino);
int remote_rmdir_down(shared_ptr<remote_inode> parent_i, uuid target_ino, std::string target_name);
int remote_symlink(shared_ptr<remote_inode> dst_parent_i, const char *src, const char *dst);
int remote_readlink(shared_ptr<remote_inode> i, char *buf, size_t size);
int remote_rename_same_parent(shared_ptr<remote_inode> parent_i, const char* old_path, const char* new_path, unsigned int flags);
int remote_rename_not_same_parent_src(shared_ptr<remote_inode> src_parent_i, const char* old_path, unsigned int flags, std::shared_ptr<inode>& target_inode);
int remote_rename_not_same_parent_dst(shared_ptr<remote_inode> dst_parent_i, std::shared_ptr<inode>& target_inode, uuid check_dst_ino, const char* new_path, unsigned int flags);
int remote_open(shared_ptr<remote_inode> i, struct fuse_file_info* file_info);
int remote_create(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info* file_info);
int remote_unlink(shared_ptr<remote_inode> parent_i, std::string child_name);
ssize_t remote_write(shared_ptr<remote_inode>i, const char* buffer, size_t size, off_t offset, int flags);
int remote_chmod(shared_ptr<remote_inode> i, mode_t mode);
int remote_chown(shared_ptr<remote_inode> i, uid_t uid, gid_t gid);
int remote_utimens(shared_ptr<remote_inode> i, const struct timespec tv[2]);
int remote_truncate (shared_ptr<remote_inode> i, off_t offset);

#endif //NMFS0_REMOTE_OPS_HPP
