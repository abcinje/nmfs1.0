#include "logger.hpp"

#define CYAN "\033[0;36m"
#define RESET "\033[0m"

logger::logger() {
	std::cout.setf(std::ios::left);
}

void logger::log(enum code_location location, std::string message)
{
#ifdef DEBUG
	std::string location_str;

	std::scoped_lock scl{this->logger_mutex};
	switch(location){
		case fuse_op:
			location_str = "fuse_operations";
			message = CYAN + message + RESET;
			break;
		case local_fs_op:
			location_str = "local_fs_ops";
			break;
		case remote_fs_op:
			location_str = "remote_fs_ops";
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
		case rpc_client_ops:
			location_str = "rpc_client";
			break;
		case rpc_server_ops:
			location_str = "rpc_server";
			break;
		case manager_lease:
			location_str = "manager_lease";
			break;
		case manager_session:
			location_str = "manager_session";
			break;
		case file_handler_ops:
			location_str = "file_handler";
			break;
		case journal_ops:
			location_str = "journal";
			break;
		case journal_table_ops:
			location_str = "journal_table";
			break;
		case transaction_ops:
			location_str = "transaction";
			break;
		default:
			location_str = "Unknown";
	}

	std::cout << std::setw(20) <<"[" + location_str + "]";
	std::cout << message << std::endl;
#endif
}

logger global_logger;
