#include "transaction.hpp"

#include "../meta/dentry.hpp"

std::vector<char> transaction::serialize(void)
{
	global_logger.log(transaction_ops, "Called serialize()");

	std::vector<char> vec;

	/* total vector size */
	for (int i = 0; i < 4; i++)
		vec.push_back(0);

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
	for (auto &i : f_inodes)
		if (i.second) {
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

	int vec_size = static_cast<int>(vec.size());
	vec[0] = static_cast<char>(vec_size & 0xff);
	vec[1] = static_cast<char>((vec_size >> 8) & 0xff);
	vec[2] = static_cast<char>((vec_size >> 16) & 0xff);
	vec[3] = static_cast<char>((vec_size >> 24) & 0xff);

	return vec;
}

int transaction::deserialize(std::vector<char> raw)
{
	global_logger.log(transaction_ops, "Called deserialize()");

	/* Consider vector size */
	off_t index = 4;

	/* valid bit */
	if (raw[index++] == 0)
		return -1;

	/* s_inode */
	int32_t s_inode_size = *(reinterpret_cast<int32_t *>(&raw[index]));
	index += sizeof(int32_t);
	if (s_inode_size != -1) {	/* finished? */
		s_inode = std::make_unique<inode>(JOURNAL);
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

		auto i = std::make_unique<inode>(JOURNAL);
		i->deserialize(&raw[index]);
		auto ret = f_inodes.insert({uuid_to_string(i->get_ino()), std::move(i)});
		if (!ret.second)
			throw std::logic_error("transaction::deserialize() failed (a duplicated key exists)");
		index += f_inode_size;
	}

	return 0;
}

transaction::transaction(const uuid &self_ino) : committed(false), status(self_status::S_UNCHANGED), s_ino(self_ino), s_inode(nullptr)
{
}

int transaction::mkself(std::shared_ptr<inode> self_inode)
{
	global_logger.log(transaction_ops, "Called transaction::mkself(" + uuid_to_string(self_inode->get_ino()) + ")");

	std::unique_lock lock(m);

	if (committed)
		return -1;

	s_inode = std::make_unique<inode>(*self_inode);
	status = self_status::S_CREATED;

	return 0;
}

int transaction::rmself(const uuid &self_ino)
{
	global_logger.log(transaction_ops, "Called transaction::rmself(" + uuid_to_string(self_ino) + ")");

	std::unique_lock lock(m);

	if (committed)
		return -1;

	status = self_status::S_DELETED;

	return 0;
}

int transaction::chself(std::shared_ptr<inode> self_inode)
{
	global_logger.log(transaction_ops, "Called transaction::chself(" + uuid_to_string(self_inode->get_ino()) + ")");

	std::unique_lock lock(m);

	if (committed)
		return -1;

	s_inode = std::make_unique<inode>(*self_inode);

	return 0;
}

int transaction::mkdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino)
{
	global_logger.log(transaction_ops, "Called transaction::mkdir(" + uuid_to_string(self_inode->get_ino()) + ", " + d_name + ")");

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

	if (s_inode) {
		s_inode->set_mtime(self_inode->get_mtime());
		s_inode->set_ctime(self_inode->get_ctime());
	} else {
		s_inode = std::make_unique<inode>(*self_inode);
	}

	return 0;
}

int transaction::rmdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino)
{
	global_logger.log(transaction_ops, "Called transaction::rmdir(" + uuid_to_string(self_inode->get_ino()) + "," + d_name + ")");

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

	if (s_inode) {
		s_inode->set_mtime(self_inode->get_mtime());
		s_inode->set_ctime(self_inode->get_ctime());
	} else {
		s_inode = std::make_unique<inode>(*self_inode);
	}

	return 0;
}

int transaction::mkreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode)
{
	global_logger.log(transaction_ops, "Called transaction::mkreg(" + uuid_to_string(f_inode->get_ino()) + ", " + f_name + ")");

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

	if (s_inode) {
		s_inode->set_mtime(self_inode->get_mtime());
		s_inode->set_ctime(self_inode->get_ctime());
	} else {
		s_inode = std::make_unique<inode>(*self_inode);
	}

	auto f_inodes_ret = f_inodes.insert({uuid_to_string(f_inode->get_ino()), nullptr});
	if (f_inodes_ret.second || !f_inodes_ret.first->second) {
		f_inodes_ret.first.value() = std::make_unique<inode>(*f_inode);
	} else {
		throw std::logic_error("transaction::mkreg() failed (file already exists)");
	}

	return 0;
}

int transaction::rmreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode)
{
	global_logger.log(transaction_ops, "Called transaction::rmreg(" + uuid_to_string(f_inode->get_ino()) + ", " + f_name + ")");

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

	if (s_inode) {
		s_inode->set_mtime(self_inode->get_mtime());
		s_inode->set_ctime(self_inode->get_ctime());
	} else {
		s_inode = std::make_unique<inode>(*self_inode);
	}

	auto f_inodes_ret = f_inodes.insert({uuid_to_string(f_inode->get_ino()), nullptr});
	if (!f_inodes_ret.second) {
		if (f_inodes_ret.first->second) {
			f_inodes_ret.first.value() = nullptr;
		} else {
			throw std::logic_error("transaction::rmreg() failed (file doesn't exist)");
		}
	}

	return 0;
}

int transaction::chreg(std::shared_ptr<inode> f_inode)
{
	global_logger.log(transaction_ops, "Called transaction::chreg(" + uuid_to_string(f_inode->get_ino()) + ")");

	std::unique_lock lock(m);

	if (committed)
		return -1;

	auto f_inodes_ret = f_inodes.insert({uuid_to_string(f_inode->get_ino()), nullptr});
	f_inodes_ret.first.value() = std::make_unique<inode>(*f_inode);

	return 0;
}

void transaction::sync(std::shared_ptr<rados_io> meta)
{
	global_logger.log(transaction_ops, "Called sync()");

	if (status == self_status::S_DELETED) {
		meta->remove(obj_category::INODE, uuid_to_string(s_ino));
		meta->remove(obj_category::DENTRY, uuid_to_string(s_ino));
		return;
	}

	/* s_inode */
	if (s_inode)
		s_inode->sync();

	/* dentries */
	dentry d(s_ino, status == self_status::S_CREATED);
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
		if (p.second)
			p.second->sync();
}

void transaction::commit(std::shared_ptr<rados_io> meta)
{
	global_logger.log(transaction_ops, "Called commit()");

	{
		std::unique_lock lock(m);

		if (committed)
			throw std::logic_error("transaction::commit() failed (tx has been already committed)");
		committed = true;
	}

	auto raw = serialize();

	size_t obj_size;
	meta->stat(obj_category::JOURNAL, uuid_to_string(s_ino), obj_size);
	meta->write(obj_category::JOURNAL, uuid_to_string(s_ino), raw.data(), raw.size(), obj_size);

	/* Update offset */
	offset = obj_size;
}

void transaction::checkpoint(std::shared_ptr<rados_io> meta)
{
	global_logger.log(transaction_ops, "Called checkpoint()");

	/* Synchronize */
	sync(meta);

	/* Clear the checkpoint bit */
	meta->write(obj_category::JOURNAL, uuid_to_string(s_ino), "\0", 1, offset);
}
