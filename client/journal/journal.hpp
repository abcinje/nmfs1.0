#ifndef _JOURNAL_HPP_
#define _JOURNAL_HPP_

#include <thread>

#include "checkpoint.hpp"
#include "commit.hpp"
#include "journal_table.hpp"
#include "mqueue.hpp"
#include "../lease/lease_client.hpp"

#define NUM_CP_THREAD 8

class journal {
private:
	std::shared_ptr<rados_io> meta;
	std::shared_ptr<lease_client> lc;

	bool stopped;
	journal_table jtable;
	mqueue<std::shared_ptr<transaction>> q[NUM_CP_THREAD];
	std::unique_ptr<std::thread> commit_thr, checkpoint_thr[NUM_CP_THREAD];

public:
	journal(std::shared_ptr<rados_io> meta_pool, std::shared_ptr<lease_client> lease);
	~journal(void);

	void check(const uuid &self_ino);

	/* self */
	void mkself(std::shared_ptr<inode> self_inode);
	void rmself(const uuid &self_ino);
	void chself(std::shared_ptr<inode> self_inode);

	/* directories */
	void mkdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino);
	void rmdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino);
	void mvdir(std::shared_ptr<inode> self_inode, const std::string &src_d_name, const uuid &src_d_ino, const std::string &dst_d_name, const uuid &dst_d_ino = nil_uuid());

	/* regular files */
	void mkreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode);
	void rmreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode);
	void mvreg(std::shared_ptr<inode> self_inode, const std::string &src_f_name, const uuid &src_f_ino, const std::string &dst_f_name, const uuid &dst_f_ino = nil_uuid());
	void chreg(const uuid &self_ino, std::shared_ptr<inode> f_inode);
};

#endif /* _JOURNAL_HPP_ */
