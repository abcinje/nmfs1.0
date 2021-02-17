#include "fuse_ops.hpp"
#include "meta/inode.hpp"
#include "logger/logger.hpp"
#include "rados_io/rados_io.hpp"

#include <cstring>

#define META_POOL "nmfs.meta"
#define DATA_POOL "nmfs.data"

rados_io *meta_pool;
rados_io *data_pool;

void *fuse_ops::init(struct fuse_conn_info *info, struct fuse_config *config)
{
	global_logger.log("Called init()");

	rados_io::conn_info ci = {"client.admin", "ceph", 0};
	meta_pool = new rados_io(ci, META_POOL);
	data_pool = new rados_io(ci, DATA_POOL);

	/* root */
	if (!meta_pool->exist("i$/")) {
		fuse_context *fuse_ctx = fuse_get_context();
		inode i(fuse_ctx->uid, fuse_ctx->gid, S_IFDIR | 0755);
		auto value = i.serialize();
		meta_pool->write("i$/", value.get(), sizeof(inode), 0);
	}

	return nullptr;
}

void fuse_ops::destroy(void *private_data)
{
	global_logger.log("Called destroy()");

	delete meta_pool;
	delete data_pool;
}

fuse_operations fuse_ops::get_fuse_ops(void)
{
	fuse_operations fops;
	memset(&fops, 0, sizeof(fuse_operations));

	fops.init	= init;
	fops.destroy	= destroy;

	return fops;
}
