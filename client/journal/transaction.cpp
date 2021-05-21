#include "transaction.hpp"

transaction::transaction(void) : committed(false), d_inode(nullptr)
{
}

std::vector<char> transaction::serialize(void)
{
	std::unique_lock lock(m);

	if (committed)
		return std::vector<char>();
	committed = true;

	std::vector<char> vec;

	/* valid bit */
	vec.push_back(1);

	/* d_inode */
	auto d_inode_vec = d_inode->serialize();
	uint32_t d_inode_size = static_cast<uint32_t>(d_inode_vec.size());
	vec.push_back(static_cast<char>(d_inode_size & 0xff));
	vec.push_back(static_cast<char>((d_inode_size >> 8) & 0xff));
	vec.push_back(static_cast<char>((d_inode_size >> 16) & 0xff));
	vec.push_back(static_cast<char>((d_inode_size >> 24) & 0xff));
	vec.insert(vec.end(), d_inode_vec.begin(), d_inode_vec.end());

	/* dentries */
	for (auto &d : dentries) {
		vec.push_back(d.second ? 1 : 0);
		uint32_t dentry_size = static_cast<uint32_t>(d.first.size());
		vec.push_back(static_cast<char>(dentry_size & 0xff));
		vec.push_back(static_cast<char>((dentry_size >> 8) & 0xff));
		vec.push_back(static_cast<char>((dentry_size >> 16) & 0xff));
		vec.push_back(static_cast<char>((dentry_size >> 24) & 0xff));
		vec.insert(vec.end(), d.first.begin(), d.first.end());
	}
	vec.push_back(-1);

	/* f_inodes */
	for (auto &i : f_inodes) {
		auto f_inode_vec = i.second->serialize();
		uint32_t f_inode_size = static_cast<uint32_t>(f_inode_vec.size());
		vec.push_back(static_cast<char>(f_inode_size & 0xff));
		vec.push_back(static_cast<char>((f_inode_size >> 8) & 0xff));
		vec.push_back(static_cast<char>((f_inode_size >> 16) & 0xff));
		vec.push_back(static_cast<char>((f_inode_size >> 24) & 0xff));
		vec.insert(vec.end(), f_inode_vec.begin(), f_inode_vec.end());
	}
	for (int i = 0; i < 4; i++)
		vec.push_back(-1);

	return vec;
}

int transaction::set_inode(std::shared_ptr<inode> i)
{
	std::unique_lock lock(m);

	if (committed)
		return -1;

	d_inode = std::make_unique<inode>(*i);

	return 0;
}

int transaction::mkdir(const std::string &d_name, const struct timespec &time)
{
	std::unique_lock lock(m);

	if (committed)
		return -1;

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

	return 0;
}

int transaction::rmdir(const std::string &d_name, const struct timespec &time)
{
	std::unique_lock lock(m);

	if (committed)
		return -1;

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

	return 0;
}

int transaction::mkreg(const std::string &f_name, std::shared_ptr<inode> i)
{
	std::unique_lock lock(m);

	if (committed)
		return -1;

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

	auto f_inodes_ret = f_inodes.insert({uuid_to_string(i->get_ino()), nullptr});
	if (f_inodes_ret.second || !f_inodes_ret.first->second) {
		f_inodes_ret.first.value() = std::make_unique<inode>(*i);
	} else {
		throw std::logic_error("transaction::mkreg() failed (file already exists)");
	}

	return 0;
}

int transaction::rmreg(const std::string &f_name, std::shared_ptr<inode> i, const struct timespec &time)
{
	std::unique_lock lock(m);

	if (committed)
		return -1;

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

	auto f_inodes_ret = f_inodes.insert({uuid_to_string(i->get_ino()), nullptr});
	if (!f_inodes_ret.second && !f_inodes_ret.first->second)
		throw std::logic_error("transaction::rmreg() failed (file doesn't exist)");

	return 0;
}
