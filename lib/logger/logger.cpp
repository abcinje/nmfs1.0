#include "logger.hpp"

#define CYAN "\033[0;36m"
#define RESET "\033[0m"

void logger::log(enum code_location location, std::string message)
{
#ifdef DEBUG
	std::string location_str;

	switch(location){
		case fuse_op:
			location_str = "fuse_operations";
			message = CYAN + message + RESET;
			break;
		case local_fs_op:
			location_str = "local_fs_operations";
			message = CYAN + message + RESET;
			break;
		case rados_io_ops:
			location_str = "rados_io_ops";
			break;
		case dentry_ops:
			location_str = "dentry_ops";
			break;
		case inode_ops:
			location_str = "inode_operations";
			break;
		case directory_table_ops:
			location_str = "directory_table";
			break;
		case dentry_table_ops:
			location_str = "dentry_table";
			break;
		case lease_ops:
			location_str = "lease_operation";
			break;
		default:
			location_str = "Unknown";
	}

	std::cout<< "[" + location_str + "]\t" + message << std::endl;
#endif
}

logger global_logger;
