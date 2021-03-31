#include "lease_util.hpp"

shared_ptr<dentry_table> become_leader(ino_t directory_ino, shared_ptr<inode> dir_inode) {
	shared_ptr<dentry_table> new_dentry_table = std::make_shared<dentry_table>(directory_ino, dir_inode);
	new_dentry_table->pull_child_metadata();

	new_dentry_table->set_loc(LOCAL);
	fuse_context *fuse_ctx = fuse_get_context();
	client *c = (client *) (fuse_ctx->private_data);
	new_dentry_table->set_leader_id(c->get_client_id());

	return new_dentry_table;
}

int retire_leader(ino_t directory_ino) {

}