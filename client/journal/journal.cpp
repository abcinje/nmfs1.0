#include "journal.hpp"

journal::journal(rados_io *meta_pool, std::shared_ptr<lease_client> lease) : meta(meta_pool), lc(lease), stopped(false)
{
	commit_thr = std::make_unique<std::thread>(commit(&stopped, meta, &jtable, &q));
	checkpoint_thr = std::make_unique<std::thread>(checkpoint(meta, &q));
}

journal::~journal(void)
{
	stopped = true;
	commit_thr->join();
	checkpoint_thr->join();
}

void journal::chself(const uuid &self_ino, std::shared_ptr<inode> self_inode)
{
	while (true) {
		auto tx = jtable.get_entry(self_ino);
		if (!tx->chself(self_inode))
			break;
	}
}

void journal::chreg(const uuid &self_ino, std::shared_ptr<inode> f_inode)
{
	while (true) {
		auto tx = jtable.get_entry(self_ino);
		if (!tx->chreg(f_inode))
			break;
	}
}

void journal::mkdir(const uuid &self_ino, const std::string &d_name, const uuid &d_ino, const struct timespec &time)
{
	while (true) {
		auto tx = jtable.get_entry(self_ino);
		if (!tx->mkdir(d_name, d_ino, time))
			break;
	}
}

void journal::rmdir(const uuid &self_ino, const std::string &d_name, const uuid &d_ino, const struct timespec &time)
{
	while (true) {
		auto tx = jtable.get_entry(self_ino);
		if (!tx->rmdir(d_name, d_ino, time))
			break;
	}
}

void journal::mkreg(const uuid &self_ino, const std::string &f_name, std::shared_ptr<inode> f_inode, const struct timespec &time)
{
	while (true) {
		auto tx = jtable.get_entry(self_ino);
		if (!tx->mkreg(f_name, f_inode, time))
			break;
	}
}

void journal::rmreg(const uuid &self_ino, const std::string &f_name, std::shared_ptr<inode> f_inode, const struct timespec &time)
{
	while (true) {
		auto tx = jtable.get_entry(self_ino);
		if (!tx->rmreg(f_name, f_inode, time))
			break;
	}
}
