#include "fuse_ops.hpp"
#include "meta/inode.hpp"
#include "logger/logger.hpp"
#include "rados_io/rados_io.hpp"
#include "client/client.hpp"

#include <cstring>

#define META_POOL "nmfs.meta"
#define DATA_POOL "nmfs.data"

rados_io *meta_pool;
rados_io *data_pool;

void *fuse_ops::init(struct fuse_conn_info *info, struct fuse_config *config)
{
	global_logger.log("Called init()");
	client *this_client;

	/* client id allocation */
	if (!meta_pool->exist("client.list")) {
		meta_pool->write("client.list", "o", 1, 0);
		this_client = new client(0);
	} else {
		this_client = new client();
	}

	rados_io::conn_info ci = {"client.admin", "ceph", 0};
	meta_pool = new rados_io(ci, META_POOL);
	data_pool = new rados_io(ci, DATA_POOL);

	/* root */
	if (!meta_pool->exist("i$/")) {
		fuse_context *fuse_ctx = fuse_get_context();
		inode i(fuse_ctx->uid, fuse_ctx->gid, S_IFDIR | 0755);
		auto value = i.serialize();
		meta_pool->write("i$/", value.get(), sizeof(inode), 0);
		meta_pool->write("d$/", "" , 0, 0);
	}

	return (void *)this_client;
}

void fuse_ops::destroy(void *private_data)
{
	global_logger.log("Called destroy()");

	delete meta_pool;
	delete data_pool;

	fuse_context *fuse_ctx = fuse_get_context();
	delete (client *)(fuse_ctx->private_data);
}

int getattr(const char* path, struct stat* stat, struct fuse_file_info* file_info) {
	global_logger.log("Called getattr()");

	try {
		inode *i = new inode(path);
		i->fill_stat(stat);

	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int access(const char* path, int mask) {
	global_logger.log("Called access()");

	fuse_context *fuse_ctx = fuse_get_context();

	try {
		inode *i = new inode(path);

		// uid gid check
		if((fuse_ctx->uid != i->get_uid()) || (fuse_ctx->gid != i->get_gid()))
			return -EACCES;

		// mode check
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}
int opendir(const char* path, struct fuse_file_info* file_info){
	global_logger.log("Called opendir()");
	try {
		inode *i = new inode(path);

		if(!S_ISDIR(i->get_mode()))
			return -ENOTDIR;

		//file_info->fh = ???;
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}
}
int releasedir(const char* path, struct fuse_file_info* file_info){
	global_logger.log("Called releasedir()");
	return 0;
}

int readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* file_info, enum fuse_readdir_flags readdir_flags);
int mkdir(const char* path, mode_t mode);
int rmdir(const char* path);
int rename(const char* old_path, const char* new_path, unsigned int flags);

int open(const char* path, struct fuse_file_info* file_info){
	global_logger.log("Called open()");

	try {
		inode *i = new inode(path);

		if(S_ISDIR(i->get_mode()))
			return -EISDIR;

		//file_info->fh = ???;
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}
}

int release(const char* path, struct fuse_file_info* file_info) {
	global_logger.log("Called release()");

	return 0;
}

int create(const char* path, mode_t mode, struct fuse_file_info* file_info) {
	global_logger.log("Called create()");

	fuse_context *fuse_ctx = fuse_get_context();
	try {
		inode *i = new inode(fuse_ctx->uid, fuse_ctx->gid, mode | S_IFREG);

	} catch(std::runtime_error &e) {
		return -EIO;
	}

	return 0;
}

int unlink(const char* path){
	global_logger.log("Called unlink()");
	return 0;
}

int read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* file_info);
int write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* file_info);

int chmod(const char* path, mode_t mode, struct fuse_file_info* file_info) {
	try {
		inode *i = new inode(path);

		mode_t type = i->get_mode() & S_IFMT;
		i->set_mode(mode | type);

		i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}
}

int chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* file_info){
	try {
		inode *i = new inode(path);

		i->set_uid(uid);
		i->set_gid(gid);

		i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}
}
int utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
	try {
		inode *i = new inode(path);

		i->set_atime(tv[0]);
		i->set_mtime(tv[1]);

		i->sync();
	} catch(inode::no_entry &e) {
		return -ENOENT;
	} catch(inode::permission_denied &e) {
		return -EACCES;
	}
}

fuse_operations fuse_ops::get_fuse_ops(void)
{
	fuse_operations fops;
	memset(&fops, 0, sizeof(fuse_operations));

	fops.init	= init;
	fops.destroy	= destroy;
	fops.getattr = getattr;
	fops.access = access;

	//fops.opendir = opendir;
	fops.releasedir = releasedir;

	//fops.readdir = readdir;
	//fops.mkdir = mkdir;
	//fops.rmdir = rmdir;
	//fops.rename = rename;

	//fops.open = open;
	fops.release = release;

	//fops.create = create;
	fops.unlink = unlink;

	//fops.read = read;
	//fops.write = write;

	fops.chmod = chmod;
	fops.chown = chown;
	fops.utimens = utimens;

	return fops;
}
