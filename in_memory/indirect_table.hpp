#ifndef NMFS0_INDIRECT_TABLE_HPP
#define NMFS0_INDIRECT_TABLE_HPP

#include <map>
#include <utility>
#include <memory>
#include "dentry_table.hpp"
#include "../meta/inode.hpp"
#include "../logger/logger.hpp"

using std::unique_ptr;

class indirect_table {
private:
	std::map<ino_t, std::unique_ptr<dentry_table>> dentry_tables;

public:
	unique_ptr<inode> get_inode(const char *path);

	int create_table(ino_t ino);
	int delete_table(ino_t ino);
};


#endif //NMFS0_INDIRECT_TABLE_HPP
