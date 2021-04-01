#include "lease_util.hpp"

shared_ptr<dentry_table> become_leader(shared_ptr<inode> dir_inode) {
	global_logger.log(lease_ops, "become_leader(" + std::to_string(dir_inode->get_ino()) + ")");
	shared_ptr<dentry_table> new_dentry_table = std::make_shared<dentry_table>(dir_inode->get_ino(), dir_inode);
	new_dentry_table->pull_child_metadata();

	new_dentry_table->set_loc(LOCAL);
	fuse_context *fuse_ctx = fuse_get_context();
	client *c = (client *) (fuse_ctx->private_data);
	new_dentry_table->set_leader_id(c->get_client_id());

	return new_dentry_table;
}

shared_ptr<dentry_table> become_leader_of_new_dir(shared_ptr<inode> parent_inode, shared_ptr<inode> dir_inode){
	global_logger.log(lease_ops, "become_leader_of_new_dir(" + std::to_string(dir_inode->get_ino()) + ")");
	shared_ptr<dentry_table> new_dentry_table = std::make_shared<dentry_table>(dir_inode->get_ino(), dir_inode);

	shared_ptr<dentry> new_d = std::make_shared<dentry>(dir_inode->get_ino(), true);
	new_d->add_new_child(".", dir_inode->get_ino());
	new_d->add_new_child("..", parent_inode->get_ino());

	new_dentry_table->set_dentries(new_d);
	new_d->sync();

	return new_dentry_table;
}

int retire_leader(ino_t directory_ino) {
	/* TODO */
	return 0;
}