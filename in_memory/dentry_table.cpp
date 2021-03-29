#include "dentry_table.hpp"

dentry_table::dentry_table(ino_t dir_ino) : dir_ino(dir_ino){
	pull_child_metadata(dir_ino);
}

dentry_table::~dentry_table() {
	this->child_inodes.clear();
}


unique_ptr<inode> dentry_table::get_target_inode(std::string filename){
	std::map<std::string, unique_ptr<inode>>::iterator it;
	it = this->child_inodes.find(filename);

	if(it == this->child_inodes.end())
		return nullptr;

	this->child_inodes.erase(it);

	return 0;
}

int dentry_table::add_inode(std::string filename, unique_ptr<inode> inode){

}

int pull_child_metadata(ino_t dir_ino){

}