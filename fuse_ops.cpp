#include "fuse_ops.hpp"
#include "meta/inode.hpp"
#include "meta/dentry.hpp"
#include "logger/logger.hpp"
#include "rados_io/rados_io.hpp"
#include "client/client.hpp"
#include "meta/file_handler.hpp"
#include "util.hpp"
#include <cstring>
#include <map>
#include <utility>
#include <mutex>

using std::unique_ptr;
using std::make_unique;

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define META_POOL "nmfs.meta"
#define DATA_POOL "nmfs.data"

rados_io *meta_pool;
rados_io *data_pool;

std::mutex m;
std::map<ino_t, unique_ptr<file_handler>> fh_list;

void *fuse_ops::init(struct fuse_conn_info *info, struct fuse_config *config)
{
	global_logger.log(fuse_op, "Called init()");
	client *this_client;

	rados_io::conn_info ci = {"client.admin", "ceph", 0};
	meta_pool = new rados_io(ci, META_POOL);
	data_pool = new rados_io(ci, DATA_POOL);

	/* client id allocation */
	if (!meta_pool->exist(CLIENT, "client.list")) {
		global_logger.log(fuse_op, "Very first Client is mounted");
		meta_pool->write(CLIENT, "client.list", "?o", 2, 0);
		this_client = new client(1);
	} else {
		this_client = new client();
		global_logger.log(fuse_op, "Client(ID=" + std::to_string(this_client->get_client_id()) + ") is mounted");
		global_logger.log(fuse_op, "Start Inode offset = " + std::to_string(this_client->get_per_client_ino_offset()));
	}

	/* root */
	if (!meta_pool->exist(INODE, "0")) {
		fuse_context *fuse_ctx = fuse_get_context();
		inode i(0, fuse_ctx->gid, S_IFDIR | 0777, true);

		dentry d(0, true);
		d.add_new_child(".", 0);
		d.add_new_child("..", 0);

		i.set_size(d.get_total_name_legth());
		i.sync();
		d.sync();
	}

	config->nullpath_ok = 0;
	return (void *)this_client;
}

void fuse_ops::destroy(void *private_data)
{
	global_logger.log(fuse_op, "Called destroy()");

	fuse_context *fuse_ctx = fuse_get_context();
	delete (client *)(fuse_ctx->private_data);

	delete meta_pool;
	delete data_pool;

}

int fuse_ops::getattr(const char* path, struct stat* stat, struct fuse_file_info* file_info) {
	global_logger.log(fuse_op, "Called getattr()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	unique_ptr<std::string> parent_name = get_parent_dir_path(path);
	unique_ptr<std::string> file_name = get_filename_from_path(path);

	try {
		if(std::string(path) == "/") {
			unique_ptr<inode> i = make_unique<inode>(0);
			i->fill_stat(stat);
		} else {
			unique_ptr<inode> parent_i = make_unique<inode>(parent_name->data());
			unique_ptr<dentry> parent_d = make_unique<dentry>(parent_i->get_ino());

			ino_t target_ino = parent_d->get_child_ino(file_name->data());
			unique_ptr<inode> i = make_unique<inode>(target_ino);
			i->fill_stat(stat);
		}
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::access(const char* path, int mask) {
	global_logger.log(fuse_op, "Called access()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	try {
		unique_ptr<inode> i = make_unique<inode>(path);
		permission_check(i.get(), mask);

	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
	    /* from inode constructor */
		return -EACCES;
	}

	return 0;
}

int fuse_ops::symlink(const char *src, const char *dst){
	global_logger.log(fuse_op, "Called symlink()");
	global_logger.log(fuse_op, "src : " + std::string(src) + " dst : " + std::string(dst));

	fuse_context *fuse_ctx = fuse_get_context();

	try {
		unique_ptr<std::string> dst_parent_name = get_parent_dir_path(dst);
		unique_ptr<inode> dst_parent_i = make_unique<inode>(dst_parent_name->data());
		unique_ptr<dentry> dst_parent_d = make_unique<dentry>(dst_parent_i->get_ino());

		unique_ptr<std::string> symlink_name = get_filename_from_path(dst);

		if (dst_parent_d->get_child_ino(symlink_name->data()) != -1)
			return -EEXIST;

		unique_ptr<inode> symlink_i = make_unique<inode>(fuse_ctx->uid, fuse_ctx->gid, S_IFLNK | 0777, std::string(src).length(), src);
		symlink_i->set_size(std::string(src).length());

		dst_parent_d->add_new_child(symlink_name->data(), symlink_i->get_ino());

		dst_parent_d->sync();
		symlink_i->sync();

		dst_parent_i->set_size(dst_parent_d->get_total_name_legth());
		dst_parent_i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::readlink(const char* path, char* buf, size_t size)
{
	global_logger.log(fuse_op, "Called readlink()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	unique_ptr<std::string> parent_name = get_parent_dir_path(path);
	unique_ptr<std::string> file_name = get_filename_from_path(path);

	try {
		unique_ptr<inode> parent_i = make_unique<inode>(parent_name->data());
		unique_ptr<dentry> parent_d = make_unique<dentry>(parent_i->get_ino());

		ino_t target_ino = parent_d->get_child_ino(file_name->data());
		unique_ptr<inode> i = make_unique<inode>(target_ino);

		if (!S_ISLNK(i->get_mode()))
			return -EINVAL;

		size_t len = MIN(i->get_link_target_len(), size-1);
		memcpy(buf, i->get_link_target_name(), len);
		buf[len] = '\0';
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::opendir(const char* path, struct fuse_file_info* file_info){
	global_logger.log(fuse_op, "Called opendir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	try {
		unique_ptr<inode> i = make_unique<inode>(path);

		if(!S_ISDIR(i->get_mode()))
			return -ENOTDIR;

		std::scoped_lock<std::mutex> lock(m);
		unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());

		fh->set_fhno((void *)file_info->fh);
		fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::releasedir(const char* path, struct fuse_file_info* file_info){
	global_logger.log(fuse_op, "Called releasedir()");
	if(path != nullptr) {
		global_logger.log(fuse_op, "path : " + std::string(path));

		unique_ptr<inode> i = make_unique<inode>(path);

		std::map<ino_t, unique_ptr<file_handler>>::iterator it;
		std::scoped_lock<std::mutex> lock(m);
		it = fh_list.find(i->get_ino());

		if (it == fh_list.end())
			return -EIO;

		fh_list.erase(it);
	} else {
		global_logger.log(fuse_op, "path : nullpath");
		file_handler *fh = reinterpret_cast<file_handler *>(file_info->fh);
		ino_t ino = fh->get_ino();

		std::map<ino_t, unique_ptr<file_handler>>::iterator it;
		std::scoped_lock<std::mutex> lock(m);
		it = fh_list.find(ino);

		if (it == fh_list.end())
			return -EIO;

		fh_list.erase(it);
	}

	return 0;
}

int fuse_ops::readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* file_info, enum fuse_readdir_flags readdir_flags){
	global_logger.log(fuse_op, "Called readdir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	unique_ptr<inode> i = make_unique<inode>(path);
	unique_ptr<dentry> d = make_unique<dentry>(i->get_ino());

	d->fill_filler(buffer, filler);

	return 0;
}
int fuse_ops::mkdir(const char* path, mode_t mode){
	global_logger.log(fuse_op, "Called mkdir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	fuse_context *fuse_ctx = fuse_get_context();

	try {
		unique_ptr<inode> parent_i = make_unique<inode>(*(get_parent_dir_path(path).get()));
		uint64_t parent_ino = parent_i->get_ino();

		unique_ptr<dentry> parent_d = make_unique<dentry>(parent_ino);

		unique_ptr<inode> i = make_unique<inode>(fuse_ctx->uid, fuse_ctx->gid, mode | S_IFDIR);

		parent_d->add_new_child(*(get_filename_from_path(path).get()), i->get_ino());
		parent_d->sync();

		unique_ptr<dentry> new_d = make_unique<dentry>(i->get_ino(), true);
		new_d->add_new_child(".", i->get_ino());
		new_d->add_new_child("..", parent_i->get_ino());

		i->set_size(new_d->get_total_name_legth());
		i->sync();
		new_d->sync();

		parent_i->set_size(parent_d->get_total_name_legth());
		parent_i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::rmdir(const char* path) {
	global_logger.log(fuse_op, "Called rmdir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	try {
		unique_ptr<inode> parent_i = make_unique<inode>(*(get_parent_dir_path(path).get()));
		uint64_t parent_ino = parent_i->get_ino();

		unique_ptr<dentry> parent_d = make_unique<dentry>(parent_ino);

		ino_t target_ino = parent_d->get_child_ino(*(get_filename_from_path(path).get())); // perform target's permission check
		unique_ptr<inode> i = make_unique<inode>(target_ino);

		if(!S_ISDIR(i->get_mode()))
			return -ENOTDIR;

		unique_ptr<dentry> target_dentry = make_unique<dentry>(target_ino);
		if(target_dentry->get_child_num() > 2)
			return -ENOTEMPTY;

		meta_pool->remove(DENTRY, std::to_string(i->get_ino()));
		meta_pool->remove(INODE, std::to_string(i->get_ino()));

		parent_d->delete_child(*(get_filename_from_path(path).get()));
		parent_d->sync();

		parent_i->set_size(parent_d->get_total_name_legth());
		parent_i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}
	return 0;
}

int fuse_ops::rename(const char* old_path, const char* new_path, unsigned int flags) {
	global_logger.log(fuse_op, "Called rename()");
	global_logger.log(fuse_op, "src : " + std::string(old_path) + " dst : " + std::string(new_path));

	if(std::string(old_path) == std::string(new_path))
		return -EEXIST;

	try {
		unique_ptr<std::string> src_parent_path = get_parent_dir_path(old_path);
		unique_ptr<std::string> dst_parent_path = get_parent_dir_path(new_path);

		unique_ptr<std::string> old_name = get_filename_from_path(old_path);
		unique_ptr<std::string> new_name = get_filename_from_path(new_path);

		size_t paradox_check = std::string(new_path).find(old_path);
		if((paradox_check == 0) && (std::string(new_path).at(std::string(old_path).length()) == '/'))
			return -EINVAL;

		if (*src_parent_path == *dst_parent_path) {
			unique_ptr<inode> parent_i = make_unique<inode>(src_parent_path->data());

			unique_ptr<dentry> d = make_unique<dentry>(parent_i->get_ino());

			ino_t target_ino = d->get_child_ino(old_name->data());
			ino_t check_dst_ino = d->get_child_ino(new_name->data());

			if (flags == 0) {
				if(check_dst_ino != -1) {
					d->delete_child(new_name->data());
					meta_pool->remove(INODE, std::to_string(check_dst_ino));
				}
				d->delete_child(old_name->data());
				d->add_new_child(new_name->data(), target_ino);

				d->sync();
			} else {
				return -ENOSYS;
			}

			parent_i->set_size(d->get_total_name_legth());
		} else {
			unique_ptr<inode> src_parent_i = make_unique<inode>(src_parent_path->data());
			unique_ptr<inode> dst_parent_i = make_unique<inode>(dst_parent_path->data());

			unique_ptr<dentry> src_d = make_unique<dentry>(src_parent_i->get_ino());
			unique_ptr<dentry> dst_d = make_unique<dentry>(dst_parent_i->get_ino());

			ino_t target_ino = src_d->get_child_ino(old_name->data());
			ino_t check_dst_ino = dst_d->get_child_ino(new_name->data());

			if (flags == 0) {
				if(check_dst_ino != -1) {
					dst_d->delete_child(new_name->data());
					meta_pool->remove(INODE, std::to_string(check_dst_ino));
				}
				src_d->delete_child(old_name->data());
				dst_d->add_new_child(new_name->data(), target_ino);

				src_d->sync();
				dst_d->sync();
			} else {
				return -ENOSYS;
			}

			src_parent_i->set_size(src_d->get_total_name_legth());
			dst_parent_i->set_size(dst_d->get_total_name_legth());
		}

	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::open(const char* path, struct fuse_file_info* file_info){
	global_logger.log(fuse_op, "Called open()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	if(file_info->flags & O_APPEND) {
		throw std::runtime_error("O_APPEND is ON");
	}

	if(file_info->flags & O_NONBLOCK) {
		throw std::runtime_error("O_NONBLOCK is ON");
	}

	if(file_info->flags & O_SYNC) {
		throw std::runtime_error("O_SYNC is ON");
	}

	if(file_info->flags & O_ASYNC) {
		throw std::runtime_error("O_ASYNC is ON");
	}

	try {
		unique_ptr<inode> i = make_unique<inode>(path);

		if(S_ISDIR(i->get_mode()))
			return -EISDIR;

		std::scoped_lock<std::mutex> lock(m);
		unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());

		if(file_info->flags & O_TRUNC) {
			i->set_size(0);
			i->sync();
		}

		fh->set_fhno((void *)file_info->fh);
		fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::release(const char* path, struct fuse_file_info* file_info) {
	global_logger.log(fuse_op, "Called release()");
	if(path != nullptr) {
		global_logger.log(fuse_op, "path : " + std::string(path));

		unique_ptr<inode> i = make_unique<inode>(path);

		std::map<ino_t, unique_ptr<file_handler>>::iterator it;
		std::scoped_lock<std::mutex> lock(m);
		it = fh_list.find(i->get_ino());

		if (it == fh_list.end())
			return -EIO;

		fh_list.erase(it);
	} else {
		global_logger.log(fuse_op, "path : nullpath");
		file_handler *fh = reinterpret_cast<file_handler *>(file_info->fh);
		ino_t ino = fh->get_ino();

		std::map<ino_t, unique_ptr<file_handler>>::iterator it;
		std::scoped_lock<std::mutex> lock(m);
		it = fh_list.find(ino);

		if (it == fh_list.end())
			return -EIO;

		fh_list.erase(it);
	}
	return 0;
}

int fuse_ops::create(const char* path, mode_t mode, struct fuse_file_info* file_info) {
	global_logger.log(fuse_op, "Called create()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	if(S_ISDIR(mode))
		return -EISDIR;

	fuse_context *fuse_ctx = fuse_get_context();
	try {
		unique_ptr<inode> parent_i = make_unique<inode>(*(get_parent_dir_path(path).get()));
		uint64_t parent_ino = parent_i->get_ino();

		unique_ptr<dentry> parent_d = make_unique<dentry>(parent_ino);

		unique_ptr<inode> i = make_unique<inode>(fuse_ctx->uid, fuse_ctx->gid, mode | S_IFREG);
		i->sync();

		parent_d->add_new_child(*(get_filename_from_path(path).get()), i->get_ino());
		parent_d->sync();

		std::scoped_lock<std::mutex> lock(m);
		unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());

		fh->set_fhno((void *)file_info->fh);
		fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));

		parent_i->set_size(parent_d->get_total_name_legth());
		parent_i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::unlink(const char* path){
	global_logger.log(fuse_op, "Called unlink()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	unique_ptr<std::string> parent_name = get_parent_dir_path(path);
	unique_ptr<std::string> file_name = get_filename_from_path(path);
	try {
		unique_ptr<inode> parent_i = make_unique<inode>(parent_name->data());
		unique_ptr<dentry> parent_d = make_unique<dentry>(parent_i->get_ino());

		ino_t target_ino = parent_d->get_child_ino(file_name->data());
		unique_ptr<inode> i = make_unique<inode>(target_ino);

		nlink_t nlink = i->get_nlink() - 1;
		if (nlink == 0) {
			/* data */
			data_pool->remove(DATA, std::to_string(i->get_ino()));

			/* inode */
			meta_pool->remove(INODE, std::to_string(i->get_ino()));

			/* parent dentry */
			parent_d->delete_child(file_name->data());
			parent_d->sync();

			/* parent inode */
			parent_i->set_size(parent_d->get_total_name_legth());
			parent_i->sync();
		} else {
			i->set_nlink(nlink);
			i->sync();
		}
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}
	return 0;
}

int fuse_ops::read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* file_info) {
	global_logger.log(fuse_op, "Called read()");
	global_logger.log(fuse_op, "path : " + std::string(path) + " size : " + std::to_string(size) + " offset : " + std::to_string(offset));

	int read_len = 0;
	try {
		unique_ptr<inode> i = make_unique<inode>(path);

		read_len = data_pool->read(DATA, std::to_string(i->get_ino()), buffer, size, offset);
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	} catch(rados_io::no_such_object &e) {
		return e.num_bytes;
	}

	return read_len;
}

int fuse_ops::write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* file_info) {
	global_logger.log(fuse_op, "Called write()");
	global_logger.log(fuse_op, "path : " + std::string(path) + " size : " + std::to_string(size) + " offset : " + std::to_string(offset));

	int written_len = 0;

	try {
		unique_ptr<inode> i = make_unique<inode>(path);

		written_len = data_pool->write(DATA, std::to_string(i->get_ino()), buffer, size, offset);

		off_t updated_size = 0;
		if(offset >= i->get_size()) {
			updated_size = i->get_size() + size;
		} else {
			if(offset + size >= i->get_size()) {
				updated_size = offset + size;
			} else {
				updated_size = i->get_size();
			}
		}

		i->set_size(updated_size);
		i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return written_len;
}

int fuse_ops::chmod(const char* path, mode_t mode, struct fuse_file_info* file_info) {
	global_logger.log(fuse_op, "Called chmod()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	unique_ptr<std::string> parent_name = get_parent_dir_path(path);
	unique_ptr<std::string> file_name = get_filename_from_path(path);
	try {
		unique_ptr<inode> parent_i = make_unique<inode>(parent_name->data());
		unique_ptr<dentry> parent_d = make_unique<dentry>(parent_i->get_ino());

		ino_t target_ino = parent_d->get_child_ino(file_name->data());
		unique_ptr<inode> i = make_unique<inode>(target_ino);

		mode_t type = i->get_mode() & S_IFMT;
		i->set_mode(mode | type);

		i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* file_info){
	global_logger.log(fuse_op, "Called chown()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	unique_ptr<std::string> parent_name = get_parent_dir_path(path);
	unique_ptr<std::string> file_name = get_filename_from_path(path);
	try {
		unique_ptr<inode> parent_i = make_unique<inode>(parent_name->data());
		unique_ptr<dentry> parent_d = make_unique<dentry>(parent_i->get_ino());

		ino_t target_ino = parent_d->get_child_ino(file_name->data());
		unique_ptr<inode> i = make_unique<inode>(target_ino);

		i->set_uid(uid);
		i->set_gid(gid);

		i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
	global_logger.log(fuse_op, "Called utimens()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	unique_ptr<std::string> parent_name = get_parent_dir_path(path);
	unique_ptr<std::string> file_name = get_filename_from_path(path);
	try {
		unique_ptr<inode> parent_i = make_unique<inode>(parent_name->data());
		unique_ptr<dentry> parent_d = make_unique<dentry>(parent_i->get_ino());

		ino_t target_ino = parent_d->get_child_ino(file_name->data());
		unique_ptr<inode> i = make_unique<inode>(target_ino);

		i->set_atime(tv[0]);
		i->set_mtime(tv[1]);

		i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

fuse_operations fuse_ops::get_fuse_ops(void)
{
	fuse_operations fops;
	memset(&fops, 0, sizeof(fuse_operations));

	fops.init	= init;
	fops.destroy = destroy;
	fops.getattr = getattr;
	fops.access = access;
	fops.symlink = symlink;
	fops.readlink = readlink;

	fops.opendir = opendir;
	fops.releasedir = releasedir;

	fops.readdir = readdir;
	fops.mkdir = mkdir;
	fops.rmdir = rmdir;
	fops.rename = rename;

	fops.open = open;
	fops.release = release;

	fops.create = create;
	fops.unlink = unlink;

	fops.read = read;
	fops.write = write;

	fops.chmod = chmod;
	fops.chown = chown;
	fops.utimens = utimens;

	return fops;
}
