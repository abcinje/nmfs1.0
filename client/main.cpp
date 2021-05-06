#include "fs_ops/fuse_ops.hpp"

int main(int argc, char *argv[])
{
	/* ./nmfs ARGS MOUNT_POINT "MANAGE_IP:PORT,REMOTE_IP:PORT" */
	fuse_operations fops = fuse_ops::get_fuse_ops();
	fuse_main(argc-1, argv, &fops, argv[argc-1]);
	return 0;
}
