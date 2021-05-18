#ifndef NMFS0_FILE_HANDLER_HPP
#define NMFS0_FILE_HANDLER_HPP

#include <memory>
#include <sys/stat.h>
#include "remote_inode.hpp"

class file_handler {
    	ino_t ino;
    	uint64_t loc;
	uint64_t fhno;

	std::shared_ptr<inode> i;
	std::shared_ptr<remote_inode> remote_i;
public:
	explicit file_handler(ino_t ino);

	ino_t get_ino();
	std::shared_ptr<inode> get_open_inode_info();

    	void set_fhno(void *this_fhno);
    	void set_loc(uint64_t this_loc);

    	void set_i(const std::shared_ptr<inode> &open_i);
    	void set_remote_i(const std::shared_ptr<remote_inode> &open_remote_i);
};


#endif //NMFS0_FILE_HANDLER_HPP
