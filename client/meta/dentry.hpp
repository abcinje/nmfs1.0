#ifndef NMFS0_DENTRY_HPP
#define NMFS0_DENTRY_HPP

#include <map>
#include <utility>
#include <mutex>
#include <shared_mutex>
#include "../fs_ops/fuse_ops.hpp"
#include "../rados_io/rados_io.hpp"
#include "../logger/logger.hpp"

#define MAX_DENTRY_OBJ_SIZE (4 * 1024 * 1024)
using std::unique_ptr;

class dentry_table;

/*TODO : when total dentries size exceed single object size */
class dentry {
private:
	ino_t this_ino;
	uint64_t child_num;
	uint64_t total_name_length;
	std::map<std::string, ino_t> child_list;

	std::recursive_mutex dentry_mutex;

public:
	dentry(ino_t ino);
	dentry(ino_t ino, bool flag);

	void add_new_child(const std::string &filename, ino_t ino);
	void delete_child(const std::string &filename);

	unique_ptr<char[]> serialize();
	void deserialize(char *raw);
	void sync();

	ino_t get_child_ino(std::string child_name);
	void fill_filler(void *buffer, fuse_fill_dir_t filler);

	uint64_t get_child_num();
	uint64_t get_total_name_legth();

	friend class dentry_table;
};


#endif //NMFS0_DENTRY_HPP
