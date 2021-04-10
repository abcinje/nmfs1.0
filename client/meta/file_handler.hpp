#ifndef NMFS0_FILE_HANDLER_HPP
#define NMFS0_FILE_HANDLER_HPP

#include <memory>
#include <sys/stat.h>

class file_handler {
	ino_t ino;
	uint64_t fhno;

public:
	file_handler(ino_t ino);

	void set_fhno(void *fhno);
	ino_t get_ino();
};


#endif //NMFS0_FILE_HANDLER_HPP
