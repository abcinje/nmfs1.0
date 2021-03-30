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
		global_logger.log(inode_ops, "Check target: " + target_name);

		target_inode = parent_dentry_table->get_child_inode(target_name);
		if (target_inode == nullptr)
			throw inode::no_entry("No such file or Directory: in path traversal");

		if(S_ISLNK(target_inode->get_mode())) {
			/* TODO */
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
		unique_ptr<inode> i = std::make_unique<inode>(ino);
		if(i->get_leader_id() == 0) { /* NOBODY -> LOCAL */
			/* TODO */
		} else if(i->get_leader_id() > 0) { /* REMOTE */
			/* TODO */
		}
	}
}

int directory_table::create_table(ino_t ino){
	unique_ptr<dentry_table> dtable = std::make_unique<dentry_table>(ino);
	dentry_tables.insert(std::make_pair(ino, std::move(dtable)));
}

int directory_table::delete_table(ino_t ino){
	std::map<ino_t, shared_ptr<dentry_table>>::iterator it;
	it = this->dentry_tables.find(ino);

	if(it == this->dentry_tables.end())
		return -1;

	this->dentry_tables.erase(it);

	return 0;
}