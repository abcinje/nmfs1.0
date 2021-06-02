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
	global_logger.log(journal_ops, "Called check()");
	jtable.delete_entry(self_ino);

	std::vector<char> value(OBJ_SIZE);
	size_t obj_size = meta->read(obj_category::JOURNAL, uuid_to_string(self_ino), value.data(), OBJ_SIZE, 0);

	size_t index = 0;
	while (index < obj_size) {
		int32_t tx_size = *(reinterpret_cast<int32_t *>(&value[index]));
		std::vector<char> raw(&value[index], &value[index] + tx_size);
		transaction tx(self_ino);
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
	global_logger.log(journal_ops, "Called journal::chself(" + uuid_to_string(self_inode->get_ino()) + ")");
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->chself(self_inode))
			break;
	}
}

void journal::chreg(const uuid &self_ino, std::shared_ptr<inode> f_inode)
{
	global_logger.log(journal_ops, "Called journal::chreg(" + uuid_to_string(self_ino) + ")");
	while (true) {
		auto tx = jtable.get_entry(self_ino);
		if (!tx->chreg(f_inode))
			break;
	}
}

void journal::mkdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino)
{
	global_logger.log(journal_ops, "Called journal::mkdir(" + uuid_to_string(self_inode->get_ino()) + d_name + ")");
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->mkdir(self_inode, d_name, d_ino))
			break;
	}
}

void journal::rmdir(std::shared_ptr<inode> self_inode, const std::string &d_name, const uuid &d_ino)
{
	global_logger.log(journal_ops, "Called journal::rmdir(" + uuid_to_string(self_inode->get_ino()) + d_name + ")");
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->rmdir(self_inode, d_name, d_ino))
			break;
	}
}

void journal::mkreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode)
{
	global_logger.log(journal_ops, "Called journal::mkreg(" + uuid_to_string(f_inode->get_ino()) + f_name + ")");
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->mkreg(self_inode, f_name, f_inode))
			break;
	}
}

void journal::rmreg(std::shared_ptr<inode> self_inode, const std::string &f_name, std::shared_ptr<inode> f_inode)
{
	global_logger.log(journal_ops, "Called journal::rmreg(" + uuid_to_string(f_inode->get_ino()) + f_name + ")");
	while (true) {
		auto tx = jtable.get_entry(self_inode->get_ino());
		if (!tx->rmreg(self_inode, f_name, f_inode))
			break;
	}
}
