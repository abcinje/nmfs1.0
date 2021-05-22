#ifndef _TRANSACTION_HPP_
#define _TRANSACTION_HPP_

#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <tsl/robin_map.h>

#include "../meta/inode.hpp"

class transaction {
private:
	std::mutex m;

	/* Has this transaction already been committed? */
	bool committed;

	/* the offset for this transaction in the journal object */
	off_t offset;

	/* the inode block of the directory itself */
	std::unique_ptr<inode> d_inode;

	/* The boolean value is true if the entry has been added and false if the entry has been deleted. */
	tsl::robin_map<std::string, bool> dentries;

	/* An unique pointer whose value is null means the file has been deleted. */
	tsl::robin_map<std::string, std::unique_ptr<inode>> f_inodes;

public:
	class invalidated : public std::exception {
	public:
		const char *what(void);
	};

	std::vector<char> serialize(void);
	void deserialize(std::vector<char> raw);

	int set_inode(std::shared_ptr<inode> i);
	int mkdir(const std::string &d_name, const struct timespec &time);
	int rmdir(const std::string &d_name, const struct timespec &time);
	int mkreg(const std::string &f_name, std::shared_ptr<inode> i);
	int rmreg(const std::string &f_name, std::shared_ptr<inode> i, const struct timespec &time);

	transaction(void);
	~transaction(void) = default;

	off_t get_offset(void);
	void set_offset(off_t off);
};

#endif /* _TRANSACTION_HPP_ */
