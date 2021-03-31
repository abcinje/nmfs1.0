#include "directory_table.hpp"

shared_ptr<inode> directory_table::path_traversal(std::string path) {
	global_logger.log(indexing_ops, "Called path_traverse(" + path + ")");

	shared_ptr<dentry_table> parent_dentry_table = get_dentry_table(0);
	shared_ptr<inode> parent_inode = parent_dentry_table->get_dir_inode();

	shared_ptr<inode> target_inode;

	int start_name, end_name = -1;
	int path_len = path.length();

	while(true){
		// get new target name
		if(set_name_bound(start_name, end_name, path, path_len) == -1)
			break;

		std::string target_name = path.substr(start_name, end_name - start_name + 1);
		global_logger.log(indexing_ops, "Check target: " + target_name);

		target_inode = parent_dentry_table->get_child_inode(target_name);
		if (target_inode == nullptr)
			throw inode::no_entry("No such file or Directory: in path traversal");

		if(S_ISLNK(target_inode->get_mode())) {
			path = std::string(target_inode->get_link_target_name());
			if(path.at(0) == '/')
				end_name = -1;
			else
				end_name = -2;
			path_len = path.length();

			if(set_name_bound(start_name, end_name, path, path_len) == -1)
				throw std::runtime_error("Link Resolution Error");

			std::string origin_name = path.substr(start_name, end_name - start_name + 1);
			target_inode = parent_dentry_table->get_child_inode(origin_name);
		}

		if(S_ISDIR(target_inode->get_mode()))
			permission_check(target_inode.get(), X_OK);

		parent_inode = target_inode;
		if(S_ISDIR(parent_inode->get_mode()))
			parent_dentry_table = get_dentry_table(target_inode->get_ino());
	}

	target_inode = parent_inode;
	return target_inode;
}

shared_ptr<dentry_table> directory_table::get_dentry_table(ino_t ino){
	std::map<ino_t, shared_ptr<dentry_table>>::iterator it;
	it = this->dentry_tables.find(ino);

	if(it != this->dentry_tables.end()) /* LOCAL, REMOTE */
		return it->second;
	else { /* UNKNOWN */
		shared_ptr<inode> i = std::make_shared<inode>(ino);
		if(i->get_leader_id() == 0) { /* NOBODY -> LOCAL */
			fuse_context *fuse_ctx = fuse_get_context();
			client *c = (client *) (fuse_ctx->private_data);

			i->set_leader_id(c->get_client_id());
			i->sync();

			shared_ptr<dentry_table> new_dentry_table = become_leader(ino, i);
			return new_dentry_table;

		} else if(i->get_leader_id() > 0) { /* REMOTE */
			/* TODO */
		}
	}
}

int directory_table::create_table(ino_t ino){
	shared_ptr<dentry_table> dtable = std::make_shared<dentry_table>(ino, std::make_shared<inode>(ino));
	dentry_tables.insert(std::make_pair(ino, dtable));
}

int directory_table::delete_table(ino_t ino){
	std::map<ino_t, shared_ptr<dentry_table>>::iterator it;
	it = this->dentry_tables.find(ino);

	if(it == this->dentry_tables.end())
		return -1;

	this->dentry_tables.erase(it);

	return 0;
}