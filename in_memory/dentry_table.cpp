#include "dentry_table.hpp"

dentry_table::dentry_table(ino_t dir_ino, shared_ptr<inode> dir_inode) : dir_ino(dir_ino), dir_inode(dir_inode){
	/*
	 * if LOCAL : pull_child_metadata() right after return to caller
	 * if REMOTE : don't call pull_child_metadata(), just add REMOTE info
	 */
}

dentry_table::~dentry_table() {
	this->child_inodes.clear();
}

int dentry_table::add_inode(std::string filename, shared_ptr<inode> inode){
	auto ret = this->child_inodes.insert(std::make_pair(filename, nullptr));
	if(ret.second) {
		ret.first->second = inode;
	} else {
		global_logger.log(indexing_ops, "Already added file is tried to inserted");
		return -1;
	}
	return 0;
}

int dentry_table::delete_inode(std::string filename) {
	std::map<std::string, shared_ptr<inode>>::iterator it;
	it = this->child_inodes.find(filename);

	if(it == this->child_inodes.end()) {
		global_logger.log(indexing_ops, "Non-existing file is tried to deleted");
		return -1;
	}

	this->child_inodes.erase(it);
	return 0;
}

shared_ptr<inode> dentry_table::get_child_inode(std::string filename){
	if(this->loc == LOCAL) {
		std::map<std::string, shared_ptr<inode>>::iterator it;
		it = this->child_inodes.find(filename);

		if(it == this->child_inodes.end())
			return nullptr;

		this->child_inodes.erase(it);

		return 0;
	} else if (this->loc == REMOTE) {
		/* TODO */
	}

}

int dentry_table::pull_child_metadata() {
	this->dentries = std::make_shared<dentry>(this->dir_ino);

	std::map<std::string, ino_t>::iterator it;
	for(it = this->dentries->child_list.begin(); it != this->dentries->child_list.end(); it++) {
		this->add_inode(it->first, std::make_shared<inode>(it->second));
	}

	return 0;
}


shared_ptr<inode> dentry_table::get_dir_inode(){
	if(this->loc == LOCAL) {
		return this->dir_inode;
	} else if (this->loc == REMOTE) {
		/* TODO */
	}
}

enum meta_location dentry_table::get_loc() {return this->loc;}
uint64_t dentry_table::get_leader_id() {return this->leader_id;}

void dentry_table::set_dir_inode(shared_ptr<inode> dir_inode) {this->dir_inode = dir_inode;}
void dentry_table::set_loc(enum meta_location loc) {this->loc = loc;}
void dentry_table::set_leader_id(uint64_t leader_id) {this->leader_id = leader_id;}



