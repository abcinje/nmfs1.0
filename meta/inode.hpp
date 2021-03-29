#ifndef _INODE_HPP_
#define _INODE_HPP_

#include <memory>
#include <sys/stat.h>
#include <stdexcept>
#include <string>
#include "../fuse_ops.hpp"
#include "../rados_io/rados_io.hpp"
#include "../logger/logger.hpp"
#include "dentry.hpp"

#define INO_OFFSET_MASK (0x000000FFFFFFFFFF)
#define REG_INODE_SIZE (sizeof(inode) - sizeof(char *))
using std::unique_ptr;
using std::runtime_error;
using std::string;

class inode {
private:
	mode_t	i_mode;
	uid_t	i_uid;
	gid_t	i_gid;
	ino_t	i_ino;
	nlink_t	i_nlink;
	off_t	i_size;

	struct timespec	i_atime;
	struct timespec	i_mtime;
	struct timespec	i_ctime;

	uint64_t leader_client_id;

	int link_target_len;
	char *link_target_name;

public:
	class no_entry : public runtime_error {
	public:
		no_entry(const char *msg);
		no_entry(const string &msg);
		const char *what(void);
	};

	class permission_denied : public runtime_error {
	public:
		permission_denied(const char *msg);
		permission_denied(const string &msg);
		const char *what(void);
	};

	//inode(uid_t owner, gid_t group, mode_t mode);
	inode(uid_t owner, gid_t group, mode_t mode, bool root = false);
	/* for symlink */
	inode(uid_t owner, gid_t group, mode_t mode, int link_target_len, const char *link_target_name);
	inode(std::string path);

	/* TODO : add permission check */
	inode(ino_t ino);

	void copy(inode *src);
	void fill_stat(struct stat *s);
	unique_ptr<char> serialize(void);
	void deserialize(const char *value);
	void sync();

	// getter
	mode_t get_mode();
	uid_t get_uid();
	gid_t get_gid();
	ino_t get_ino();
	nlink_t get_nlink();
	off_t get_size();
	struct timespec get_atime();
	struct timespec get_mtime();
	struct timespec get_ctime();

	int get_link_target_len();
	ino_t get_link_target_ino();
	char *get_link_target_name();

	// setter
	void set_mode(mode_t mode);
	void set_uid(uid_t uid);
	void set_gid(gid_t gid);
	void set_ino(ino_t ino);
	void set_nlink(nlink_t nlink);
	void set_size(off_t size);
	void set_atime(struct timespec atime);
	void set_mtime(struct timespec mtime);

	void set_link_target_len(int len);
	void set_link_target_name(const char *name);

};

ino_t alloc_new_ino();
void permission_check(inode *i, int mask);
int set_name_bound(int &start_name, int &end_name, std::string &path, int path_len);
#endif /* _INODE_HPP_ */
