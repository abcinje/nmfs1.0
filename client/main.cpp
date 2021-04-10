#include "fs_ops/fuse_ops.hpp"

int main(int argc, char *argv[])
{
	fuse_operations fops = fuse_ops::get_fuse_ops();
	fuse_main(argc, argv, &fops, NULL);

	return 0;
}
