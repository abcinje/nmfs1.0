#include "file_handler.hpp"

file_handler::file_handler(ino_t ino) : ino(ino) {

}

void file_handler::set_fhno(void *fhno) {
	this->fhno = reinterpret_cast<uint64_t>(fhno);
}

ino_t  file_handler::get_ino() {
	return this->ino;
}