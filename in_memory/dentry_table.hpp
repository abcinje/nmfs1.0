#ifndef NMFS0_DENTRY_TABLE_HPP
#define NMFS0_DENTRY_TABLE_HPP

#include <map>
#include <utility>
#include <memory>
#include "../meta/inode.hpp"
#include "../meta/dentry.hpp"

enum meta_location {
    LOCAL = 0,
    REMOTE,
    UNKNOWN,
    NOBODY /* temporaly status */
};

using std::shared_ptr;

class dentry_table {
private:
	ino_t dir_ino;
	shared_ptr<inode> dir_inode;

	shared_ptr<dentry> dentries;
	std::map<std::string, shared_ptr<inode>> child_inodes;

	enum meta_location loc;
	uint64_t leader_id;

public:
	explicit dentry_table(ino_t dir_ino, shared_ptr<inode> dir_inode);
	~dentry_table();

 	int add_inode(std::string filename, shared_ptr<inode> inode);
	int delete_inode(std::string filename);
	shared_ptr<inode> get_child_inode(std::string filename);
	int pull_dir_inode_itself();
	int pull_child_metadata();

	shared_ptr<inode> get_dir_inode();
	enum meta_location get_loc();
	uint64_t get_leader_id();

	void set_dir_inode(shared_ptr<inode> dir_inode);
	void set_loc(enum meta_location loc);
	void set_leader_id(uint64_t leader_id);

};

#endif //NMFS0_DENTRY_TABLE_HPP
