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
	std::shared_ptr<rados_io> meta;
	std::shared_ptr<lease_client> lc;

	bool stopped;
	journal_table jtable;
	mqueue<std::shared_ptr<transaction>> q;
	std::unique_ptr<std::thread> commit_thr, checkpoint_thr;

public:
	journal(std::shared_ptr<rados_io> meta_pool, std::shared_ptr<lease_client> lease);
	~journal(void);

	void check(const uuid &self_ino);

	void chself(std::shared_ptr<inode> self_inode);
	void chreg(const uuid &self_ino, std::shared_ptr<inode> f_inode);
	void mkdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino);
	void rmdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino);
	void mkreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode);
	void rmreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode);
};

#endif /* _JOURNAL_HPP_ */
