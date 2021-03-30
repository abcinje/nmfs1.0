#include "dentry_table.hpp"

dentry_table::dentry_table(ino_t dir_ino) : dir_ino(dir_ino){
	pull_child_metadata(dir_ino);
}

dentry_table::~dentry_table() {
	this->child_inodes.clear();
}


shared_ptr<inode> dentry_table::get_dir_inode(){
	if(this->loc == LOCAL) {
		return this->dir_inode;
	} else if (this->loc == REMOTE) {
		/* TODO */
	}
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

int dentry_table::add_inode(std::string filename, shared_ptr<inode> inode){

}

int pull_child_metadata(ino_t dir_ino){

}