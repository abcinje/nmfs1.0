#include "local_ops.hpp"

extern std::shared_ptr<rados_io> meta_pool;
extern std::shared_ptr<rados_io> data_pool;
extern std::unique_ptr<directory_table> indexing_table;
extern std::unique_ptr<client> this_client;
extern std::unique_ptr<file_handler_list> open_context;
extern std::unique_ptr<journal> journalctl;

void local_getattr(shared_ptr<inode> i, struct stat *stat) {
	global_logger.log(local_fs_op, "Called getattr()");
	{
		std::scoped_lock scl{i->inode_mutex};
		i->fill_stat(stat);
	}
}

void local_access(shared_ptr<inode> i, int mask) {
	global_logger.log(local_fs_op, "Called access()");
	{
		std::scoped_lock scl{i->inode_mutex};
		i->permission_check(mask);
	}
}

int local_opendir(shared_ptr<inode> i, struct fuse_file_info *file_info) {
	global_logger.log(local_fs_op, "Called opendir()");
	{
		std::scoped_lock scl{i->inode_mutex};
		if (!S_ISDIR(i->get_mode()))
			return -ENOTDIR;

		shared_ptr<file_handler> fh = std::make_shared<file_handler>(i->get_ino());
		fh->set_loc(LOCAL);
		fh->set_i(i);
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());
		fh->set_fhno(file_info->fh);

		open_context->add_file_handler(file_info->fh, fh);

	}
	return 0;
}

int local_releasedir(shared_ptr<inode> i, struct fuse_file_info *file_info) {
	global_logger.log(local_fs_op, "Called releasedir(class inode)");
	int ret = open_context->delete_file_handler(file_info->fh);

	return ret;
}

void local_readdir(shared_ptr<inode> i, void *buffer, fuse_fill_dir_t filler) {
	global_logger.log(local_fs_op, "Called readdir()");
	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(i->get_ino());
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		parent_dentry_table->fill_filler(buffer, filler);
	}
}

int local_mkdir(shared_ptr<inode> parent_i, std::string new_child_name, mode_t mode, std::shared_ptr<inode>& new_dir_inode, std::shared_ptr<dentry>& new_dir_dentry) {
	global_logger.log(local_fs_op, "Called mkdir()");

	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());
	shared_ptr<inode> new_i = std::make_shared<inode>(parent_i->get_ino(), this_client->get_client_uid(), this_client->get_client_gid(),mode | S_IFDIR);
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		parent_dentry_table->create_child_inode(new_child_name, new_i);

		struct timespec ts{};
		timespec_get(&ts, TIME_UTC);
		parent_i->set_mtime(ts);
		parent_i->set_ctime(ts);
		journalctl->mkdir(parent_i, new_child_name, new_i->get_ino());

		new_i->set_size(DIR_INODE_SIZE);
		shared_ptr<dentry> new_d = std::make_shared<dentry>(new_i->get_ino(), true);
		journalctl->mkself(new_i);

		new_dir_inode = new_i;
		new_dir_dentry = new_d;
	}
	return 0;
}

int local_rmdir_top(shared_ptr<inode> target_i, uuid target_ino) {
	global_logger.log(local_fs_op, "Called rmdir_top()");
	shared_ptr<dentry_table> target_dentry_table = indexing_table->get_dentry_table(target_ino);
	if (target_dentry_table == nullptr) {
		throw std::runtime_error("directory table is corrupted : Can't find leased directory");
	}

	{
		std::scoped_lock scl{target_dentry_table->dentry_table_mutex};
		if (target_dentry_table->get_child_num() > 0)
			return -ENOTEMPTY;
		journalctl->rmself(target_ino);
		/* TODO : is it okay to delete dentry_table which is locked? */
		indexing_table->delete_dentry_table(target_ino);
	}
	return 0;
}

int local_rmdir_down(shared_ptr<inode> parent_i, uuid target_ino, std::string target_name) {
	global_logger.log(local_fs_op, "Called rmdir_down()");
	int ret = 0;
	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());
	if (parent_dentry_table == nullptr) {
		throw std::runtime_error("directory table is corrupted : Can't find leased directory");
	}
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		/* TODO : is it okay to delete inode which is locked? */
		parent_dentry_table->delete_child_inode(target_name);

		struct timespec ts{};
		timespec_get(&ts, TIME_UTC);
		parent_i->set_mtime(ts);
		parent_i->set_ctime(ts);
		journalctl->rmdir(parent_i, target_name, target_ino);

		/* It may be failed if top and down both are local */
		ret = indexing_table->delete_dentry_table(target_ino);
		if (ret == -1) {
			global_logger.log(local_fs_op, "this dentry table already removed in rmdir_top()");
		}
	}
	return 0;
}

int local_symlink(shared_ptr<inode> dst_parent_i, const char *src, const char *dst) {
	global_logger.log(local_fs_op, "Called symlink()");
	shared_ptr<dentry_table> dst_parent_dentry_table = indexing_table->get_dentry_table(dst_parent_i->get_ino());

	unique_ptr<std::string> symlink_name = get_filename_from_path(dst);
	{
		std::scoped_lock scl{dst_parent_dentry_table->dentry_table_mutex};
		if (!dst_parent_dentry_table->check_child_inode(*symlink_name).is_nil())
			return -EEXIST;

		shared_ptr<inode> symlink_i = std::make_shared<inode>(dst_parent_i->get_ino(), this_client->get_client_uid(),
								      this_client->get_client_gid(), S_IFLNK | 0777,
								      src);

		symlink_i->set_size(static_cast<off_t>(std::string(src).length()));

		dst_parent_dentry_table->create_child_inode(*symlink_name, symlink_i);

		struct timespec ts{};
		timespec_get(&ts, TIME_UTC);
		journalctl->mkreg(dst_parent_i, *symlink_name, symlink_i);
	}
	return 0;
}

int local_readlink(shared_ptr<inode> i, char *buf, size_t size) {
	global_logger.log(local_fs_op, "Called readlink()");
	{
		std::scoped_lock scl{i->inode_mutex};
		if (!S_ISLNK(i->get_mode()))
			return -EINVAL;

		size_t len = MIN(i->get_link_target_len(), size - 1);
		memcpy(buf, i->get_link_target_name()->data(), len);
		buf[len] = '\0';
	}
	return 0;
}

int local_rename_same_parent(shared_ptr<inode> parent_i, const char *old_path, const char *new_path, unsigned int flags) {
	global_logger.log(local_fs_op, "Called rename_same_parent()");
	size_t paradox_check = std::string(new_path).find(old_path);
	if ((paradox_check == 0) && (std::string(new_path).at(std::string(old_path).length()) == '/'))
		return -EINVAL;

	unique_ptr<std::string> old_name = get_filename_from_path(old_path);
	unique_ptr<std::string> new_name = get_filename_from_path(new_path);

	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());
	shared_ptr<inode> target_i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		target_i = parent_dentry_table->get_child_inode(*old_name);
	}

	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex, target_i->inode_mutex};
		uuid check_dst_ino = parent_dentry_table->check_child_inode(*new_name);

		struct timespec ts{};
		timespec_get(&ts, TIME_UTC);
		parent_i->set_mtime(ts);
		parent_i->set_ctime(ts);

		if (flags == 0) {
			if (!check_dst_ino.is_nil()) {
				/* TODO : directory inode location is different */
				std::shared_ptr<inode> check_dst_inode = parent_dentry_table->get_child_inode(*new_name);
				parent_dentry_table->delete_child_inode(*new_name);
				journalctl->rmreg(parent_i, *new_name, check_dst_inode);
			}

			parent_dentry_table->delete_child_inode(*old_name);
			journalctl->rmreg(parent_i, *old_name, target_i);
			parent_dentry_table->create_child_inode(*new_name, target_i);
			journalctl->mkreg(parent_i, *new_name, target_i);
		} else {
			return -ENOSYS;
		}
	}
	return 0;
}

std::shared_ptr<inode> local_rename_not_same_parent_src(shared_ptr<inode> src_parent_i, const char *old_path, unsigned int flags) {
	global_logger.log(local_fs_op, "Called rename_not_same_parent_src()");
	unique_ptr<std::string> old_name = get_filename_from_path(old_path);

	uuid target_ino;
	shared_ptr<dentry_table> src_dentry_table = indexing_table->get_dentry_table(src_parent_i->get_ino());
	shared_ptr<inode> target_i;
	{
		std::scoped_lock scl{src_dentry_table->dentry_table_mutex};
		target_i = src_dentry_table->get_child_inode(*old_name);
	}

	{
		std::scoped_lock scl{src_dentry_table->dentry_table_mutex, target_i->inode_mutex};
		if (flags == 0) {
			/* TODO : is it okay to delete inode which is locked? */
			src_dentry_table->delete_child_inode(*old_name);
			journalctl->rmreg(src_parent_i, *old_name, target_i);
		} else {
			throw std::runtime_error("NOT IMPLEMENTED");
		}
	}
	return target_i;
}

int local_rename_not_same_parent_dst(shared_ptr<inode> dst_parent_i, std::shared_ptr<inode> target_inode, uuid check_dst_ino,
				     const char *new_path, unsigned int flags) {
	global_logger.log(local_fs_op, "Called rename_not_same_parent_dst()");
	unique_ptr<std::string> new_name = get_filename_from_path(new_path);

	shared_ptr<dentry_table> dst_dentry_table = indexing_table->get_dentry_table(dst_parent_i->get_ino());
	target_inode->set_loc(LOCAL);
	target_inode->set_p_ino(dst_parent_i->get_ino());
	{
		std::scoped_lock scl{dst_dentry_table->dentry_table_mutex, target_inode->inode_mutex};
		if (flags == 0) {
			if (!check_dst_ino.is_nil()) {
				std::shared_ptr<inode> check_dst_inode = dst_dentry_table->get_child_inode(*new_name);
				dst_dentry_table->delete_child_inode(*new_name);
				journalctl->rmreg(dst_parent_i, *new_name, check_dst_inode);
				/* TODO : is it okay to delete inode which is locked? */
			}
			dst_dentry_table->create_child_inode(*new_name, target_inode);
			journalctl->mkreg(dst_parent_i, *new_name, target_inode);
		} else {
			throw std::runtime_error("NOT IMPLEMENTED");
		}
	}
	return 0;
}

int local_open(shared_ptr<inode> i, struct fuse_file_info *file_info) {
	global_logger.log(local_fs_op, "Called open()");
	{
		std::scoped_lock scl{i->inode_mutex};
		if ((file_info->flags & O_DIRECTORY) && !S_ISDIR(i->get_mode()))
			return -ENOTDIR;

		if ((file_info->flags & O_NOFOLLOW) && S_ISLNK(i->get_mode()))
			return -ELOOP;

		if ((file_info->flags & O_TRUNC) && !(file_info->flags & O_PATH)) {
			i->set_size(0);
			journalctl->chreg(i->get_p_ino(), i);
		}

		shared_ptr<file_handler> fh = std::make_shared<file_handler>(i->get_ino());
		fh->set_loc(LOCAL);
		fh->set_i(i);
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());
		fh->set_fhno(file_info->fh);

		open_context->add_file_handler(file_info->fh, fh);
	}
	return 0;
}

int local_release(shared_ptr<inode> i, struct fuse_file_info *file_info) {
	global_logger.log(local_fs_op, "Called release(class inode)");
	int ret = open_context->delete_file_handler(file_info->fh);

	return ret;
}

void local_create(shared_ptr<inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info *file_info) {
	global_logger.log(local_fs_op, "Called create()");

	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());
	shared_ptr<inode> i = std::make_shared<inode>(parent_i->get_ino(), this_client->get_client_uid(), this_client->get_client_gid(),mode | S_IFREG);
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};

		parent_dentry_table->create_child_inode(new_child_name, i);

		struct timespec ts{};
		timespec_get(&ts, TIME_UTC);
		parent_i->set_mtime(ts);
		parent_i->set_ctime(ts);
		journalctl->mkreg(parent_i, new_child_name, i);

		shared_ptr<file_handler> fh = std::make_shared<file_handler>(i->get_ino());
		fh->set_loc(LOCAL);
		fh->set_i(i);
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());
		fh->set_fhno(file_info->fh);

		open_context->add_file_handler(file_info->fh, fh);
	}
}

void local_unlink(shared_ptr<inode> parent_i, std::string child_name) {
	global_logger.log(local_fs_op, "Called unlink()");
	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		shared_ptr<inode> target_i = parent_dentry_table->get_child_inode(child_name);
		nlink_t nlink = target_i->get_nlink() - 1;
		if (nlink == 0) {
			/* data */
			data_pool->remove(obj_category::DATA, uuid_to_string(target_i->get_ino()));

			/* parent dentry */
			parent_dentry_table->delete_child_inode(child_name);

			struct timespec ts{};
			timespec_get(&ts, TIME_UTC);
			parent_i->set_mtime(ts);
			parent_i->set_ctime(ts);
			journalctl->rmreg(parent_i, child_name, target_i);
		} else {
			target_i->set_nlink(nlink);
			journalctl->chreg(target_i->get_p_ino(), target_i);
		}
	}
}

ssize_t local_read(shared_ptr<inode> i, char *buffer, size_t size, off_t offset) {
	global_logger.log(local_fs_op, "Called read()");
	size_t read_len = 0;

	read_len = data_pool->read(obj_category::DATA, uuid_to_string(i->get_ino()), buffer, size, offset);
	return read_len;
}

ssize_t local_write(shared_ptr<inode> i, const char *buffer, size_t size, off_t offset, int flags) {
	global_logger.log(local_fs_op, "Called write()");
	size_t written_len = 0;

	{
		std::scoped_lock scl{i->inode_mutex};
		if (flags & O_APPEND) {
			offset = i->get_size();
		}

		written_len = data_pool->write(obj_category::DATA, uuid_to_string(i->get_ino()), buffer, size, offset);

		if (i->get_size() < offset + size) {
			i->set_size(offset + size);

			journalctl->chreg(i->get_p_ino(), i);
		}
	}
	return written_len;
}

void local_chmod(shared_ptr<inode> i, mode_t mode) {
	global_logger.log(local_fs_op, "Called chmod()");
	{
		std::scoped_lock scl{i->inode_mutex};
		mode_t type = i->get_mode() & S_IFMT;

		i->set_mode(mode | type);

		if(S_ISDIR(i->get_mode()))
			journalctl->chself(i);
		else
			journalctl->chreg(i->get_p_ino(), i);
	}
}

void local_chown(shared_ptr<inode> i, uid_t uid, gid_t gid) {
	global_logger.log(local_fs_op, "Called chown()");
	{
		std::scoped_lock scl{i->inode_mutex};
		if (((int32_t) uid) >= 0)
			i->set_uid(uid);

		if (((int32_t) gid) >= 0)
			i->set_gid(gid);

		if(S_ISDIR(i->get_mode()))
			journalctl->chself(i);
		else
			journalctl->chreg(i->get_p_ino(), i);
	}
}

void local_utimens(shared_ptr<inode> i, const struct timespec tv[2]) {
	global_logger.log(local_fs_op, "Called utimens()");
	{
		std::scoped_lock scl{i->inode_mutex};
		if (tv[0].tv_nsec == UTIME_NOW) {
			struct timespec ts{};
			if (!timespec_get(&ts, TIME_UTC))
				throw runtime_error("timespec_get() failed");
			i->set_atime(ts);
		} else if (tv[0].tv_nsec == UTIME_OMIT) { ;
		} else {
			i->set_atime(tv[0]);
		}

		if (tv[1].tv_nsec == UTIME_NOW) {
			struct timespec ts{};
			if (!timespec_get(&ts, TIME_UTC))
				throw runtime_error("timespec_get() failed");
			i->set_mtime(ts);
		} else if (tv[1].tv_nsec == UTIME_OMIT) { ;
		} else {
			i->set_mtime(tv[1]);
		}

		if(S_ISDIR(i->get_mode()))
			journalctl->chself(i);
		else
			journalctl->chreg(i->get_p_ino(), i);
	}
}

int local_truncate(const shared_ptr<inode> i, off_t offset) {
	global_logger.log(local_fs_op, "Called truncate()");
	/*TODO : clear setuid, setgid*/
	int ret;
	{
		std::scoped_lock scl{i->inode_mutex};
		if (S_ISDIR(i->get_mode()))
			return -EISDIR;

		ret = data_pool->truncate(obj_category::DATA, uuid_to_string(i->get_ino()), offset);

		i->set_size(offset);
		struct timespec ts{};
		timespec_get(&ts, TIME_UTC);
		i->set_mtime(ts);
		i->set_ctime(ts);

		if(S_ISDIR(i->get_mode()))
			journalctl->chself(i);
		else
			journalctl->chreg(i->get_p_ino(), i);
	}
	return ret;
}
