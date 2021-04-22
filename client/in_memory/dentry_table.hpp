#ifndef NMFS0_DENTRY_TABLE_HPP
#define NMFS0_DENTRY_TABLE_HPP

#include <map>
#include <utility>
#include <memory>
#include "../meta/inode.hpp"
#include "../meta/dentry.hpp"

using std::shared_ptr;

class dentry_table {
private:
	ino_t dir_ino;

	shared_ptr<dentry> dentries;
	std::map<std::string, shared_ptr<inode>> child_inodes;

	enum meta_location loc;
	uint64_t leader_id;

	std::recursive_mutex dentry_table_mutex;
public:
	explicit dentry_table(ino_t dir_ino);
	~dentry_table();

	int create_child_inode(std::string filename, shared_ptr<inode> inode);
	/* Just used in pull_child_metadata */
 	int add_child_inode(std::string filename, shared_ptr<inode> inode);
	int delete_child_inode(std::string filename);

	shared_ptr<inode> get_child_inode(std::string filename);
	ino_t check_child_inode(std::string filename);
	int pull_child_metadata();

	enum meta_location get_loc();
	uint64_t get_leader_id();

	void set_loc(enum meta_location loc);
	void set_leader_id(uint64_t leader_id);
	void set_dentries(shared_ptr<dentry> dentries);

	/* wrapper of dentry class member functions */
	void fill_filler(void *buffer, fuse_fill_dir_t filler);
	uint64_t get_child_num();
	uint64_t get_total_name_legth();
};

#endif //NMFS0_DENTRY_TABLE_HPP
