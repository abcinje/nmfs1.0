#ifndef NMFS0_DENTRY_TABLE_HPP
#define NMFS0_DENTRY_TABLE_HPP

#include <map>
#include <utility>
#include <memory>
#include "../meta/inode.hpp"

enum meta_location {
    LOCAL = 0,
    REMOTE,
    UNKNOWN,

    /* temporaly status */
    NOBODY
};

using std::shared_ptr;

class dentry_table {
private:
	ino_t dir_ino;
	shared_ptr<inode> dir_inode;
	std::map<std::string, shared_ptr<inode>> child_inodes;

    	enum meta_location loc;
    	uint64_t leader_client_id;

public:
	dentry_table(ino_t dir_ino);
	~dentry_table();

    	int add_inode(std::string filename, shared_ptr<inode> inode);
    	int delete_inode(std::string filename);

    	shared_ptr<inode> get_dir_inode();
	shared_ptr<inode> get_child_inode(std::string filename);

};

int pull_child_metadata(ino_t dir_ino);
#endif //NMFS0_DENTRY_TABLE_HPP
