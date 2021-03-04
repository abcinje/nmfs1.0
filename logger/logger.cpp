#include "logger.hpp"

void logger::log(enum code_location location, std::string message)
{
	std::string* location_str;

	switch(location){
		case fuse_op:
			location_str = new std::string("fuse_operations");
			break;
		case rados_io_ops:
			location_str = new std::string("rados_io_ops");
			break;
		case dentry_ops:
			location_str = new std::string("dentry_ops");
			break;
		case inode_ops:
			location_str = new std::string("inode_operations");
			break;
		default:
			location_str = new std::string("Unknown");
	}

	std::cout<< "[" << *location_str + "]\t " + message << std::endl;

	free(location_str);
}

logger global_logger;
