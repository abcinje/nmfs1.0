#ifndef _JOURNAL_HPP_
#define _JOURNAL_HPP_

#include <thread>

#include "checkpoint.hpp"
#include "commit.hpp"
#include "journal_table.hpp"
#include "mqueue.hpp"
#include "../lease/lease_client.hpp"

class journal {
private:
	rados_io *meta;
	std::shared_ptr<lease_client> lc;

	bool stopped;
	journal_table jtable;
	mqueue<std::shared_ptr<transaction>> q;
	std::unique_ptr<std::thread> commit_thr, checkpoint_thr;

public:
	journal(rados_io *meta_pool, std::shared_ptr<lease_client> lease);
	~journal(void);

	void chself(const uuid &self_ino, std::shared_ptr<inode> self_inode);
	void chreg(const uuid &self_ino, std::shared_ptr<inode> f_inode);
	void mkdir(const uuid &self_ino, const std::string &d_name, const uuid &d_ino, const struct timespec &time);
	void rmdir(const uuid &self_ino, const std::string &d_name, const uuid &d_ino, const struct timespec &time);
	void mkreg(const uuid &self_ino, const std::string &f_name, std::shared_ptr<inode> f_inode, const struct timespec &time);
	void rmreg(const uuid &self_ino, const std::string &f_name, std::shared_ptr<inode> f_inode, const struct timespec &time);
};

#endif /* _JOURNAL_HPP_ */
