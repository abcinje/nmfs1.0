#ifndef NMFS0_REMOTE_OPS_HPP
#define NMFS0_REMOTE_OPS_HPP

#include "../in_memory/directory_table.hpp"
#include "../meta/remote_inode.hpp"
#include "../meta/file_handler.hpp"
#include "../util.hpp"
#include "../rpc/rpc_client.hpp"

void remote_getattr(shared_ptr<remote_inode> i, struct stat* stat);
void remote_access(shared_ptr<remote_inode> i, int mask);
int remote_opendir(shared_ptr<remote_inode> i, struct fuse_file_info* file_info);
void remote_readdir(shared_ptr<remote_inode> i, void* buffer, fuse_fill_dir_t filler);
uuid remote_mkdir(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode);
int remote_rmdir_top(shared_ptr<remote_inode> target_i, uuid target_ino);
int remote_rmdir_down(shared_ptr<remote_inode> parent_i, uuid target_ino, std::string target_name);
int remote_symlink(shared_ptr<remote_inode> dst_parent_i, const char *src, const char *dst);
int remote_readlink(shared_ptr<remote_inode> i, char *buf, size_t size);
int remote_rename_same_parent(shared_ptr<remote_inode> parent_i, const char* old_path, const char* new_path, unsigned int flags);
uuid remote_rename_not_same_parent_src(shared_ptr<remote_inode> src_parent_i, const char* old_path, unsigned int flags);
int remote_rename_not_same_parent_dst(shared_ptr<remote_inode> dst_parent_i, uuid target_ino, uuid check_dst_ino, const char* new_path, unsigned int flags);
int remote_open(shared_ptr<remote_inode> i, struct fuse_file_info* file_info);
void remote_create(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info* file_info);
void remote_unlink(shared_ptr<remote_inode> parent_i, std::string child_name);
size_t remote_write(shared_ptr<remote_inode>i, const char* buffer, size_t size, off_t offset, int flags);
void remote_chmod(shared_ptr<remote_inode> i, mode_t mode);
void remote_chown(shared_ptr<remote_inode> i, uid_t uid, gid_t gid);
void remote_utimens(shared_ptr<remote_inode> i, const struct timespec tv[2]);
int remote_truncate (shared_ptr<remote_inode> i, off_t offset);

#endif //NMFS0_REMOTE_OPS_HPP
