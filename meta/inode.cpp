#include "inode.hpp"
#include "../client/client.hpp"

using std::runtime_error;

extern rados_io *meta_pool;
uint64_t per_client_ino_offset = 1;

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
	global_logger.log("Called inode(new file)");
	struct timespec ts;
	if (!timespec_get(&ts, TIME_UTC))
		runtime_error("timespec_get() failed");
	i_atime = i_mtime = i_ctime = ts;
	i_ino = alloc_new_ino();
}

inode::inode(uid_t owner, gid_t group, mode_t mode, ino_t symlink_target_ino) : i_mode(mode), i_uid(owner), i_gid(group), i_nlink(1), i_size(0),  symlink_target_ino(symlink_target_ino)
{
	global_logger.log("Called inode(symlink)");
	struct timespec ts;
	if (!timespec_get(&ts, TIME_UTC))
		runtime_error("timespec_get() failed");
	i_atime = i_mtime = i_ctime = ts;
	i_ino = alloc_new_ino();
}

inode::inode(const std::string &path)
{
	global_logger.log("Called inode(" + path + ")");
	inode *parent_inode, *target_inode;
	dentry *parent_dentry;

	/* exception handle if path == "/" */
	parent_inode = new inode(0);
	parent_dentry = new dentry(0);

	int start_name, end_name = -1;
	int path_len = path.length();

	while(true){
		// get new target name
		start_name = end_name + 2;
		if(start_name > path_len)
			break;
		for(int i = start_name; i < path_len; i++){
			if(path.at(i) == '/'){
				end_name = i - 1;
				break;
			}
		}
		std::string target_name = path.substr(start_name, end_name - start_name + 1);
		global_logger.log("Check target: " + target_name);

		// translate target name to target's inode number
		ino_t target_ino = parent_dentry->get_child_ino(target_name);
		if (target_ino == -1)
			throw no_entry("No such file or Directory: inode number " + std::to_string(target_ino));

		inode *target_inode = new inode(target_ino);

		/* resolve symlink to original file/directory */
		if(S_ISLNK(target_inode->get_mode())) {
			ino_t origin_ino = target_inode->get_ino();
			free(target_inode);
			try {
				target_inode = new inode(origin_ino);
			} catch(no_entry &e){
				throw no_entry("No such file or Directory: inode number " + std::to_string(origin_ino));
			}
		}
		if(!permission_check(target_inode, X_OK))
			throw permission_denied("Permission Denied: " + target_name);

		// target become next parent
		free(parent_inode);
		free(parent_dentry);

		parent_inode = target_inode;
		if(S_ISDIR(parent_inode->get_mode()))
			parent_dentry = new dentry(parent_inode->get_ino());
	}

	this->copy(target_inode);
	free(parent_inode);
	free(parent_dentry);


}

inode::inode(ino_t ino)
{
	global_logger.log("Called inode(" + std::to_string(ino) + ")");
	char *raw_data = (char *)malloc(sizeof(inode));
	try {
		meta_pool->read("i$" + std::to_string(ino), raw_data, sizeof(inode), 0);
		this->deserialize(raw_data);
	} catch(rados_io::no_such_object &e){
		throw no_entry("No such file or Directory: inode number " + std::to_string(ino));
	}
}

void inode::copy(inode *src)
{
	global_logger.log("Called inode.copy()");
	memcpy(this, src, sizeof(inode));
}

void inode::fill_stat(struct stat *s)
{
	global_logger.log("Called inode.fill_stat()");
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
	global_logger.log("Called inode.serialize()");
	unique_ptr<char> value(new char[sizeof(inode)]);
	memcpy(value.get(), this, sizeof(inode));
	return std::move(value);
}

void inode::deserialize(const char *value)
{
	global_logger.log("Called inode.deserialize()");
	memcpy(this, value, sizeof(inode));
}

void inode::sync()
{
	global_logger.log("Called inode.sync()");
	unique_ptr<char> raw = this->serialize();
	meta_pool->write("i$" + std::to_string(this->i_ino), raw.get(), sizeof(inode), 0);
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

ino_t alloc_new_ino() {
	global_logger.log("Called alloc_new_ino()");
	fuse_context *fuse_ctx = fuse_get_context();
	client *c = (client *) (fuse_ctx->private_data);
	ino_t new_ino;
	/* new_ino use client_id for first 24 bit and use ino_offset for next 40 bit, total 64bit(8bytes) */
	if (c != NULL) {
		new_ino = (c->get_client_id()) << 40;
		new_ino = new_ino + (per_client_ino_offset & INO_OFFSET_MASK);
		global_logger.log("new inode number : " + std::to_string(new_ino));
	} else { /* for very first client */
		new_ino = (1) << 40;
		new_ino = new_ino + (per_client_ino_offset & INO_OFFSET_MASK);
		global_logger.log("new inode number : " + std::to_string(new_ino));
	}

	return new_ino;
}

bool permission_check(inode *i, int mask){
	global_logger.log("Called permission_check");
	bool check_read = (mask & R_OK) ? true : false;
	bool check_write = (mask & W_OK) ? true : false;
	bool check_exec = (mask & X_OK) ? true : false;
	bool ret = true;

	mode_t target_mode;
	fuse_context *fuse_ctx = fuse_get_context();

	if(fuse_ctx->uid == i->get_uid()){
		target_mode = (i->get_mode() & S_IRWXU) >> 6;
	} else if (fuse_ctx->gid == i->get_gid()){
		target_mode = (i->get_mode() & S_IRWXG) >> 3;
	} else {
		target_mode = i->get_mode() & S_IRWXO;
	}

	if(check_read){
		ret = ret & (target_mode & R_OK);
	}
	if(check_write){
		ret = ret & (target_mode & W_OK);
	}
	if(check_exec){
		ret = ret & (target_mode & X_OK);
	}

	return ret;

}