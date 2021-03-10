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

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define META_POOL "nmfs.meta"
#define DATA_POOL "nmfs.data"

rados_io *meta_pool;
rados_io *data_pool;

std::map<ino_t, unique_ptr<file_handler>> fh_list;

void *fuse_ops::init(struct fuse_conn_info *info, struct fuse_config *config)
{
	global_logger.log(fuse_op, "Called init()");
	client *this_client;

	rados_io::conn_info ci = {"client.admin", "ceph", 0};
	meta_pool = new rados_io(ci, META_POOL);
	data_pool = new rados_io(ci, DATA_POOL);

	/* client id allocation */
	if (!meta_pool->exist("client.list")) {
		meta_pool->write("client.list", "?o", 2, 0);
		this_client = new client(1);
	} else {
		this_client = new client();
	}

	/* root */
	if (!meta_pool->exist("i$0")) {
		fuse_context *fuse_ctx = fuse_get_context();
		inode i(fuse_ctx->uid, fuse_ctx->gid, S_IFDIR | 0755);
		i.set_ino(0);

		dentry d(0, true);
		d.add_new_child(".", 0);
		d.add_new_child("..", 0);

		i.set_size(d.get_total_name_legth());
		i.sync();
		d.sync();
	}

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
			inode *i = new inode(0);
			i->fill_stat(stat);

			delete i;
		} else {
			inode *parent_i = new inode(parent_name->data());
			dentry *parent_d = new dentry(parent_i->get_ino());

			ino_t target_ino = parent_d->get_child_ino(file_name->data());
			inode *i = new inode(target_ino);
			i->fill_stat(stat);

			delete i;
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

	fuse_context *fuse_ctx = fuse_get_context();

	try {
		inode *i = new inode(path);
		bool ret = permission_check(i, mask);

		delete i;

		if(!ret)
			return -EACCES;

	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
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
		inode *dst_parent_i = new inode(dst_parent_name->data());
		dentry *dst_parent_d = new dentry(dst_parent_i->get_ino());

		unique_ptr<std::string> symlink_name = get_filename_from_path(dst);

		if(dst_parent_d->get_child_ino(symlink_name->data()) != -1) {
			delete dst_parent_d;
			delete dst_parent_i;
			return -EEXIST;
		}

		inode *symlink_i = new inode(fuse_ctx->uid, fuse_ctx->gid, S_IFLNK | 0777, std::string(src).length(), src);
		symlink_i->set_size(std::string(src).length());

		dst_parent_d->add_new_child(symlink_name->data(), symlink_i->get_ino());

		dst_parent_d->sync();
		symlink_i->sync();

		dst_parent_i->set_size(dst_parent_d->get_total_name_legth());
		dst_parent_i->sync();

		delete dst_parent_d;
		delete dst_parent_i;
		delete symlink_i;
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
		inode *parent_i = new inode(parent_name->data());
		dentry *parent_d = new dentry(parent_i->get_ino());

		ino_t target_ino = parent_d->get_child_ino(file_name->data());
		inode *i = new inode(target_ino);

		if (!S_ISLNK(i->get_mode())) {
			delete parent_i;
			delete parent_d;
			delete i;

			return -EINVAL;
		}

		size_t len = MIN(i->get_link_target_len(), size-1);
		memcpy(buf, i->get_link_target_name(), len);
		buf[len] = '\0';

		delete parent_i;
		delete parent_d;
		delete i;
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
		inode *i = new inode(path);

		if(!S_ISDIR(i->get_mode())) {
			delete i;
			return -ENOTDIR;
		}

		unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());

		fh->set_fhno((void *)file_info->fh);
		fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));

		delete i;
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::releasedir(const char* path, struct fuse_file_info* file_info){
	global_logger.log(fuse_op, "Called releasedir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	inode *i = new inode(path);

	std::map<ino_t, unique_ptr<file_handler>>::iterator it;
	it = fh_list.find(i->get_ino());

	if(it == fh_list.end())
		return -EIO;

	fh_list.erase(it);

	delete i;
	return 0;
}

int fuse_ops::readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* file_info, enum fuse_readdir_flags readdir_flags){
	global_logger.log(fuse_op, "Called readdir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	inode *i = new inode(path);
	dentry *d = new dentry(i->get_ino());

	d->fill_filler(buffer, filler);

	delete i;
	delete d;
	return 0;
}
int fuse_ops::mkdir(const char* path, mode_t mode){
	global_logger.log(fuse_op, "Called mkdir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	fuse_context *fuse_ctx = fuse_get_context();
	try {
		inode *parent_i = new inode(*(get_parent_dir_path(path).get()));
		uint64_t parent_ino = parent_i->get_ino();

		dentry *parent_d = new dentry(parent_ino);

		inode *i = new inode(fuse_ctx->uid, fuse_ctx->gid, mode | S_IFDIR);

		parent_d->add_new_child(*(get_filename_from_path(path).get()), i->get_ino());
		parent_d->sync();

		dentry *new_d = new dentry(i->get_ino(), true);
		new_d->add_new_child(".", i->get_ino());
		new_d->add_new_child("..", parent_i->get_ino());

		i->set_size(new_d->get_total_name_legth());
		i->sync();
		new_d->sync();

		parent_i->set_size(parent_d->get_total_name_legth());
		parent_i->sync();

		delete parent_i;
		delete parent_d;
		delete i;
		delete new_d;
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
		inode *parent_i = new inode(*(get_parent_dir_path(path).get()));
		uint64_t parent_ino = parent_i->get_ino();

		dentry *parent_d = new dentry(parent_ino);

		ino_t target_ino = parent_d->get_child_ino(*(get_filename_from_path(path).get())); // perform target's permission check
		inode *i = new inode(target_ino);

		if(!S_ISDIR(i->get_mode()))
			return -ENOTDIR;

		dentry *target_dentry = new dentry(target_ino);
		if(target_dentry->get_child_num())
			return -ENOTEMPTY;

		meta_pool->remove("d$" + std::to_string(i->get_ino()));
		meta_pool->remove("i$" + std::to_string(i->get_ino()));

		parent_d->delete_child(*(get_filename_from_path(path).get()));
		parent_d->sync();

		parent_i->set_size(parent_d->get_total_name_legth());
		parent_i->sync();

		delete parent_i;
		delete parent_d;
		delete i;
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
			inode *parent_i = new inode(src_parent_path->data());

			dentry *d = new dentry(parent_i->get_ino());

			ino_t target_ino = d->get_child_ino(old_name->data());
			ino_t check_dst_ino = d->get_child_ino(new_name->data());

			if (flags == 0) {
				if(check_dst_ino != -1) {
					d->delete_child(new_name->data());
					meta_pool->remove("i$" + std::to_string(check_dst_ino));
				}
				d->delete_child(old_name->data());
				d->add_new_child(new_name->data(), target_ino);

				d->sync();
			} else {
				return -ENOSYS;
			}

			parent_i->set_size(d->get_total_name_legth());
			delete parent_i;
			delete d;

		} else {
			inode *src_parent_i = new inode(src_parent_path->data());
			inode *dst_parent_i = new inode(dst_parent_path->data());

			dentry *src_d = new dentry(src_parent_i->get_ino());
			dentry *dst_d = new dentry(dst_parent_i->get_ino());

			ino_t target_ino = src_d->get_child_ino(old_name->data());
			ino_t check_dst_ino = dst_d->get_child_ino(new_name->data());

			if (flags == 0) {
				if(check_dst_ino != -1) {
					dst_d->delete_child(new_name->data());
					meta_pool->remove("i$" + std::to_string(check_dst_ino));
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

			delete src_parent_i;
			delete dst_parent_i;
			delete src_d;
			delete dst_d;
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

	try {
		inode *i = new inode(path);

		if(S_ISDIR(i->get_mode())) {
			delete i;
			return -EISDIR;
		}

		unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());

		fh->set_fhno((void *)file_info->fh);
		fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));

		delete i;
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::release(const char* path, struct fuse_file_info* file_info) {
	global_logger.log(fuse_op, "Called release()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	inode *i = new inode(path);

	std::map<ino_t, unique_ptr<file_handler>>::iterator it;
	it = fh_list.find(i->get_ino());

	if(it == fh_list.end()) {
		delete i;
		return -EIO;
	}

	fh_list.erase(it);

	delete i;
	return 0;
}

int fuse_ops::create(const char* path, mode_t mode, struct fuse_file_info* file_info) {
	global_logger.log(fuse_op, "Called create()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	if(S_ISDIR(mode))
		return -EISDIR;

	fuse_context *fuse_ctx = fuse_get_context();
	try {
		inode *parent_i = new inode(*(get_parent_dir_path(path).get()));
		uint64_t parent_ino = parent_i->get_ino();

		dentry *parent_d = new dentry(parent_ino);

		inode *i = new inode(fuse_ctx->uid, fuse_ctx->gid, mode | S_IFREG);
		i->sync();

		parent_d->add_new_child(*(get_filename_from_path(path).get()), i->get_ino());
		parent_d->sync();

		unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
		file_info->fh = reinterpret_cast<uint64_t>(fh.get());

		fh->set_fhno((void *)file_info->fh);
		fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));

		parent_i->set_size(parent_d->get_total_name_legth());
		parent_i->sync();

		delete parent_i;
		delete parent_d;
		delete i;
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

	try {
		inode *parent_i = new inode(*(get_parent_dir_path(path).get()));
		uint64_t parent_ino = parent_i->get_ino();

		dentry *parent_d = new dentry(parent_ino);

		ino_t target_ino = parent_d->get_child_ino(*(get_filename_from_path(path).get())); // perform target's permission check
		inode *i = new inode(target_ino);

		meta_pool->remove("i$" + std::to_string(i->get_ino()));

		parent_d->delete_child(*(get_filename_from_path(path).get()));
		parent_d->sync();

		parent_i->set_size(parent_d->get_total_name_legth());
		parent_i->sync();

		delete parent_i;
		delete parent_d;
		delete i;
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
		inode *i = new inode(path);

		read_len = data_pool->read(std::to_string(i->get_ino()), buffer, size, offset);
		delete i;
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	} catch(rados_io::no_such_object &e) {
		return e.num_bytes;
	}

	return read_len;
}

int fuse_ops::write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* file_info){
	global_logger.log(fuse_op, "Called write()");
	global_logger.log(fuse_op, "path : " + std::string(path) + " size : " + std::to_string(size) + " offset : " + std::to_string(offset));

	int written_len = 0;

	try {
		inode *i = new inode(path);

		written_len = data_pool->write(std::to_string(i->get_ino()), buffer, size, offset);

		off_t updated_size;
		if(offset >= i->get_size())
			updated_size = i->get_size() + size;
		else if (offset < i->get_size()) {
			if(offset + size >= i->get_size())
				updated_size = offset + size;
		}

		i->set_size(updated_size);
		i->sync();

		delete i;
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
		inode *parent_i = new inode(parent_name->data());
		dentry *parent_d = new dentry(parent_i->get_ino());

		ino_t target_ino = parent_d->get_child_ino(file_name->data());
		inode *i = new inode(target_ino);

		mode_t type = i->get_mode() & S_IFMT;
		i->set_mode(mode | type);

		i->sync();
		delete i;
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
		inode *parent_i = new inode(parent_name->data());
		dentry *parent_d = new dentry(parent_i->get_ino());

		ino_t target_ino = parent_d->get_child_ino(file_name->data());
		inode *i = new inode(target_ino);

		i->set_uid(uid);
		i->set_gid(gid);

		i->sync();
		delete i;
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
		inode *parent_i = new inode(parent_name->data());
		dentry *parent_d = new dentry(parent_i->get_ino());

		ino_t target_ino = parent_d->get_child_ino(file_name->data());
		inode *i = new inode(target_ino);

		i->set_atime(tv[0]);
		i->set_mtime(tv[1]);

		i->sync();
		delete i;
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
