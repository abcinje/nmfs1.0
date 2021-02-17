#include "inode.hpp"

#include <cstring>
#include <stdexcept>

using std::runtime_error;

inode::inode(uid_t owner, gid_t group, mode_t mode) : i_mode(mode), i_uid(owner), i_gid(group), i_nlink(1), i_size(0)
{
	struct timespec ts;
	if (!timespec_get(&ts, TIME_UTC))
		runtime_error("timespec_get() failed");
	i_atime = i_mtime = i_ctime = ts;
}

void inode::fill_stat(struct stat *s)
{
	s->st_mode	= i_mode;
	s->st_uid	= i_uid;
	s->st_gid	= i_gid;
	s->st_ino	= i_ino;
	s->st_nlink	= i_nlink;
	s->st_size	= i_size;

	s->st_atim.tv_sec	= i_atime.tv_sec;
	s->st_atim.tv_nsec	= i_atime.tv_nsec;
	s->st_mtim.tv_sec	= i_mtime.tv_sec;
	s->st_mtim.tv_nsec	= i_mtime.tv_nsec;
	s->st_ctim.tv_sec	= i_ctime.tv_sec;
	s->st_ctim.tv_sec	= i_ctime.tv_nsec;
}

unique_ptr<char> inode::serialize(void)
{
	unique_ptr<char> value(new char[sizeof(inode)]);
	memcpy(value.get(), this, sizeof(inode));
	return std::move(value);
}

void inode::deserialize(unique_ptr<char> value)
{
	memcpy(this, value.get(), sizeof(inode));
}
