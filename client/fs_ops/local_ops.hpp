#ifndef NMFS0_LOCAL_OPS_HPP
#define NMFS0_LOCAL_OPS_HPP
#include "../in_memory/directory_table.hpp"
#include "../meta/file_handler.hpp"
#include "../journal/journal.hpp"
#include "../util.hpp"

void local_getattr(shared_ptr<inode> i, struct stat* stat);
void local_access(shared_ptr<inode> i, int mask);
int local_opendir(shared_ptr<inode> i, struct fuse_file_info* file_info);
int local_releasedir(shared_ptr<inode> i, struct fuse_file_info* file_info);
void local_readdir(shared_ptr<inode> i, void* buffer, fuse_fill_dir_t filler);
int local_mkdir(shared_ptr<inode> parent_i, std::string new_child_name, mode_t mode, std::shared_ptr<inode>& new_dir_inode, std::shared_ptr<dentry>& new_dir_dentry);
int local_rmdir_top(shared_ptr<inode> target_i, uuid target_ino);
int local_rmdir_down(shared_ptr<inode> parent_i, uuid target_ino, std::string target_name);
int local_symlink(shared_ptr<inode> dst_parent_i, const char *src, const char *dst);
int local_readlink(shared_ptr<inode> i, char *buf, size_t size);
int local_rename_same_parent(shared_ptr<inode> parent_i, const char* old_path, const char* new_path, unsigned int flags);
std::shared_ptr<inode> local_rename_not_same_parent_src(shared_ptr<inode> src_parent_i, const char* old_path, unsigned int flags);
int local_rename_not_same_parent_dst(shared_ptr<inode> dst_parent_i, std::shared_ptr<inode> target_inode, uuid check_dst_ino, const char* new_path, unsigned int flags);
int local_open(shared_ptr<inode> i, struct fuse_file_info* file_info);
int local_release(shared_ptr<inode> i, struct fuse_file_info* file_info);
void local_create(shared_ptr<inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info* file_info);
void local_unlink(shared_ptr<inode> parent_i, std::string child_name);
ssize_t local_read(shared_ptr<inode> i, char* buffer, size_t size, off_t offset);
ssize_t local_write(shared_ptr<inode> i, const char* buffer, size_t size, off_t offset, int flags);
void local_chmod(shared_ptr<inode> i, mode_t mode);
void local_chown(shared_ptr<inode> i, uid_t uid, gid_t gid);
void local_utimens(shared_ptr<inode> i, const struct timespec tv[2]);
int local_truncate (shared_ptr<inode> i, off_t offset);
#endif //NMFS0_LOCAL_OPS_HPP
