#include "dentry_table.hpp"

dentry_table::dentry_table(ino_t dir_ino, shared_ptr<inode> dir_inode) : dir_ino(dir_ino), dir_inode(dir_inode){
	/*
	 * if LOCAL : pull_child_metadata() right after return to caller if dentry object exist
	 * if REMOTE : don't call pull_child_metadata(), just add REMOTE info
	 */
}

dentry_table::~dentry_table() {
	global_logger.log(directory_table_ops, "Called ~directory_table(" + std::to_string(this->dir_ino)+")");
	this->child_inodes.clear();

	fuse_context *fuse_ctx = fuse_get_context();
	client *myself = (client *)(fuse_ctx->private_data);
	uint64_t client_id = myself->get_client_id();

	if(this->dir_ino == client_id) {
		this->dir_inode->set_leader_id(0);
		this->dir_inode->sync();
	}
}

int dentry_table::add_child_inode(std::string filename, shared_ptr<inode> inode){
	global_logger.log(dentry_table_ops, "Called add_child_ino(" + filename + ")");
	auto ret = this->child_inodes.insert(std::make_pair(filename, nullptr));
	if(ret.second) {
		ret.first->second = inode;

		this->dentries->add_new_child(filename, inode->get_ino());
		this->dentries->sync();
	} else {
		global_logger.log(dentry_table_ops, "Already added file is tried to inserted");
		return -1;
	}
	return 0;
}

int dentry_table::delete_child_inode(std::string filename) {
	global_logger.log(dentry_table_ops, "Called delete_child_inode(" + filename + ")");
	std::map<std::string, shared_ptr<inode>>::iterator it;
	it = this->child_inodes.find(filename);

	if(it == this->child_inodes.end()) {
		global_logger.log(dentry_table_ops, "Non-existing file is tried to deleted");
		return -1;
	}

	this->child_inodes.erase(it);

	this->dentries->delete_child(filename);
	this->dentries->sync();
	return 0;
}

shared_ptr<inode> dentry_table::get_child_inode(std::string filename){
	global_logger.log(dentry_table_ops, "Called get_child_inode(" + filename + ")");
	if(this->loc == LOCAL) {
		std::map<std::string, shared_ptr<inode>>::iterator it;
		it = this->child_inodes.find(filename);

		if(it == this->child_inodes.end()) {
			throw inode::no_entry("No such file or directory : get_child_inode");
		}

		return it->second;
	} else if (this->loc == REMOTE) {
		/* TODO */
	}

	return nullptr;
}

int dentry_table::pull_child_metadata() {
	global_logger.log(dentry_table_ops, "Called pull_child_metadata()");
	this->dentries = std::make_shared<dentry>(this->dir_ino);

	std::map<std::string, ino_t>::iterator it;
	for(it = this->dentries->child_list.begin(); it != this->dentries->child_list.end(); it++) {
		this->add_child_inode(it->first, std::make_shared<inode>(it->second));
	}

	return 0;
}


shared_ptr<inode> dentry_table::get_dir_inode(){
	global_logger.log(dentry_table_ops, "Called get_dir_inode()");
	if(this->loc == LOCAL) {
		return this->dir_inode;
	} else if (this->loc == REMOTE) {
		/* TODO */
	}
	return nullptr;
}

enum meta_location dentry_table::get_loc() {return this->loc;}
uint64_t dentry_table::get_leader_id() {return this->leader_id;}

void dentry_table::set_dir_inode(shared_ptr<inode> dir_inode) {this->dir_inode = dir_inode;}
void dentry_table::set_loc(enum meta_location loc) {this->loc = loc;}
void dentry_table::set_leader_id(uint64_t leader_id) {this->leader_id = leader_id;}
void dentry_table::set_dentries(shared_ptr<dentry> dentries) {this->dentries = dentries;}


void dentry_table::fill_filler(void *buffer, fuse_fill_dir_t filler) {
	this->dentries->fill_filler(buffer, filler);
}
uint64_t dentry_table::get_child_num() {
	return this->dentries->get_child_num();
}
uint64_t dentry_table::get_total_name_legth() {
	return this->dentries->get_total_name_legth();
}




