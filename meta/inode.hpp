#ifndef _INODE_HPP_
#define _INODE_HPP_

#include <memory>
#include <sys/stat.h>

using std::unique_ptr;

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

public:
	inode(uid_t owner, gid_t group, mode_t mode);

	void fill_stat(struct stat *s);
	unique_ptr<char> serialize(void);
	void deserialize(unique_ptr<char>);
};

#endif /* _INODE_HPP_ */
