#include "remote_ops.hpp"

void remote_getattr(shared_ptr<remote_inode> i, struct stat* stat) {
	global_logger.log(remote_fs_op, "Called remote_getattr()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	rc->getattr(i, stat);
}

void remote_access(shared_ptr<remote_inode> i, int mask) {
	global_logger.log(remote_fs_op, "Called remote_access()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	rc->access(i, mask);
}

int remote_opendir(shared_ptr<remote_inode> i, struct fuse_file_info* file_info) {
	global_logger.log(remote_fs_op, "Called remote_opendir()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->opendir(i, file_info);
	return ret;
}

void remote_readdir(shared_ptr<remote_inode> i, void* buffer, fuse_fill_dir_t filler) {
	global_logger.log(remote_fs_op, "Called remote_readdir()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	rc->readdir(i, buffer, filler);
}

ino_t remote_mkdir(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode) {
	global_logger.log(remote_fs_op, "Called remote_mkdir()");
	std::string remote_address(parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	ino_t new_dir_ino = rc->mkdir(parent_i, new_child_name, mode);
	return new_dir_ino;
}

int remote_rmdir_top(shared_ptr<remote_inode> target_i, std::string target_name) {
	global_logger.log(remote_fs_op, "Called remote_rmdir_top()");
	std::string remote_address(target_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->rmdir_top(target_i, target_name);
	return ret;
}

int remote_rmdir_down(shared_ptr<remote_inode> parent_i, ino_t target_ino, std::string target_name) {
	global_logger.log(remote_fs_op, "Called remote_rmdir_down()");
	std::string remote_address(parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	rc->rmdir_down(parent_i, target_ino, target_name);
	return 0;
}

int remote_symlink(shared_ptr<remote_inode> dst_parent_i, const char *src, const char *dst) {
	global_logger.log(remote_fs_op, "Called remote_synlink()");
	std::string remote_address(dst_parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->symlink(dst_parent_i, src, dst);
	return ret;
}

int remote_readlink(shared_ptr<remote_inode> i, char *buf, size_t size) {
	global_logger.log(remote_fs_op, "Called remote_readlink()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->readlink(i, buf, size);
	return ret;
}

int remote_rename_same_parent(shared_ptr<remote_inode> parent_i, const char* old_path, const char* new_path, unsigned int flags) {
	global_logger.log(remote_fs_op, "Called remote_rename_same_parent()");
	std::string remote_address(parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->rename_same_parent(parent_i, old_path, new_path, flags);
	return ret;
}

ino_t remote_rename_not_same_parent_src(shared_ptr<remote_inode> src_parent_i, const char* old_path, unsigned int flags) {
	global_logger.log(remote_fs_op, "Called remote_rename_not_same_parent_src()");
	std::string remote_address(src_parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	ino_t target_ino = rc->rename_not_same_parent_src(src_parent_i, old_path, flags);
	return target_ino;
}

int remote_rename_not_same_parent_dst(shared_ptr<remote_inode> dst_parent_i, ino_t target_ino, ino_t check_dst_ino, const char* new_path, unsigned int flags) {
	global_logger.log(remote_fs_op, "Called remote_rename_not_same_parent_dst()");
	std::string remote_address(dst_parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->rename_not_same_parent_dst(dst_parent_i, target_ino, check_dst_ino, new_path, flags);
	return ret;
}

int remote_rename_not_same_parent(shared_ptr<remote_inode> src_parent_i, shared_ptr<remote_inode> dst_parent_i, const char* old_path, const char* new_path, unsigned int flags) {
	global_logger.log(remote_fs_op, "Called remote_rename_not_same_parent()");

	return -ENOSYS;
}

int remote_open(shared_ptr<remote_inode> i, struct fuse_file_info* file_info) {
	global_logger.log(remote_fs_op, "Called remote_open()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->open(i, file_info);
	return ret;
}

void remote_create(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info* file_info) {
	global_logger.log(remote_fs_op, "Called remote_create()");
	std::string remote_address(parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	rc->create(parent_i, new_child_name, mode, file_info);
}

void remote_unlink(shared_ptr<remote_inode> parent_i, std::string child_name) {
	global_logger.log(remote_fs_op, "Called remote_unlink()");
	std::string remote_address(parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	rc->unlink(parent_i, child_name);
}

size_t remote_write(shared_ptr<remote_inode> i, const char* buffer, size_t size, off_t offset, int flags) {
	global_logger.log(remote_fs_op, "Called remote_write()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	size_t written_len = rc->write(i, buffer, size, offset, flags);
	return written_len;
}

void remote_chmod(shared_ptr<remote_inode> i, mode_t mode) {
	global_logger.log(remote_fs_op, "Called remote_chmod()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	rc->chmod(i, mode);
}

void remote_chown(shared_ptr<remote_inode> i, uid_t uid, gid_t gid) {
	global_logger.log(remote_fs_op, "Called remote_chown()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	rc->chown(i, uid, gid);
}

void remote_utimens(shared_ptr<remote_inode> i, const struct timespec tv[2]) {
	global_logger.log(remote_fs_op, "Called remote_utimens()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	rc->utimens(i, tv);
}

int remote_truncate (shared_ptr<remote_inode> i, off_t offset) {
	global_logger.log(remote_fs_op, "Called remote_truncate()");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->truncate(i, offset);
	return ret;
}