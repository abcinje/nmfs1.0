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
#include "../temp/lease_util.hpp"

#define ENOTLEADER 8000

using std::shared_ptr;

class directory_table {
private:
    	shared_ptr<inode> root_inode;
	std::map<ino_t, shared_ptr<dentry_table>> dentry_tables;

	std::recursive_mutex directory_table_mutex;
public:
    	directory_table();
	~directory_table();
    	int add_dentry_table(ino_t ino, shared_ptr<dentry_table> dtable);
    	int delete_dentry_table(ino_t ino);

    	shared_ptr<inode> path_traversal(std::string path);
    	shared_ptr<dentry_table> get_dentry_table(ino_t ino);
		uint64_t check_dentry_table(ino_t ino);

    	shared_ptr<inode> get_root_inode();
};


#endif //NMFS0_DIRECTORY_TABLE_HPP
