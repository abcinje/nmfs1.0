#include "directory_table.hpp"

directory_table::directory_table() {
	this->root_inode  = std::make_shared<inode>(0);
}

directory_table::~directory_table() {
	global_logger.log(directory_table_ops, "Called ~directory_table()");

	std::map<ino_t, shared_ptr<dentry_table>>::iterator it;
	for(it = this->dentry_tables.begin(); it != this->dentry_tables.end(); it++) {
		it->second = nullptr;
	}
}

shared_ptr<inode> directory_table::path_traversal(std::string path) {
	global_logger.log(directory_table_ops, "Called path_traverse(" + path + ")");

	shared_ptr<dentry_table> parent_dentry_table = this->get_dentry_table(0);
	shared_ptr<inode> parent_inode = this->get_root_inode();

	ino_t check_target_ino;
	shared_ptr<inode> target_inode;

	int start_name, end_name = -1;
	int path_len = path.length();

	while(true){
		// get new target name
		if(set_name_bound(start_name, end_name, path, path_len) == -1)
			break;

		std::string target_name = path.substr(start_name, end_name - start_name + 1);
		global_logger.log(directory_table_ops, "Check target: " + target_name);

		check_target_ino = parent_dentry_table->check_child_inode(target_name);
		if (check_target_ino == -1)
			throw inode::no_entry("No such file or Directory: in path traversal");
		else
			target_inode = parent_dentry_table->get_child_inode(target_name);

		if(S_ISDIR(target_inode->get_mode()))
			permission_check(target_inode, X_OK);

		parent_inode = target_inode;
		if(S_ISDIR(parent_inode->get_mode()))
			parent_dentry_table = this->get_dentry_table(target_inode->get_ino());
	}

	target_inode = parent_inode;
	return target_inode;
}

shared_ptr<dentry_table> directory_table::get_dentry_table(ino_t ino){
	global_logger.log(directory_table_ops, "get_dentry_table(" + std::to_string(ino) + ")");
	std::scoped_lock scl{this->directory_table_mutex};
	std::map<ino_t, shared_ptr<dentry_table>>::iterator it;
	it = this->dentry_tables.find(ino);

	if(it != this->dentry_tables.end()) { /* LOCAL, REMOTE */
		global_logger.log(directory_table_ops, "dentry_table : HIT");
		return it->second;
	}
	else { /* UNKNOWN */
		global_logger.log(directory_table_ops, "dentry_table : MISS");
		shared_ptr<inode> i = std::make_shared<inode>(ino);

		shared_ptr<dentry_table> new_dentry_table = become_leader(i->get_ino());
		this->add_dentry_table(ino, new_dentry_table);

		return new_dentry_table;
	}

}

int directory_table::add_dentry_table(ino_t ino, shared_ptr<dentry_table> dtable){
	global_logger.log(directory_table_ops, "Called add_dentry_table(" + std::to_string(ino) + ")");
	std::scoped_lock scl{this->directory_table_mutex};
	auto ret = dentry_tables.insert(std::make_pair(ino, nullptr));
	if(ret.second) {
		ret.first->second = dtable;
	} else {
		global_logger.log(directory_table_ops, "Already added dentry table is tried to inserted");
		return -1;
	}
	return 0;
}

int directory_table::delete_dentry_table(ino_t ino){
	global_logger.log(directory_table_ops, "Called delete_dentry_table(" + std::to_string(ino) + ")");
	std::map<ino_t, shared_ptr<dentry_table>>::iterator it;
	std::scoped_lock scl{this->directory_table_mutex};
	it = this->dentry_tables.find(ino);

	if(it == this->dentry_tables.end()) {
		global_logger.log(directory_table_ops, "Non-existed dentry table is tried to deleted");
		return -1;
	}

	this->dentry_tables.erase(it);

	return 0;
}

shared_ptr<inode> directory_table::get_root_inode() {
	std::scoped_lock scl{this->directory_table_mutex};
	return this->root_inode;
}