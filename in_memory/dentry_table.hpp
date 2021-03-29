#ifndef NMFS0_DENTRY_TABLE_HPP
#define NMFS0_DENTRY_TABLE_HPP

#include <map>
#include <utility>
#include <memory>
#include "../meta/inode.hpp"

using std::unique_ptr;

class dentry_table {
private:
	ino_t dir_ino;
	unique_ptr<inode> dir_inode;
	std::map<std::string, unique_ptr<inode>> child_inodes;

public:
	dentry_table(ino_t dir_ino);
	~dentry_table();

	unique_ptr<inode> get_target_inode(std::string filename);
	int add_inode(std::string filename, unique_ptr<inode> inode);
};

int pull_child_metadata(ino_t dir_ino);
#endif //NMFS0_DENTRY_TABLE_HPP
