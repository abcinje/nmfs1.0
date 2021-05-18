#ifndef NMFS0_FILE_HANDLER_HPP
#define NMFS0_FILE_HANDLER_HPP

#include <memory>
#include <sys/stat.h>
#include "remote_inode.hpp"

class file_handler {
private:
    	ino_t ino;
    	uint64_t loc;
	uint64_t fhno;

	std::shared_ptr<inode> i;
	std::shared_ptr<remote_inode> remote_i;
public:
	explicit file_handler(ino_t ino);

	ino_t get_ino();
	std::shared_ptr<inode> get_open_inode_info();

    	void set_fhno(uint64_t this_fhno);
    	void set_loc(uint64_t this_loc);

    	void set_i(const std::shared_ptr<inode> &open_i);
    	void set_remote_i(const std::shared_ptr<remote_inode> &open_remote_i);
};

class file_handler_list {
private:
    /* <fuse_file_info->fh, file_handler> */
    std::map<uint64_t, std::shared_ptr<file_handler>> fh_list;

public:
    	std::mutex file_handler_mutex;
    	void add_file_handler(uint64_t key, std::shared_ptr<file_handler> fh);
	std::shared_ptr<file_handler> get_file_handler(uint64_t key);
	int delete_file_handler(uint64_t key);
};

#endif //NMFS0_FILE_HANDLER_HPP
