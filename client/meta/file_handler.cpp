#include "file_handler.hpp"

file_handler::file_handler(ino_t ino) : ino(ino), fhno(0) {

}

void file_handler::set_fhno(void *this_fhno) {
	this->fhno = reinterpret_cast<uint64_t>(fhno);
}

ino_t  file_handler::get_ino() {
	return this->ino;
}

std::shared_ptr<inode> file_handler::get_open_inode_info() {
	if(this->loc == LOCAL){
		return this->i;
	} else if (this->loc == REMOTE) {
		return std::dynamic_pointer_cast<inode>(this->remote_i);
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
