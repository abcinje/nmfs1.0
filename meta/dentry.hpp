#ifndef NMFS0_DENTRY_HPP
#define NMFS0_DENTRY_HPP

#include <map>
#include <fuse.h>
#include "../rados_io/rados_io.hpp"

#define MAX_DENTRY_OBJ_SIZE (4 * 1024 * 1024)
#define RAW_LINE_SIZE (256 + 8)
using std::unique_ptr;

class dentry {
private:
	ino_t ino;
	uint64_t child_num;
	std::map<std::string, ino_t> child_list;

public:
	dentry(ino_t ino);

	unique_ptr<char> serialize();
	void deserialize(char *raw);
	void sync();

	ino_t get_child_ino(std::string child_name);
	void fill_filler(void *buffer, fuse_fill_dir_t filler);
};


#endif //NMFS0_DENTRY_HPP
