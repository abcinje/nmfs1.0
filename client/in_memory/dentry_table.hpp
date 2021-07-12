#ifndef NMFS0_DENTRY_TABLE_HPP
#define NMFS0_DENTRY_TABLE_HPP

#include <map>
#include <utility>
#include <memory>
#include "../meta/inode.hpp"
#include "../meta/dentry.hpp"
#include "../rpc/rpc_client.hpp"

using std::shared_ptr;

enum dentry_table_status{
    VALID = 0,
    INVALID /* expired or removed */
};

class dentry_table {
private:
    	enum dentry_table_status status;
	uuid dir_ino;
	shared_ptr<inode> this_dir_inode;

	shared_ptr<dentry> dentries;
	std::map<std::string, shared_ptr<inode>> child_inodes;

	enum meta_location loc;
	std::string leader_ip;

public:
	std::shared_mutex dentry_table_mutex;

	class not_leader : public runtime_error {
    	public:
	    	explicit not_leader(const string &msg);
	    	const char *what(void);
	};

	explicit dentry_table(uuid dir_ino, enum meta_location loc);
    	explicit dentry_table(std::shared_ptr<inode> new_dir_inode, std::shared_ptr<dentry> new_dir_dentry, enum meta_location loc);
	~dentry_table();

	int create_child_inode(std::string filename, shared_ptr<inode> inode);
	/* Just used in pull_child_metadata */
 	int add_child_inode(std::string filename, shared_ptr<inode> inode);
	int delete_child_inode(std::string filename);

	shared_ptr<inode> get_child_inode(std::string filename, uuid target_ino = nil_uuid());
	uuid check_child_inode(std::string filename);
	int pull_child_metadata(const std::shared_ptr<dentry_table>& this_dentry_table);

	enum meta_location get_loc();

	uuid get_dir_ino();
	shared_ptr<inode> get_this_dir_inode();
	const string &get_leader_ip();

	void set_leader_ip(std::string new_leader_ip);
	void set_status(enum dentry_table_status new_status);
	/* wrapper of dentry class member functions */
	void fill_filler(void *buffer, fuse_fill_dir_t filler);
	uint64_t get_child_num();

	/* only used in rpc_readdir */
	std::map<std::string, shared_ptr<inode>>::iterator get_child_inode_begin();
	std::map<std::string, shared_ptr<inode>>::iterator get_child_inode_end();

	bool is_valid();

};

#endif //NMFS0_DENTRY_TABLE_HPP
