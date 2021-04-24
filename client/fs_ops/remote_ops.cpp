#include "remote_ops.hpp"

void remote_getattr(shared_ptr<remote_inode> i, struct stat* stat) {

}

void remote_access(shared_ptr<remote_inode> i, int mask) {

}

int remote_opendir(shared_ptr<remote_inode> i, struct fuse_file_info* file_info) {
	return -ENOSYS;
}

void remote_readdir(shared_ptr<remote_inode> i, void* buffer, fuse_fill_dir_t filler) {

}

void remote_mkdir(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode) {

}

int remote_rmdir(shared_ptr<remote_inode> parent_i, shared_ptr<remote_inode> target_i, std::string target_name) {
	return -ENOSYS;
}

int remote_symlink(shared_ptr<remote_inode> dst_parent_i, const char *src, const char *dst) {
	return -ENOSYS;
}

int remote_readlink(shared_ptr<remote_inode> i, char *buf, size_t size) {
	return -ENOSYS;
}

int remote_rename_same_parent(shared_ptr<remote_inode> parent_i, const char* old_path, const char* new_path, unsigned int flags) {
	return -ENOSYS;
}

int remote_rename_not_same_parent(shared_ptr<remote_inode> src_parent_i, shared_ptr<remote_inode> dst_parent_i, const char* old_path, const char* new_path, unsigned int flags) {
	return -ENOSYS;
}

int remote_open(shared_ptr<remote_inode> i, struct fuse_file_info* file_info) {
	return -ENOSYS;
}

void remote_create(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info* file_info) {

}

void remote_unlink(shared_ptr<remote_inode> parent_i, std::string child_name) {

}

size_t remote_read(shared_ptr<remote_inode> i, char* buffer, size_t size, off_t offset) {
	return -ENOSYS;
}

size_t remote_write(shared_ptr<remote_inode> i, const char* buffer, size_t size, off_t offset, int flags) {
	return -ENOSYS;
}

void remote_chmod(shared_ptr<remote_inode> i, mode_t mode) {

}

void remote_chown(shared_ptr<remote_inode> i, uid_t uid, gid_t gid) {

}

void remote_utimens(shared_ptr<remote_inode> i, const struct timespec tv[2]) {

}

int remote_truncate (shared_ptr<remote_inode> i, off_t offset) {
	return -ENOSYS;
}