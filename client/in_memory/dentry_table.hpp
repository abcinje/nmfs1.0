#ifndef NMFS0_DENTRY_TABLE_HPP
#define NMFS0_DENTRY_TABLE_HPP

#include <map>
#include <utility>
#include <memory>
#include "../meta/inode.hpp"
#include "../meta/dentry.hpp"
#include "../rpc/rpc_client.hpp"

using std::shared_ptr;

class dentry_table {
private:
	ino_t dir_ino;
	shared_ptr<inode> this_dir_inode;

	shared_ptr<dentry> dentries;
	std::map<std::string, shared_ptr<inode>> child_inodes;

	enum meta_location loc;
	std::string leader_ip;

	std::recursive_mutex dentry_table_mutex;
public:
	explicit dentry_table(ino_t dir_ino, enum meta_location loc);
	~dentry_table();

	int create_child_inode(std::string filename, shared_ptr<inode> inode);
	/* Just used in pull_child_metadata */
 	int add_child_inode(std::string filename, shared_ptr<inode> inode);
	int delete_child_inode(std::string filename);

	shared_ptr<inode> get_child_inode(std::string filename, ino_t for_get_dtable = 0);
	ino_t check_child_inode(std::string filename);
	int pull_child_metadata();

	enum meta_location get_loc();

	ino_t get_dir_ino();
	shared_ptr<inode> get_this_dir_inode();
	const string &get_leader_ip();

	void set_leader_ip(std::string new_leader_ip);

	/* wrapper of dentry class member functions */
	void fill_filler(void *buffer, fuse_fill_dir_t filler);
	uint64_t get_child_num();

	/* only used in rpc_readdir */
	std::recursive_mutex& get_dentry_table_mutex();
	std::map<std::string, shared_ptr<inode>>::iterator get_child_inode_begin();
	std::map<std::string, shared_ptr<inode>>::iterator get_child_inode_end();
};

#endif //NMFS0_DENTRY_TABLE_HPP
