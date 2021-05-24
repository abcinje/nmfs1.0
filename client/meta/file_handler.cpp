#include "file_handler.hpp"

file_handler::file_handler(uuid ino) : ino(ino), fhno(0) {

}

void file_handler::set_fhno(uint64_t this_fhno) {
	this->fhno = reinterpret_cast<uint64_t>(this_fhno);
}

uuid  file_handler::get_ino() {
	return this->ino;
}

std::shared_ptr<inode> file_handler::get_open_inode_info() {
	global_logger.log(file_handler_ops, "Called get_open_inode_info()");
	if(this->loc == LOCAL){
		return this->i;
	} else if (this->loc == REMOTE) {
		return std::static_pointer_cast<inode>(this->remote_i);
	} else {
		throw std::runtime_error("file_handler is corrupted : cannot know loc of opened file");
	}
}

void file_handler::set_loc(uint64_t this_loc) {
	file_handler::loc = this_loc;
}

void file_handler::set_i(const std::shared_ptr<inode> &open_i) {
	file_handler::i = open_i;
}

void file_handler::set_remote_i(const std::shared_ptr<remote_inode> &open_remote_i) {
	file_handler::remote_i = open_remote_i;
}

void file_handler_list::add_file_handler(uint64_t key, std::shared_ptr<file_handler> fh) {
	global_logger.log(file_handler_ops, "Called add_file_handler()");
	std::scoped_lock scl{this->file_handler_mutex};
	auto ret = fh_list.insert(std::make_pair(key, nullptr));
	if(ret.second) {
		ret.first->second = fh;
	} else {
		throw std::runtime_error("file_handler is corrupted : cannot add file_handler to list");

	}
}

std::shared_ptr<file_handler> file_handler_list::get_file_handler(uint64_t key) {
	global_logger.log(file_handler_ops, "Called get_file_handler()");
	std::scoped_lock scl{this->file_handler_mutex};
	std::map<uint64_t , std::shared_ptr<file_handler>>::iterator it;
	it = fh_list.find(key);

	if (it == fh_list.end())
		throw std::runtime_error("file_handler is corrupted : cannot get file_handler from list");

	return it->second;
}

int file_handler_list::delete_file_handler(uint64_t key) {
	global_logger.log(file_handler_ops, "Called delete_file_handler()");
	std::scoped_lock scl{this->file_handler_mutex};
	std::map<uint64_t , std::shared_ptr<file_handler>>::iterator it;
	it = fh_list.find(key);

	if (it == fh_list.end())
		return -EIO;

	fh_list.erase(it);
	return 0;
}
