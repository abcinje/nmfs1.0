#include "inode.hpp"

#include <cstring>
#include <stdexcept>

using std::runtime_error;

inode::no_entry::no_entry(const char *msg) : runtime_error(msg){

}
inode::no_entry::no_entry(const string &msg) : runtime_error(msg.c_str()){

}
const char *inode::no_entry::what(void)
{
	return runtime_error::what();
}

inode:: permission_denied::permission_denied(const char *msg) : runtime_error(msg){

}
inode:: permission_denied::permission_denied(const string &msg) : runtime_error(msg.c_str()){

}
const char *inode::permission_denied::what(void)
{
	return runtime_error::what();
}


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

// getter
mode_t inode::get_mode(){return this->i_mode;}
uid_t inode::get_uid(){return this->i_uid;}
gid_t inode::get_gid(){return this->i_gid;}
ino_t inode::get_ino() {return this->i_ino;}
nlink_t inode::get_nlink() {return this->i_nlink;}
off_t inode::get_size(){return this->i_size;}
struct timespec inode::get_atime(){return this->i_atime;}
struct timespec inode::get_mtime(){return this->i_mtime;}
struct timespec inode::get_ctime(){return this->i_ctime;}

// setter
void inode::set_mode(mode_t mode){this->i_mode = mode;}
void inode::set_uid(uid_t uid){this->i_uid = uid;}
void inode::set_gid(gid_t gid){this->i_gid = gid;}
void inode::set_ino(ino_t ino){this->i_ino = ino;}
void inode::set_nlink(nlink_t nlink){this->i_nlink = nlink;}
void inode::set_size(off_t size){this->i_size = size;}
void inode::set_atime(struct timespec atime){this->i_atime = atime;}
void inode::set_mtime(struct timespec mtime){this->i_mtime = mtime;}
