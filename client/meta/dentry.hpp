#ifndef NMFS0_DENTRY_HPP
#define NMFS0_DENTRY_HPP

#include <map>
#include <mutex>
#include <utility>

#include <tsl/robin_map.h>

#include "lib/logger/logger.hpp"
#include "lib/rados_io/rados_io.hpp"

#include "inode.hpp"
#include "../fs_ops/fuse_ops.hpp"

#define MAX_DENTRY_OBJ_SIZE OBJ_SIZE

using std::unique_ptr;
using namespace boost::uuids;

class dentry_table;

/*TODO : when total dentries size exceed single object size */
class dentry {
private:
	uuid this_ino;
	tsl::robin_map<std::string, uuid> child_list;

public:
	explicit dentry(uuid ino, bool mkdir = false);

	void add_child(const std::string &filename, uuid ino);
	void delete_child(const std::string &filename);

	unique_ptr<char[]> serialize();
	void deserialize(char *raw);
	void sync();

	uuid get_child_ino(const std::string& child_name) const;
	void fill_filler(void *buffer, fuse_fill_dir_t filler) const;

	uint64_t get_child_num() const;
	uint64_t get_total_name_length() const;

	friend class dentry_table;
};


#endif //NMFS0_DENTRY_HPP
