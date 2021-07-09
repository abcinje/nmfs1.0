#include "directory_table.hpp"

extern std::unique_ptr<lease_client> lc;
extern std::unique_ptr<journal> journalctl;

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
	shared_ptr<dentry_table> root_dentry_table = this->get_dentry_table(get_root_ino());
}

directory_table::~directory_table() {
	global_logger.log(directory_table_ops, "Called ~directory_table()");

	for(auto it = this->dentry_tables.begin(); it != this->dentry_tables.end(); it++) {
		it.value() = nullptr;
	}
}

shared_ptr<inode> directory_table::path_traversal(const std::string &path) {
	global_logger.log(directory_table_ops, "Called path_traverse(" + path + ")");

	shared_ptr<dentry_table> parent_dentry_table = this->get_dentry_table(get_root_ino());
	shared_ptr<inode> target_inode = parent_dentry_table->get_this_dir_inode();;
	uuid check_target_ino;

	int start_name, end_name = -1;
	int path_len = static_cast<int>(path.length());

	while(true){
		// get new target name
		if(set_name_bound(start_name, end_name, path, path_len) == -1)
			break;

		std::string target_name = path.substr(start_name, end_name - start_name + 1);
		global_logger.log(directory_table_ops, "Check target: " + target_name);
		{
			std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
			check_target_ino = parent_dentry_table->check_child_inode(target_name);

			if (check_target_ino.is_nil())
				throw inode::no_entry("No such file or Directory: in path traversal");
			else
				/* if target is dir, this child is just for checking mode.
				 * if target is reg, this child is actual inode */
				target_inode = parent_dentry_table->get_child_inode(target_name, check_target_ino);

			if(target_inode == nullptr)
				throw std::runtime_error("Failed to make remote_inode in path_traversal()");

			if (S_ISDIR(target_inode->get_mode())) {
				parent_dentry_table = this->get_dentry_table(check_target_ino);
				target_inode = parent_dentry_table->get_this_dir_inode();
				target_inode->permission_check(X_OK);
			}
		}
	}

	return target_inode;
}

shared_ptr<dentry_table> directory_table::lease_dentry_table(uuid ino){
	global_logger.log(directory_table_ops, "Called lease_dentry_table(" + uuid_to_string(ino) + ")");
	std::scoped_lock scl{this->directory_table_mutex};

	std::string temp_address;
	int ret = lc->acquire(ino, temp_address);
	shared_ptr<dentry_table> new_dentry_table = nullptr;
	if(ret == 0) {
		global_logger.log(directory_table_ops, "Success to acquire lease");
		/* Check whether the directory needs recovery */
		//journalctl->check(ino);

		/* Success to acquire lease */
		new_dentry_table = std::make_shared<dentry_table>(ino, LOCAL);
		new_dentry_table->set_leader_ip(temp_address);
		new_dentry_table->pull_child_metadata();
		this->add_dentry_table(ino, new_dentry_table);
	} else if(ret == -1) {
		global_logger.log(directory_table_ops, "Fail to acquire lease, this dir already has the leader");
		global_logger.log(directory_table_ops, "Leader Address: " + temp_address);
		/* Fail to acquire lease, this dir already has the leader */
		new_dentry_table = std::make_shared<dentry_table>(ino, REMOTE);
		new_dentry_table->set_leader_ip(temp_address);
		this->add_dentry_table(ino, new_dentry_table);
	}

	return new_dentry_table;
}

shared_ptr<dentry_table> directory_table::lease_dentry_table_mkdir(std::shared_ptr<inode> new_dir_inode, std::shared_ptr<dentry> new_dir_dentry) {
	global_logger.log(directory_table_ops, "Called lease_dentry_table_mkdir(" + uuid_to_string(new_dir_inode->get_ino()) + ")");
	std::scoped_lock scl{this->directory_table_mutex};

	std::string temp_address;
	int ret = lc->acquire(new_dir_inode->get_ino(), temp_address);
	shared_ptr<dentry_table> new_dentry_table = nullptr;
	if(ret == 0) {
		global_logger.log(directory_table_ops, "Success to acquire lease");
		/* Check whether the directory needs recovery */
		//journalctl->check(ino);

		/* Success to acquire lease */
		new_dentry_table = std::make_shared<dentry_table>(new_dir_inode, new_dir_dentry, LOCAL);
		new_dentry_table->set_leader_ip(temp_address);
		this->add_dentry_table(new_dir_inode->get_ino(), new_dentry_table);
	} else if(ret == -1) {
		throw std::runtime_error("New directory should have local dentry table");
	}

	return new_dentry_table;
}

shared_ptr<dentry_table> directory_table::get_dentry_table(uuid ino, bool remote) {
	global_logger.log(directory_table_ops, "get_dentry_table(" + uuid_to_string(ino) + ")");
	std::scoped_lock scl{this->directory_table_mutex};

	auto it = this->dentry_tables.find(ino);
	if (remote) {
		if (it != this->dentry_tables.end()) { /* LOCAL, REMOTE */
			global_logger.log(directory_table_ops, "dentry_table : HIT");
			bool valid = lc->is_valid(ino);
			if (valid) {
				return it->second;
			} else {
				this->dentry_tables.erase(it);
				throw dentry_table::not_leader("Lease is expired at remote side");
			}
		} else { /* UNKNOWN */
			global_logger.log(directory_table_ops, "dentry_table : MISS");
			throw dentry_table::not_leader("This client doesn't have lease of this dentry table");
		}
	} else {
		if (it != this->dentry_tables.end()) { /* LOCAL, REMOTE */
			global_logger.log(directory_table_ops, "dentry_table : HIT");
			bool valid = lc->is_valid(ino);
			if (valid) {
				return it->second;
			} else {
				this->dentry_tables.erase(it);
				shared_ptr<dentry_table> new_dentry_table = lease_dentry_table(ino);
				return new_dentry_table;
			}
		} else { /* UNKNOWN */
			global_logger.log(directory_table_ops, "dentry_table : MISS");
			shared_ptr<dentry_table> new_dentry_table = lease_dentry_table(ino);
			return new_dentry_table;
		}
	}
}

int directory_table::add_dentry_table(uuid ino, shared_ptr<dentry_table> dtable){
	global_logger.log(directory_table_ops, "Called add_dentry_table(" + uuid_to_string(ino) + ")");
	std::scoped_lock scl{this->directory_table_mutex};

	auto ret = dentry_tables.insert(std::make_pair(ino, nullptr));
	if(ret.second) {
		ret.first.value() = dtable;
	} else {
		global_logger.log(directory_table_ops, "Already added dentry table is tried to inserted");
		return -1;
	}
	return 0;
}

int directory_table::delete_dentry_table(uuid ino){
	global_logger.log(directory_table_ops, "Called delete_dentry_table(" + uuid_to_string(ino) + ")");
	std::scoped_lock scl{this->directory_table_mutex};

	auto it = this->dentry_tables.find(ino);
	if(it == this->dentry_tables.end()) {
		global_logger.log(directory_table_ops, "Non-existed dentry table is tried to delete");
		return -1;
	}

	this->dentry_tables.erase(it);

	return 0;
}

void directory_table::find_remote_dentry_table_again(const std::shared_ptr<remote_inode>& remote_i) {
	global_logger.log(directory_table_ops, "Called find_remote_dentry_table_again()");

	shared_ptr<dentry_table> target_dentry_table = this->get_dentry_table(remote_i->get_dentry_table_ino());
	if(target_dentry_table->get_loc() == REMOTE) {
		global_logger.log(directory_table_ops, "Remote dentry table moves to other leader");
	} else if (target_dentry_table->get_loc() == LOCAL) {
		global_logger.log(directory_table_ops, "Remote dentry table becomes Local dentry table");
	}

	remote_i->set_leader_ip(target_dentry_table->get_leader_ip());
}

