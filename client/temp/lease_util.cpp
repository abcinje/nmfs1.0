#include "lease_util.hpp"

shared_ptr<dentry_table> become_leader(ino_t ino) {
	global_logger.log(lease_ops, "become_leader(" + std::to_string(ino) + ")");
	shared_ptr<dentry_table> new_dentry_table = std::make_shared<dentry_table>(ino, LOCAL);
	new_dentry_table->pull_child_metadata();

	return new_dentry_table;
}

shared_ptr<dentry_table> become_leader_of_new_dir(ino_t parnet_ino, ino_t ino){
	global_logger.log(lease_ops, "become_leader_of_new_dir(" + std::to_string(ino) + ")");
	shared_ptr<dentry_table> new_dentry_table = std::make_shared<dentry_table>(ino, LOCAL);

	shared_ptr<dentry> new_d = std::make_shared<dentry>(ino, true);

	new_dentry_table->set_dentries(new_d);
	new_d->sync();

	return new_dentry_table;
}

int retire_leader(ino_t directory_ino) {
	/* TODO */
	return 0;
}