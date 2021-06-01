#ifndef _TRANSACTION_HPP_
#define _TRANSACTION_HPP_

#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>
#include <boost/uuid/uuid.hpp>
#include <tsl/robin_map.h>

#include "../../lib/rados_io/rados_io.hpp"
#include "../meta/inode.hpp"

class transaction {
private:
	std::mutex m;

	/* Has this transaction already been committed? */
	std::atomic<bool> committed;

	/* the offset for this transaction in the journal object */
	off_t offset;

	/* the inode block of the directory itself */
	std::unique_ptr<inode> s_inode;

	/* The boolean value is true if the entry has been added and false if the entry has been deleted. */
	tsl::robin_map<std::string, std::pair<bool, boost::uuids::uuid>> dentries;

	/* An unique pointer whose value is null means the file has been deleted. */
	tsl::robin_map<std::string, std::unique_ptr<inode>> f_inodes;

public:
	int chself(std::shared_ptr<inode> self_inode);
	int chreg(std::shared_ptr<inode> self_inode, std::shared_ptr<inode> f_inode);
	int mkdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino);
	int rmdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino);
	int mkreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode);
	int rmreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode);

	transaction(void);
	~transaction(void) = default;

	std::vector<char> serialize(void);
	int deserialize(std::vector<char> raw);

	void sync(void);

	void commit(std::shared_ptr<rados_io> meta);
	void checkpoint(std::shared_ptr<rados_io> meta);
};

#endif /* _TRANSACTION_HPP_ */
