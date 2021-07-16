#include <climits>
#include <iostream>
#include <unistd.h>

#include "fs_ops/fuse_ops.hpp"

int main(int argc, char *argv[])
{
	/* Get realpath of config file */
	char path[PATH_MAX];
	if (!realpath(argv[argc-1], path)) {
		std::cerr << "Failed to resolve the path of config file." << std::endl;
		exit(1);
	}

	/* ./nmfs ARGS MOUNT_POINT CONFIG_PATH */
	fuse_operations fops = fuse_ops::get_fuse_ops();
	fuse_main(argc-1, argv, &fops, path);

	return 0;
}
