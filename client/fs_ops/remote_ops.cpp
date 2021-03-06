#include "remote_ops.hpp"

int remote_getattr(shared_ptr<remote_inode> i, struct stat* stat) {
	global_logger.log(remote_fs_op, "Called remote_getattr()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->getattr(i, stat);
	return ret;
}

int remote_access(shared_ptr<remote_inode> i, int mask) {
	global_logger.log(remote_fs_op, "Called remote_access()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->access(i, mask);
	return ret;
}

int remote_opendir(shared_ptr<remote_inode> i, struct fuse_file_info* file_info) {
	global_logger.log(remote_fs_op, "Called remote_opendir()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->opendir(i, file_info);
	return ret;
}

int remote_readdir(shared_ptr<remote_inode> i, void* buffer, fuse_fill_dir_t filler) {
	global_logger.log(remote_fs_op, "Called remote_readdir()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->readdir(i, buffer, filler);
	return ret;
}

int remote_mkdir(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, std::shared_ptr<inode>& new_dir_inode, std::shared_ptr<dentry>& new_dir_dentry) {
	global_logger.log(remote_fs_op, "Called remote_mkdir()");
	if(parent_i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->mkdir(parent_i, new_child_name, mode, new_dir_inode, new_dir_dentry);
	return ret;
}

int remote_rmdir_top(shared_ptr<remote_inode> target_i, uuid target_ino) {
	global_logger.log(remote_fs_op, "Called remote_rmdir_top()");
	if(target_i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(target_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->rmdir_top(target_i, target_ino);
	return ret;
}

int remote_rmdir_down(shared_ptr<remote_inode> parent_i, uuid target_ino, std::string target_name) {
	global_logger.log(remote_fs_op, "Called remote_rmdir_down()");
	if(parent_i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->rmdir_down(parent_i, target_ino, target_name);
	return ret;
}

int remote_symlink(shared_ptr<remote_inode> dst_parent_i, const char *src, const char *dst) {
	global_logger.log(remote_fs_op, "Called remote_synlink()");
	if(dst_parent_i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(dst_parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->symlink(dst_parent_i, src, dst);
	return ret;
}

int remote_readlink(shared_ptr<remote_inode> i, char *buf, size_t size) {
	global_logger.log(remote_fs_op, "Called remote_readlink()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->readlink(i, buf, size);
	return ret;
}

int remote_rename_same_parent(shared_ptr<remote_inode> parent_i, const char* old_path, const char* new_path, unsigned int flags) {
	global_logger.log(remote_fs_op, "Called remote_rename_same_parent()");
	if(parent_i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->rename_same_parent(parent_i, old_path, new_path, flags);
	return ret;
}

int remote_rename_not_same_parent_src(shared_ptr<remote_inode> src_parent_i, const char* old_path, unsigned int flags, std::shared_ptr<inode>& target_inode) {
	global_logger.log(remote_fs_op, "Called remote_rename_not_same_parent_src()");
	if(src_parent_i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(src_parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->rename_not_same_parent_src(src_parent_i, old_path, flags, target_inode);
	return ret;
}

int remote_rename_not_same_parent_dst(shared_ptr<remote_inode> dst_parent_i, std::shared_ptr<inode>& target_inode, uuid check_dst_ino, const char* new_path, unsigned int flags) {
	global_logger.log(remote_fs_op, "Called remote_rename_not_same_parent_dst()");
	if(dst_parent_i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(dst_parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->rename_not_same_parent_dst(dst_parent_i, target_inode, check_dst_ino, new_path, flags);
	return ret;
}

int remote_open(shared_ptr<remote_inode> i, struct fuse_file_info* file_info) {
	global_logger.log(remote_fs_op, "Called remote_open()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->open(i, file_info);
	return ret;
}

int remote_create(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info* file_info) {
	global_logger.log(remote_fs_op, "Called remote_create()");
	if(parent_i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->create(parent_i, new_child_name, mode, file_info);
	return ret;
}

int remote_unlink(shared_ptr<remote_inode> parent_i, std::string child_name) {
	global_logger.log(remote_fs_op, "Called remote_unlink()");
	if(parent_i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(parent_i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->unlink(parent_i, child_name);
	return ret;
}

ssize_t remote_write(shared_ptr<remote_inode> i, const char* buffer, size_t size, off_t offset, int flags) {
	global_logger.log(remote_fs_op, "Called remote_write()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	ssize_t written_len = rc->write(i, buffer, size, offset, flags);
	return written_len;
}

int remote_chmod(shared_ptr<remote_inode> i, mode_t mode) {
	global_logger.log(remote_fs_op, "Called remote_chmod()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->chmod(i, mode);
	return ret;
}

int remote_chown(shared_ptr<remote_inode> i, uid_t uid, gid_t gid) {
	global_logger.log(remote_fs_op, "Called remote_chown()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->chown(i, uid, gid);
	return ret;
}

int remote_utimens(shared_ptr<remote_inode> i, const struct timespec tv[2]) {
	global_logger.log(remote_fs_op, "Called remote_utimens()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->utimens(i, tv);
	return ret;
}

int remote_truncate (shared_ptr<remote_inode> i, off_t offset) {
	global_logger.log(remote_fs_op, "Called remote_truncate()");
	if(i == nullptr)
		throw std::runtime_error("inode casting is failed");
	std::string remote_address(i->get_address());
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	int ret = rc->truncate(i, offset);
	return ret;
}