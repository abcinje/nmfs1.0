#include "transaction.hpp"

#include "../meta/dentry.hpp"

const char *transaction::invalidated::what(void)
{
	return "Tried to make a checkpoint for an invalidated transaction.";
}

std::vector<char> transaction::serialize(void)
{
	std::vector<char> vec;

	/* valid bit */
	vec.push_back(1);

	/* s_inode */
	if (s_inode) {
		auto s_inode_vec = s_inode->serialize();
		int32_t s_inode_size = static_cast<uint32_t>(s_inode_vec.size());
		vec.push_back(static_cast<char>(s_inode_size & 0xff));
		vec.push_back(static_cast<char>((s_inode_size >> 8) & 0xff));
		vec.push_back(static_cast<char>((s_inode_size >> 16) & 0xff));
		vec.push_back(static_cast<char>((s_inode_size >> 24) & 0xff));
		vec.insert(vec.end(), s_inode_vec.begin(), s_inode_vec.end());
	} else {
		for (int i = 0; i < 4; i++)
			vec.push_back(-1);
	}

	/* dentries */
	for (auto &d : dentries) {
		/* added or deleted */
		vec.push_back(d.second.first ? 1 : 0);

		/* entry name */
		int32_t dentry_size = static_cast<uint32_t>(d.first.size());
		vec.push_back(static_cast<char>(dentry_size & 0xff));
		vec.push_back(static_cast<char>((dentry_size >> 8) & 0xff));
		vec.push_back(static_cast<char>((dentry_size >> 16) & 0xff));
		vec.push_back(static_cast<char>((dentry_size >> 24) & 0xff));
		vec.insert(vec.end(), d.first.begin(), d.first.end());

		/* ino */
		vec.insert(vec.end(), d.second.second.begin(), d.second.second.end());
	}
	vec.push_back(-1);

	/* f_inodes */
	for (auto &i : f_inodes) {
		auto f_inode_vec = i.second->serialize();
		int32_t f_inode_size = static_cast<uint32_t>(f_inode_vec.size());
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

void transaction::deserialize(std::vector<char> raw)
{
	off_t index = 0;

	/* valid bit */
	if (raw[index++] == 0)
		throw invalidated();

	/* s_inode */
	int32_t s_inode_size = *(reinterpret_cast<int32_t *>(&raw[index]));
	index += sizeof(int32_t);
	if (s_inode_size != -1) {	/* finished? */
		s_inode->deserialize(&raw[index]);
		index += sizeof(inode);
	}

	/* dentries */
	while (true) {
		char entry_stat = raw[index++];
		if (entry_stat == -1)	/* finished? */
			break;

		/* entry name */
		int32_t dentry_size = *(reinterpret_cast<int32_t *>(&raw[index]));
		index += sizeof(int32_t);
		std::string dentry(&raw[index], dentry_size);
		index += dentry_size;

		/* ino */
		boost::uuids::uuid ino;
		std::copy(raw.begin() + index, raw.begin() + index + ino.size(), ino.begin());
		index += ino.size();

		auto ret = dentries.insert({dentry, {static_cast<bool>(entry_stat), ino}});
		if (!ret.second)
			throw std::logic_error("transaction::deserialize() failed (a duplicated key exists)");
	}

	/* f_inodes */
	while (true) {
		int32_t f_inode_size = *(reinterpret_cast<int32_t *>(&raw[index]));
		index += sizeof(int32_t);
		if (f_inode_size == -1)	/* finished? */
			break;

		std::unique_ptr<inode> i = std::make_unique<inode>();
		i->deserialize(&raw[index]);
		auto ret = f_inodes.insert({uuid_to_string(i->get_ino()), std::move(i)});
		if (!ret.second)
			throw std::logic_error("transaction::deserialize() failed (a duplicated key exists)");
		index += f_inode_size;
	}
}

transaction::transaction(void) : committed(false), s_inode(nullptr)
{
}

int transaction::chself(std::shared_ptr<inode> self_inode)
{
	std::unique_lock lock(m);

	if (committed)
		return -1;

	s_inode = std::make_unique<inode>(*self_inode);

	return 0;
}

int transaction::mkdir(const std::string &d_name, const uuid &d_ino, const struct timespec &time)
{
	std::unique_lock lock(m);

	if (committed)
		return -1;

	auto dentries_ret = dentries.insert({d_name, {true, d_ino}});
	if (!dentries_ret.second) {
		if (!dentries_ret.first->second.first) {
			dentries_ret.first.value() = {true, d_ino};
		} else {
			throw std::logic_error("transaction::mkdir() failed (directory already exists)");
		}
	}

	s_inode->set_mtime(time);
	s_inode->set_ctime(time);

	return 0;
}

int transaction::rmdir(const std::string &d_name, const uuid &d_ino, const struct timespec &time)
{
	std::unique_lock lock(m);

	if (committed)
		return -1;

	auto dentries_ret = dentries.insert({d_name, {false, d_ino}});
	if (!dentries_ret.second) {
		if (dentries_ret.first->second.first) {
			dentries_ret.first.value() = {false, d_ino};
		} else {
			throw std::logic_error("transaction::rmdir() failed (directory doesn't exist)");
		}
	}

	s_inode->set_mtime(time);
	s_inode->set_ctime(time);

	return 0;
}

int transaction::mkreg(const std::string &f_name, std::shared_ptr<inode> f_inode, const struct timespec &time)
{
	std::unique_lock lock(m);

	if (committed)
		return -1;

	auto dentries_ret = dentries.insert({f_name, {true, f_inode->get_ino()}});
	if (!dentries_ret.second) {
		if (!dentries_ret.first->second.first) {
			dentries_ret.first.value() = {true, f_inode->get_ino()};
		} else {
			throw std::logic_error("transaction::mkreg() failed (file already exists)");
		}
	}

	s_inode->set_mtime(f_inode->get_mtime());
	s_inode->set_ctime(f_inode->get_ctime());

	auto f_inodes_ret = f_inodes.insert({uuid_to_string(f_inode->get_ino()), nullptr});
	if (f_inodes_ret.second || !f_inodes_ret.first->second) {
		f_inodes_ret.first.value() = std::make_unique<inode>(*f_inode);
	} else {
		throw std::logic_error("transaction::mkreg() failed (file already exists)");
	}

	return 0;
}

int transaction::rmreg(const std::string &f_name, std::shared_ptr<inode> f_inode, const struct timespec &time)
{
	std::unique_lock lock(m);

	if (committed)
		return -1;

	auto dentries_ret = dentries.insert({f_name, {false, f_inode->get_ino()}});
	if (!dentries_ret.second) {
		if (dentries_ret.first->second.first) {
			dentries_ret.first.value() = {false, f_inode->get_ino()};
		} else {
			throw std::logic_error("transaction::rmreg() failed (file doesn't exist)");
		}
	}

	s_inode->set_mtime(time);
	s_inode->set_ctime(time);

	auto f_inodes_ret = f_inodes.insert({uuid_to_string(f_inode->get_ino()), nullptr});
	if (!f_inodes_ret.second && !f_inodes_ret.first->second)
		throw std::logic_error("transaction::rmreg() failed (file doesn't exist)");

	return 0;
}

void transaction::commit(rados_io *meta)
{
	{
		std::unique_lock lock(m);

		if (committed)
			throw std::logic_error("transaction::commit() failed (tx has been already committed)");
		committed = true;
	}

	auto raw = serialize();

	size_t obj_size;
	meta->stat(obj_category::JOURNAL, uuid_to_string(s_inode->get_ino()), obj_size);
	meta->write(obj_category::JOURNAL, uuid_to_string(s_inode->get_ino()), raw.data(), raw.size(), obj_size);

	/* Update offset */
	offset = obj_size;
}

void transaction::checkpoint(rados_io *meta)
{
	/* s_inode */
	s_inode->sync();

	/* dentries */
	dentry d(s_inode->get_ino());
	for (const auto &p : dentries) {
		bool alive = p.second.first;
		std::string name = p.first;
		boost::uuids::uuid ino = p.second.second;

		if (alive) {
			d.add_new_child(name, ino);
		} else {
			d.delete_child(name);
		}
	}
	d.sync();

	/* f_inodes */
	for (const auto &p : f_inodes)
		p.second->sync();

	/* Clear the checkpoint bit */
	meta->write(obj_category::JOURNAL, uuid_to_string(s_inode->get_ino()), "\0", 1, offset);
}
