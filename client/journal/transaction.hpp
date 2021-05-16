#ifndef _TRANSACTION_HPP_
#define _TRANSACTION_HPP_

#include <memory>
#include <mutex>
#include <tsl/robin_map.h>

#include "../meta/inode.hpp"

class transaction {
private:
	std::mutex m;
	bool committed;
	std::unique_ptr<inode> d_inode;

	/* The boolean value is true if the entry has been added and false if the entry has been deleted. */
	tsl::robin_map<std::string, bool> dentries;

	/* An unique pointer whose value is null means the file has been deleted. */
	tsl::robin_map<ino_t, std::unique_ptr<inode>> f_inodes;

public:
	transaction(void);
	~transaction(void) = default;

	int set_inode(std::shared_ptr<inode> i);
	int mkdir(const std::string &d_name, const struct timespec &time);
	int rmdir(const std::string &d_name, const struct timespec &time);
	int mkreg(const std::string &f_name, std::shared_ptr<inode> i);
	int rmreg(const std::string &f_name, std::shared_ptr<inode> i, const struct timespec &time);
};

#endif /* _TRANSACTION_HPP_ */
