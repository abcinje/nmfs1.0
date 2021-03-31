#include "lease_util.hpp"

shared_ptr<dentry_table> become_leader(ino_t directory_ino) {
	shared_ptr<dentry_table> new_dentry_table = std::make_shared<dentry_table>(directory_ino);

	new_dentry_table->set_loc(LOCAL);

	fuse_context *fuse_ctx = fuse_get_context();
	client *c = (client *) (fuse_ctx->private_data);
	new_dentry_table->set_leader_id(c->get_client_id());

	new_dentry_table->pull_child_metadata();
	return new_dentry_table;
}

int retire_leader(ino_t directory_ino) {

}