#include "journal.hpp"

journal::journal(std::shared_ptr<rados_io> meta_pool, std::shared_ptr<lease_client> lease) : meta(meta_pool), lc(lease), stopped(false)
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

void journal::check(const uuid &self_ino)
{
	jtable.delete_entry(self_ino);

	std::vector<char> value(OBJ_SIZE);
	size_t obj_size = meta->read(obj_category::JOURNAL, uuid_to_string(self_ino), value.data(), OBJ_SIZE, 0);

	size_t index = 0;
	while (index < obj_size) {
		int32_t tx_size = *(reinterpret_cast<int32_t *>(&value[index]));
		std::vector<char> raw(&value[index], &value[index] + tx_size);
		transaction tx;
		if (!tx.deserialize(raw)) {
			tx.sync();
		} else {
			throw std::runtime_error("journal::check() failed (not implemented yet)");
		}
		index += tx_size;
	}
}

void journal::chself(std::shared_ptr<inode> self_inode)
{
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->chself(self_inode))
			break;
	}
}

void journal::chreg(std::shared_ptr<inode> self_inode, std::shared_ptr<inode> f_inode)
{
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->chreg(self_inode, f_inode))
			break;
	}
}

void journal::mkdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino)
{
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->mkdir(self_inode, d_name, d_ino))
			break;
	}
}

void journal::rmdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino)
{
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->rmdir(self_inode, d_name, d_ino))
			break;
	}
}

void journal::mkreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode)
{
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->mkreg(self_inode, f_name, f_inode))
			break;
	}
}

void journal::rmreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode)
{
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->rmreg(self_inode, f_name, f_inode))
			break;
	}
}
