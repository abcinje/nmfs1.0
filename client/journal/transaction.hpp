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
public:
	enum class self_status {
		S_UNCHANGED,
		S_CREATED,
		S_DELETED,
	};

private:
	std::mutex m;

	/* Has this transaction already been committed? */
	std::atomic<bool> committed;

	/* the offset for this transaction in the journal object */
	off_t offset;

	/* the status of the directory itself
	 *   mkself -> S_CREATED
	 *   rmself -> S_DELETED
	 *   o.w.   -> S_UNCHANGED
	 */
	self_status status;

	/* the inode number of the directory itself
	 * This field should be always valid unlike s_inode pointer */
	uuid s_ino;

	/* the inode block of the directory itself */
	std::unique_ptr<inode> s_inode;

	/* The integer value is the count diff for directory entries since this transaction has been created. */
	tsl::robin_map<std::string, std::pair<int, boost::uuids::uuid>> dentries;

	/* An unique pointer whose value is null means the file has been deleted. */
	tsl::robin_map<std::string, std::unique_ptr<inode>> f_inodes;

public:
	transaction(const uuid &self_ino);
	~transaction(void) = default;

	/* self */
	int mkself(std::shared_ptr<inode> self_inode);
	int rmself(const uuid &self_ino);
	int chself(std::shared_ptr<inode> self_inode);

	/* directories */
	int mkdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino);
	int rmdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino);

	/* regular files */
	int mkreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode);
	int rmreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode);
	int chreg(std::shared_ptr<inode> f_inode);

	std::vector<char> serialize(void);
	int deserialize(std::vector<char> raw);

	void sync(std::shared_ptr<rados_io> meta);

	void commit(std::shared_ptr<rados_io> meta);
	void checkpoint(std::shared_ptr<rados_io> meta);
};

#endif /* _TRANSACTION_HPP_ */
