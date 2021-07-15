#ifndef NMFS0_DIRECTORY_TABLE_HPP
#define NMFS0_DIRECTORY_TABLE_HPP

#include <map>
#include <utility>
#include <memory>
#include <mutex>

#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <tsl/robin_map.h>

#include "lib/logger/logger.hpp"

#include "dentry_table.hpp"
#include "../meta/inode.hpp"
#include "../meta/dentry.hpp"
#include "../client/client.hpp"
#include "../lease/lease_client.hpp"
#include "../journal/journal.hpp"

using std::shared_ptr;
using namespace boost::uuids;

class directory_table {
private:
	tsl::robin_map<uuid, shared_ptr<dentry_table>, boost::hash<uuid>> dentry_tables;

public:
	std::recursive_mutex directory_table_mutex;

	directory_table();
	~directory_table();
	int add_dentry_table(uuid ino, shared_ptr<dentry_table> dtable);
	int delete_dentry_table(uuid ino);

	shared_ptr<inode> path_traversal(const std::string &path);
	shared_ptr<dentry_table> lease_dentry_table(uuid ino);
	shared_ptr<dentry_table> lease_dentry_table_mkdir(std::shared_ptr<inode> new_dir_inode, std::shared_ptr<dentry> new_dir_dentry);
	shared_ptr<dentry_table> get_dentry_table(uuid ino, bool remote = false);
	void find_remote_dentry_table_again(const std::shared_ptr<remote_inode>& remote_i);
};

#endif //NMFS0_DIRECTORY_TABLE_HPP
