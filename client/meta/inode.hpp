#ifndef _INODE_HPP_
#define _INODE_HPP_

#include <memory>
#include <sys/stat.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <mutex>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include "../fs_ops/fuse_ops.hpp"
#include "../../lib/rados_io/rados_io.hpp"
#include "../../lib/logger/logger.hpp"
#include "dentry.hpp"
#include "../client/client.hpp"
#include "../util.hpp"

#define INO_OFFSET_MASK (0x000000FFFFFFFFFF)
#define VFTABLE_OFFSET 8
#define REG_INODE_SIZE (sizeof(inode) - sizeof(char *) - sizeof(std::recursive_mutex) - VFTABLE_OFFSET)
#define DIR_INODE_SIZE 4096
#define ENOTLEADER 8000

using std::unique_ptr;
using std::runtime_error;
using std::string;
using namespace boost::uuids;

enum meta_location {
    LOCAL = 0,
    REMOTE,
    UNKNOWN,
    NOBODY /* temporaly status */
};

class uuid_controller {
private:
    random_generator generator;
public:
    uuid_controller() = default;

    uuid alloc_new_uuid();
    uint64_t get_postfix_from_uuid(uuid id);
    uint64_t get_prefix_from_uuid(uuid id);
    uuid splice_prefix_and_postfix(uint64_t prefix, uint64_t postfix);
};

uuid get_root_ino();
std::string uuid_to_string(uuid id);

class inode {
private:
	mode_t	i_mode;
	uid_t	i_uid;
	gid_t	i_gid;
	uuid	i_ino;
	nlink_t	i_nlink;
	off_t	i_size;

	struct timespec	i_atime;
	struct timespec	i_mtime;
	struct timespec	i_ctime;

	uint64_t loc;

	int link_target_len;
	char *link_target_name;

public:
	std::recursive_mutex inode_mutex;
	class no_entry : public runtime_error {
	public:
		no_entry(const string &msg);
		const char *what(void);
	};

	class permission_denied : public runtime_error {
	public:
		permission_denied(const string &msg);
		const char *what(void);
	};

	inode(const inode &copy);

	/* for normal reg file and directory */
	inode(uid_t owner, gid_t group, mode_t mode, bool root = false);
	/* for symlink */
	inode(uid_t owner, gid_t group, mode_t mode, const char *link_target_name);
	/* for pull metadata */
	inode(uuid ino);
	/* parent constructor for remote_inode and dummy_inode which used with file_handler */
	inode();

	void fill_stat(struct stat *s);
	std::vector<char> serialize(void);
	void deserialize(const char *value);
	void sync();
	virtual void permission_check(int mask);

	// getter
	virtual mode_t get_mode();
	uid_t get_uid();
	gid_t get_gid();
	uuid get_ino();
	nlink_t get_nlink();
	off_t get_size();
	struct timespec get_atime();
	struct timespec get_mtime();
	struct timespec get_ctime();

	uint64_t get_loc();

	int get_link_target_len();
	char *get_link_target_name();

	// setter
	void set_mode(mode_t mode);
	void set_uid(uid_t uid);
	void set_gid(gid_t gid);
	void set_ino(uuid ino);
	void set_nlink(nlink_t nlink);
	void set_size(off_t size);
	void set_atime(struct timespec atime);
	void set_mtime(struct timespec mtime);
	void set_ctime(struct timespec ctime);

	void set_loc(uint64_t loc);
	void set_link_target_len(int len);
	void set_link_target_name(const char *name);
};

uuid alloc_new_ino();

#endif /* _INODE_HPP_ */
