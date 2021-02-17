#include "fuse_ops.hpp"

#include <cstring>

void* fuse_ops::init(struct fuse_conn_info* info, struct fuse_config *config)
{
	return nullptr;
}

void fuse_ops::destroy(void* private_data)
{
}

fuse_operations fuse_ops::get_fuse_ops(void)
{
	fuse_operations fops;
	memset(&fops, 0, sizeof(fuse_operations));

	fops.init	= init;
	fops.destroy	= destroy;

	return fops;
}
