#ifndef _LOGGER_HPP_
#define _LOGGER_HPP_

#include <iostream>
#include <string>

enum code_location {
	fuse_op = 0,
	rados_io_ops,
	dentry_ops,
	inode_ops,
	directory_table_ops,
	dentry_table_ops
};


class logger {
public:
	void log( enum code_location location, std::string message);
};

extern logger global_logger;

#endif /* _LOGGER_HPP_ */
