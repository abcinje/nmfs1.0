#include "local_ops.hpp"

#include <utility>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
extern rados_io *meta_pool;
extern rados_io *data_pool;
extern directory_table *indexing_table;

std::map<ino_t, unique_ptr<file_handler>> fh_list;
std::mutex file_handler_mutex;

void local_getattr(shared_ptr<inode> i, struct stat* stat) {
	global_logger.log(local_fs_op, "Called getattr()");
	i->fill_stat(stat);
}

void local_access(shared_ptr<inode> i, int mask) {
	global_logger.log(local_fs_op, "Called access()");
	i->permission_check(mask);
}

int local_opendir(shared_ptr<inode> i, struct fuse_file_info* file_info) {
	global_logger.log(local_fs_op, "Called opendir()");
	if(!S_ISDIR(i->get_mode()))
		return -ENOTDIR;

	{
		std::scoped_lock<std::mutex> lock{file_handler_mutex};
		unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());

		fh->set_fhno((void *) file_info->fh);
		fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));
	}
	return 0;
}

int local_releasedir(shared_ptr<inode> i, struct fuse_file_info* file_info) {
	global_logger.log(local_fs_op, "Called releasedir(class inode)");
	std::map<ino_t, unique_ptr<file_handler>>::iterator it;
	{
		std::scoped_lock<std::mutex> lock{file_handler_mutex};
		it = fh_list.find(i->get_ino());

		if (it == fh_list.end())
			return -EIO;

		fh_list.erase(it);
	}
	return 0;
}

int local_releasedir(ino_t ino, struct fuse_file_info* file_info){
	global_logger.log(local_fs_op, "Called releasedir(ino_t)");
	std::map<ino_t, unique_ptr<file_handler>>::iterator it;
	{
		std::scoped_lock<std::mutex> lock{file_handler_mutex};
		it = fh_list.find(ino);

		if (it == fh_list.end())
			return -EIO;

		fh_list.erase(it);
	}

	return 0;
}

void local_readdir(shared_ptr<inode> i, void* buffer, fuse_fill_dir_t filler) {
	global_logger.log(local_fs_op, "Called readdir()");
	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(i->get_ino());

	parent_dentry_table->fill_filler(buffer, filler);
}

void local_mkdir(shared_ptr<inode> parent_i, std::string new_child_name, mode_t mode) {
	global_logger.log(local_fs_op, "Called mkdir()");
	fuse_context *fuse_ctx = fuse_get_context();

	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());
	shared_ptr<inode> i = std::make_shared<inode>(fuse_ctx->uid, fuse_ctx->gid, mode | S_IFDIR);
	parent_dentry_table->create_child_inode(new_child_name, i);

	/* TODO : separate make_new_child routine and become_leader routine */
	shared_ptr<dentry_table> new_dentry_table = become_leader_of_new_dir(parent_i->get_ino(), i->get_ino());

	i->set_size(new_dentry_table->get_total_name_legth());
	i->sync();

	/* TODO : manage dentry and inode of newly created directory */
	//shared_ptr<dentry> new_d = std::make_shared<dentry>(i->get_ino(), true);
	//new_d->sync();

	parent_i->set_size(parent_dentry_table->get_total_name_legth());
	parent_i->sync();

	indexing_table->add_dentry_table(i->get_ino(), new_dentry_table);

}

int local_rmdir(shared_ptr<inode> parent_i, shared_ptr<inode> target_i, std::string target_name) {
	global_logger.log(local_fs_op, "Called rmdir()");
	if(!S_ISDIR(target_i->get_mode()))
		return -ENOTDIR;

	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());
	shared_ptr<dentry_table> target_dentry_table = indexing_table->get_dentry_table(target_i->get_ino());

	if(target_dentry_table == nullptr) {
		throw std::runtime_error("directory table is corrupted : Can't find leaesd directory");
	}

	if(target_dentry_table->get_child_num() > 2)
		return -ENOTEMPTY;
	meta_pool->remove(DENTRY, std::to_string(target_i->get_ino()));
	meta_pool->remove(INODE, std::to_string(target_i->get_ino()));

	parent_dentry_table->delete_child_inode(target_name);

	parent_i->set_size(parent_dentry_table->get_total_name_legth());
	parent_i->sync();

	indexing_table->delete_dentry_table(target_i->get_ino());

	return 0;
}

int local_symlink(shared_ptr<inode> dst_parent_i, const char *src, const char *dst) {
	global_logger.log(local_fs_op, "Called symlink()");
	fuse_context *fuse_ctx = fuse_get_context();
	shared_ptr<dentry_table> dst_parent_dentry_table = indexing_table->get_dentry_table(dst_parent_i->get_ino());

	unique_ptr<std::string> symlink_name = get_filename_from_path(dst);

	if (dst_parent_dentry_table->check_child_inode(symlink_name->data()) != -1)
		return -EEXIST;

	shared_ptr<inode> symlink_i = std::make_shared<inode>(fuse_ctx->uid, fuse_ctx->gid, S_IFLNK | 0777, std::string(src).length(), src);

	symlink_i->set_size(std::string(src).length());

	dst_parent_dentry_table->create_child_inode(symlink_name->data(), symlink_i);
	symlink_i->sync();

	dst_parent_i->set_size(dst_parent_dentry_table->get_total_name_legth());
	dst_parent_i->sync();

	return 0;
}

int local_readlink(shared_ptr<inode> i, char *buf, size_t size) {
	global_logger.log(local_fs_op, "Called readdir()");
	if (!S_ISLNK(i->get_mode()))
		return -EINVAL;

	size_t len = MIN(i->get_link_target_len(), size-1);
	memcpy(buf, i->get_link_target_name(), len);
	buf[len] = '\0';

	return 0;
}

int local_rename_same_parent(shared_ptr<inode> parent_i, const char* old_path, const char* new_path, unsigned int flags){
	global_logger.log(local_fs_op, "Called rename_same_parent()");
	size_t paradox_check = std::string(new_path).find(old_path);
	if((paradox_check == 0) && (std::string(new_path).at(std::string(old_path).length()) == '/'))
		return -EINVAL;

	unique_ptr<std::string> old_name = get_filename_from_path(old_path);
	unique_ptr<std::string> new_name = get_filename_from_path(new_path);

	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());

	shared_ptr<inode> target_i = parent_dentry_table->get_child_inode(*old_name);
	ino_t check_dst_ino = parent_dentry_table->check_child_inode(*new_name);

	if (flags == 0) {
		if(check_dst_ino != -1) {
			parent_dentry_table->delete_child_inode(*new_name);
			meta_pool->remove(INODE, std::to_string(check_dst_ino));
		}
		parent_dentry_table->delete_child_inode(*old_name);
		parent_dentry_table->create_child_inode(*new_name, target_i);

	} else {
		return -ENOSYS;
	}

	parent_i->set_size(parent_dentry_table->get_total_name_legth());
	return 0;
}

int local_rename_not_same_parent(shared_ptr<inode> src_parent_i, shared_ptr<inode> dst_parent_i, const char* old_path, const char* new_path, unsigned int flags){
	global_logger.log(local_fs_op, "Called rename_not_same_parent()");
	unique_ptr<std::string> old_name = get_filename_from_path(old_path);
	unique_ptr<std::string> new_name = get_filename_from_path(new_path);

	shared_ptr<dentry_table> src_dentry_table = indexing_table->get_dentry_table(src_parent_i->get_ino());
	shared_ptr<dentry_table> dst_dentry_table = indexing_table->get_dentry_table(dst_parent_i->get_ino());

	shared_ptr<inode> target_i = src_dentry_table->get_child_inode(*old_name);
	ino_t check_dst_ino = dst_dentry_table->check_child_inode(*new_name);

	if (flags == 0) {
		if(check_dst_ino != -1) {
			dst_dentry_table->delete_child_inode(*new_name);
			meta_pool->remove(INODE, std::to_string(check_dst_ino));
		}
		src_dentry_table->delete_child_inode(*old_name);
		dst_dentry_table->create_child_inode(*new_name, target_i);

	} else {
		return -ENOSYS;
	}

	src_parent_i->set_size(src_dentry_table->get_total_name_legth());
	dst_parent_i->set_size(dst_dentry_table->get_total_name_legth());
	return 0;
}

int local_open(shared_ptr<inode> i, struct fuse_file_info* file_info) {
	global_logger.log(local_fs_op, "Called open()");
	if ((file_info->flags & O_DIRECTORY) && !S_ISDIR(i->get_mode()))
		return -ENOTDIR;

	if ((file_info->flags & O_NOFOLLOW) && S_ISLNK(i->get_mode()))
		return -ELOOP;

	if ((file_info->flags & O_TRUNC) && !(file_info->flags & O_PATH)) {
		i->set_size(0);
		i->sync();
	}

	{
		std::scoped_lock<std::mutex> lock{file_handler_mutex};
		unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());

		fh->set_fhno((void *) file_info->fh);
		fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));
	}

	return 0;
}

int local_release(shared_ptr<inode> i, struct fuse_file_info* file_info) {
	global_logger.log(local_fs_op, "Called release(class inode)");
	std::map<ino_t, unique_ptr<file_handler>>::iterator it;
	{
		std::scoped_lock<std::mutex> lock{file_handler_mutex};
		it = fh_list.find(i->get_ino());

		if (it == fh_list.end())
			return -EIO;

		fh_list.erase(it);
	}

	return 0;
}

int local_release(ino_t ino, struct fuse_file_info* file_info){
	global_logger.log(local_fs_op, "Called release(ino_t)");
	std::map<ino_t, unique_ptr<file_handler>>::iterator it;
	{
		std::scoped_lock<std::mutex> lock{file_handler_mutex};
		it = fh_list.find(ino);

		if (it == fh_list.end())
			return -EIO;

		fh_list.erase(it);
	}

	return 0;
}

void local_create(shared_ptr<inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info* file_info) {
	global_logger.log(local_fs_op, "Called create()");
	fuse_context *fuse_ctx = fuse_get_context();

	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());
	shared_ptr<inode> i = std::make_shared<inode>(fuse_ctx->uid, fuse_ctx->gid, mode | S_IFREG);

	i->sync();

	parent_dentry_table->create_child_inode(new_child_name, i);
	parent_i->set_size(parent_dentry_table->get_total_name_legth());
	parent_i->sync();

	{
		std::scoped_lock<std::mutex> lock{file_handler_mutex};
		unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());

		fh->set_fhno((void *) file_info->fh);
		fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));
	}

}

void local_unlink(shared_ptr<inode> parent_i, std::string child_name) {
	global_logger.log(local_fs_op, "Called unlink()");
	shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());

	shared_ptr<inode> target_i = parent_dentry_table->get_child_inode(child_name);
	nlink_t nlink = target_i->get_nlink() - 1;
	if (nlink == 0) {
		/* data */
		data_pool->remove(DATA, std::to_string(target_i->get_ino()));

		/* inode */
		meta_pool->remove(INODE, std::to_string(target_i->get_ino()));

		/* parent dentry */
		parent_dentry_table->delete_child_inode(child_name);

		/* parent inode */
		parent_i->set_size(parent_dentry_table->get_total_name_legth());
		parent_i->sync();
	} else {
		target_i->set_nlink(nlink);
		target_i->sync();
	}

}

size_t local_read(shared_ptr<inode> i, char* buffer, size_t size, off_t offset) {
	global_logger.log(local_fs_op, "Called read()");
	size_t read_len = 0;

	read_len = data_pool->read(DATA, std::to_string(i->get_ino()), buffer, size, offset);
	return read_len;
}

size_t local_write(shared_ptr<inode> i, const char* buffer, size_t size, off_t offset, int flags) {
	global_logger.log(local_fs_op, "Called write()");
	size_t written_len = 0;

	if(flags & O_APPEND) {
		offset = i->get_size();
	}

	written_len = data_pool->write(DATA, std::to_string(i->get_ino()), buffer, size, offset);

	if (i->get_size() < offset + size) {
		i->set_size(offset + size);
		i->sync();
	}

	return written_len;
}

void local_chmod(shared_ptr<inode> i, mode_t mode) {
	global_logger.log(local_fs_op, "Called chmod()");
	mode_t type = i->get_mode() & S_IFMT;

	i->set_mode(mode | type);
	i->sync();
}
void local_chown(shared_ptr<inode> i, uid_t uid, gid_t gid) {
	global_logger.log(local_fs_op, "Called chown()");
	if (((int32_t) uid) >= 0)
		i->set_uid(uid);

	if (((int32_t) gid) >= 0)
		i->set_gid(gid);

	i->sync();
}

void local_utimens(shared_ptr<inode> i, const struct timespec tv[2]){
	global_logger.log(local_fs_op, "Called utimens()");
	if (tv[0].tv_nsec == UTIME_NOW) {
		struct timespec ts;
		if (!timespec_get(&ts, TIME_UTC))
			runtime_error("timespec_get() failed");
		i->set_atime(ts);
	} else if (tv[0].tv_nsec == UTIME_OMIT) { ;
	} else {
		i->set_atime(tv[0]);
	}

	if (tv[1].tv_nsec == UTIME_NOW) {
		struct timespec ts;
		if (!timespec_get(&ts, TIME_UTC))
			runtime_error("timespec_get() failed");
		i->set_mtime(ts);
	} else if (tv[1].tv_nsec == UTIME_OMIT) { ;
	} else {
		i->set_mtime(tv[1]);
	}

	i->sync();
}

int local_truncate (const shared_ptr<inode> i, off_t offset) {
	global_logger.log(local_fs_op, "Called truncate()");
	/*TODO : clear setuid, setgid*/
	if(S_ISDIR(i->get_mode()))
		return -EISDIR;

	int ret;
	ret = data_pool->truncate(DATA, std::to_string(i->get_ino()), offset);

	i->set_size(offset);
	i->sync();

	return ret;
}