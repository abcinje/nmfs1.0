#include "transaction.hpp"

transaction::transaction(void) : d_inode(nullptr)
{
}

void transaction::set_inode(std::shared_ptr<inode> i)
{
	std::unique_lock lock(m);

	d_inode = std::make_unique<inode>(*i);
}

void transaction::mkdir(const std::string &d_name, const struct timespec &time)
{
	std::unique_lock lock(m);

	auto dentries_ret = dentries.insert({d_name, true});
	if (!dentries_ret.second) {
		if (!dentries_ret.first->second) {
			dentries_ret.first.value() = true;
		} else {
			throw std::logic_error("transaction::mkdir() failed (directory already exists)");
		}
	}

	d_inode->set_mtime(time);
	d_inode->set_ctime(time);
}

void transaction::rmdir(const std::string &d_name, const struct timespec &time)
{
	std::unique_lock lock(m);

	auto dentries_ret = dentries.insert({d_name, false});
	if (!dentries_ret.second) {
		if (dentries_ret.first->second) {
			dentries_ret.first.value() = false;
		} else {
			throw std::logic_error("transaction::rmdir() failed (directory doesn't exist)");
		}
	}

	d_inode->set_mtime(time);
	d_inode->set_ctime(time);
}

void transaction::mkreg(const std::string &f_name, std::shared_ptr<inode> i)
{
	std::unique_lock lock(m);

	auto dentries_ret = dentries.insert({f_name, true});
	if (!dentries_ret.second) {
		if (!dentries_ret.first->second) {
			dentries_ret.first.value() = true;
		} else {
			throw std::logic_error("transaction::mkreg() failed (file already exists)");
		}
	}

	d_inode->set_mtime(i->get_mtime());
	d_inode->set_ctime(i->get_ctime());

	auto f_inodes_ret = f_inodes.insert({i->get_ino(), nullptr});
	if (f_inodes_ret.second || !f_inodes_ret.first->second) {
		f_inodes_ret.first.value() = std::make_unique<inode>(*i);
	} else {
		throw std::logic_error("transaction::mkreg() failed (file already exists)");
	}
}

void transaction::rmreg(const std::string &f_name, std::shared_ptr<inode> i, const struct timespec &time)
{
	std::unique_lock lock(m);

	auto dentries_ret = dentries.insert({f_name, false});
	if (!dentries_ret.second) {
		if (dentries_ret.first->second) {
			dentries_ret.first.value() = false;
		} else {
			throw std::logic_error("transaction::rmreg() failed (file doesn't exist)");
		}
	}

	d_inode->set_mtime(time);
	d_inode->set_ctime(time);

	auto f_inodes_ret = f_inodes.insert({i->get_ino(), nullptr});
	if (!f_inodes_ret.second && !f_inodes_ret.first->second)
		throw std::logic_error("transaction::rmreg() failed (file doesn't exist)");
}
