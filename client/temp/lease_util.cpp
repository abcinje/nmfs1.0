#include "lease_util.hpp"

shared_ptr<dentry_table> become_leader(ino_t ino) {
	global_logger.log(lease_ops, "become_leader(" + std::to_string(ino) + ")");
	shared_ptr<dentry_table> new_dentry_table = std::make_shared<dentry_table>(ino, LOCAL);
	new_dentry_table->pull_child_metadata();

	fuse_context *fuse_ctx = fuse_get_context();
	client *c = (client *) (fuse_ctx->private_data);
	new_dentry_table->set_leader_id(c->get_client_id());

	return new_dentry_table;
}

shared_ptr<dentry_table> become_leader_of_new_dir(ino_t parnet_ino, ino_t ino){
	global_logger.log(lease_ops, "become_leader_of_new_dir(" + std::to_string(ino) + ")");
	shared_ptr<dentry_table> new_dentry_table = std::make_shared<dentry_table>(ino, LOCAL);

	fuse_context *fuse_ctx = fuse_get_context();
	client *c = (client *) (fuse_ctx->private_data);
	new_dentry_table->set_leader_id(c->get_client_id());

	shared_ptr<dentry> new_d = std::make_shared<dentry>(ino, true);

	new_dentry_table->set_dentries(new_d);
	new_d->sync();

	return new_dentry_table;
}

int retire_leader(ino_t directory_ino) {
	/* TODO */
	return 0;
}