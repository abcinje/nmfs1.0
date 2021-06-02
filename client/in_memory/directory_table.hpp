#ifndef NMFS0_DIRECTORY_TABLE_HPP
#define NMFS0_DIRECTORY_TABLE_HPP

#include <map>
#include <utility>
#include <memory>
#include <mutex>

#include "dentry_table.hpp"
#include "../meta/inode.hpp"
#include "../meta/dentry.hpp"
#include "../logger/logger.hpp"
#include "../client/client.hpp"
#include "../lease/lease_client.hpp"
#include "../journal/journal.hpp"

using std::shared_ptr;

class directory_table {
private:
	std::map<uuid, shared_ptr<dentry_table>> dentry_tables;

public:
	std::recursive_mutex directory_table_mutex;

	directory_table();
	~directory_table();
	int add_dentry_table(uuid ino, shared_ptr<dentry_table> dtable);
	int delete_dentry_table(uuid ino);

	shared_ptr<inode> path_traversal(const std::string &path);
    	shared_ptr<dentry_table> lease_dentry_table(uuid ino);
    	shared_ptr<dentry_table> lease_dentry_table_mkdir(std::shared_ptr<inode> new_dir_inode);
	shared_ptr<dentry_table> get_dentry_table(uuid ino, bool remote = false);
	void find_remote_dentry_table_again(const std::shared_ptr<remote_inode>& remote_i);
};

#endif //NMFS0_DIRECTORY_TABLE_HPP

