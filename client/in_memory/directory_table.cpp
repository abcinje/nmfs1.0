#include "directory_table.hpp"

extern std::unique_ptr<lease_client> lc;
extern std::shared_ptr<inode> root_inode;
static int set_name_bound(int &start_name, int &end_name, const std::string &path, int path_len){
	start_name = end_name + 2;
	if(start_name >= path_len)
		return -1;

	int i;
	for(i = start_name; i < path_len; i++){
		if(path.at(i) == '/'){
			end_name = i - 1;
			break;
		}
	}
	if(i == path_len)
		end_name = i - 1;

	return 0;
}

directory_table::directory_table() {
	shared_ptr<dentry_table> root_dentry_table = this->get_dentry_table(0);
	root_inode  = std::make_shared<inode>(0);
	if(root_dentry_table->get_loc() == LOCAL)
		root_inode->set_loc(LOCAL);
	else
		root_inode->set_loc(REMOTE);
}

directory_table::~directory_table() {
	global_logger.log(directory_table_ops, "Called ~directory_table()");

	std::map<ino_t, shared_ptr<dentry_table>>::iterator it;
	for(it = this->dentry_tables.begin(); it != this->dentry_tables.end(); it++) {
		it->second = nullptr;
	}
}

shared_ptr<inode> directory_table::path_traversal(const std::string &path) {
	global_logger.log(directory_table_ops, "Called path_traverse(" + path + ")");

	std::scoped_lock scl{this->directory_table_mutex};
	shared_ptr<dentry_table> parent_dentry_table = this->get_dentry_table(0);
	shared_ptr<inode> parent_inode = root_inode;

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
		/* TODO : ino_t cannot be -1 */
		if (check_target_ino == -1)
			throw inode::no_entry("No such file or Directory: in path traversal");
		else
			target_inode = parent_dentry_table->get_child_inode(target_name);

		if(target_inode == nullptr)
			throw std::runtime_error("Failed to make remote_inode in path_traversal()");

		if(S_ISDIR(target_inode->get_mode()))
			target_inode->permission_check(X_OK);

		parent_inode = target_inode;
		if(S_ISDIR(parent_inode->get_mode()))
			parent_dentry_table = this->get_dentry_table(check_target_ino);
	}

	if(parent_dentry_table->get_loc() == LOCAL) {
		global_logger.log(directory_table_ops, "Return Local Inode");
		target_inode = parent_inode;
	} else if(parent_dentry_table->get_loc() == REMOTE){
		global_logger.log(directory_table_ops, "Return Remote Inode");
		target_inode = parent_inode;
		if(path != "/")
			target_inode->set_ino(check_target_ino);
	}

	return target_inode;
}

shared_ptr<dentry_table> directory_table::lease_dentry_table(ino_t ino){
	global_logger.log(directory_table_ops, "Called lease_dentry_table(" + std::to_string(ino) + ")");
	std::string temp_address;
	int ret = lc->acquire(ino, temp_address);
	shared_ptr<dentry_table> new_dentry_table;
	if(ret == 0) {
		global_logger.log(directory_table_ops, "Success to acquire lease");
		/* Success to acquire lease */
		new_dentry_table = std::make_shared<dentry_table>(ino, LOCAL);
		new_dentry_table->pull_child_metadata();
		this->add_dentry_table(ino, new_dentry_table);
	} else if(ret == -1) {
		global_logger.log(directory_table_ops, "Fail to acquire lease, this dir already has the leader");
		global_logger.log(directory_table_ops, "Leader Address: " + temp_address);
		/* Fail to acquire lease, this dir already has the leader */
		new_dentry_table = std::make_shared<dentry_table>(ino, REMOTE);
		new_dentry_table->set_leader_id(temp_address);
		this->add_dentry_table(ino, new_dentry_table);
	}

	return new_dentry_table;
}

shared_ptr<dentry_table> directory_table::get_dentry_table(ino_t ino){
	global_logger.log(directory_table_ops, "get_dentry_table(" + std::to_string(ino) + ")");
	std::scoped_lock scl{this->directory_table_mutex};
	std::map<ino_t, shared_ptr<dentry_table>>::iterator it;
	it = this->dentry_tables.find(ino);

	if(it != this->dentry_tables.end()) { /* LOCAL, REMOTE */
		global_logger.log(directory_table_ops, "dentry_table : HIT");
		bool accessible = lc->access(ino);
		if(accessible) {
			return it->second;
		} else {
			this->dentry_tables.erase(it);
			shared_ptr<dentry_table> new_dentry_table = lease_dentry_table(ino);
			return new_dentry_table;
		}
	}
	else { /* UNKNOWN */
		/*
		 * if the directory has no leader, make local dentry_table
		 * if the directory already has leader, make remote dentry_table and fill loc and leader_ip
		 */
		global_logger.log(directory_table_ops, "dentry_table : MISS");
		shared_ptr<dentry_table> new_dentry_table = lease_dentry_table(ino);
		return new_dentry_table;
	}

}

uint64_t directory_table::check_dentry_table(ino_t ino){
	global_logger.log(directory_table_ops, "check_dentry_table(" + std::to_string(ino) + ")");
	std::scoped_lock scl{this->directory_table_mutex};
	std::map<ino_t, shared_ptr<dentry_table>>::iterator it;
	it = this->dentry_tables.find(ino);

	if(it != this->dentry_tables.end()) { /* LOCAL, REMOTE */
		global_logger.log(directory_table_ops, "dentry_table : HIT");
		return it->second->get_loc();
	}
	else { /* UNKNOWN */
		global_logger.log(directory_table_ops, "dentry_table : MISS");
		return UNKNOWN;
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
