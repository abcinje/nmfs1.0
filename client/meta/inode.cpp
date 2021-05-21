#include "inode.hpp"

#include <cstring>

using std::runtime_error;

extern rados_io *meta_pool;
extern client *this_client;
extern std::unique_ptr<uuid_controller> ino_controller;
std::recursive_mutex alloc_mutex;

inode::no_entry::no_entry(const string &msg) : runtime_error(msg)
{
}

const char *inode::no_entry::what(void)
{
	return runtime_error::what();
}

inode::permission_denied::permission_denied(const string &msg) : runtime_error(msg)
{
}

const char *inode::permission_denied::what(void)
{
	return runtime_error::what();
}

inode::inode(const inode &copy)
{
	i_mode		= copy.i_mode;
	i_uid		= copy.i_uid;
	i_gid		= copy.i_gid;
	i_ino		= copy.i_ino;
	i_nlink		= copy.i_nlink;
	i_size		= copy.i_size;

	i_atime		= copy.i_atime;
	i_mtime		= copy.i_mtime;
	i_ctime		= copy.i_ctime;

	link_target_len		= copy.link_target_len;
	link_target_name	= reinterpret_cast<char *>(malloc(link_target_len + 1));
	memcpy(link_target_name, copy.link_target_name, link_target_len + 1);
}

inode::inode(uid_t owner, gid_t group, mode_t mode, bool root) : i_mode(mode), i_uid(owner), i_gid(group), i_nlink(1), i_size(0), link_target_len(0), link_target_name(NULL)
{
	global_logger.log(inode_ops, "Called inode(new file)");
	struct timespec ts;
	if (!timespec_get(&ts, TIME_UTC))
		runtime_error("timespec_get() failed");
	i_atime = i_mtime = i_ctime = ts;
	loc = LOCAL;
	i_ino = root ? get_root_ino() : alloc_new_ino();
}

/* TODO : allocated target name should be freed later */
inode::inode(uid_t owner, gid_t group, mode_t mode, const char *link_target_name) : i_mode(mode), i_uid(owner), i_gid(group), i_nlink(1), i_size(0)
{
	int link_target_len = static_cast<int>(strlen(link_target_name));

	global_logger.log(inode_ops,"Called inode(symlink)");
	global_logger.log(inode_ops,"link_target_len : " + std::to_string(link_target_len) + " link_target_name : " + std::string(link_target_name));
	struct timespec ts;
	if (!timespec_get(&ts, TIME_UTC))
		runtime_error("timespec_get() failed");
	i_atime = i_mtime = i_ctime = ts;
	loc = LOCAL;
	i_ino = alloc_new_ino();

	this->set_link_target_len(link_target_len);
	this->set_link_target_name(link_target_name);
}


inode::inode(uuid ino)
{
	global_logger.log(inode_ops, "Called inode(" + uuid_to_string(ino) + ")");
	unique_ptr<char[]> raw_data = std::make_unique<char[]>(REG_INODE_SIZE);
	try {
		meta_pool->read(obj_category::INODE, uuid_to_string(ino), raw_data.get(), REG_INODE_SIZE, 0);
		this->deserialize(raw_data.get());
	} catch(rados_io::no_such_object &e){
		throw no_entry("No such file or Directory: in inode(ino) constructor");
	}
}

inode::inode(){
	loc = REMOTE;
}


void inode::fill_stat(struct stat *s)
{
	global_logger.log(inode_ops, "Called inode.fill_stat()");

	s->st_mode	= i_mode;
	s->st_uid	= i_uid;
	s->st_gid	= i_gid;
	s->st_ino	= ino_controller->get_postfix_from_uuid(i_ino);
	s->st_nlink	= i_nlink;
	s->st_size	= i_size;

	s->st_atim.tv_sec	= i_atime.tv_sec;
	s->st_atim.tv_nsec	= i_atime.tv_nsec;
	s->st_mtim.tv_sec	= i_mtime.tv_sec;
	s->st_mtim.tv_nsec	= i_mtime.tv_nsec;
	s->st_ctim.tv_sec	= i_ctime.tv_sec;
	s->st_ctim.tv_sec	= i_ctime.tv_nsec;
}
/* TODO : what is difference this + VFTABLE_OFFSET and &i_mode */
std::vector<char> inode::serialize(void)
{
	global_logger.log(inode_ops, "Called inode.serialize()");
	global_logger.log(inode_ops, "serialized ino : " + uuid_to_string(this->i_ino));
	std::vector<char> value(REG_INODE_SIZE + this->link_target_len);
	memcpy(value.data(), &i_mode, REG_INODE_SIZE);

	if(S_ISLNK(this->i_mode) && (this->link_target_len > 0)){
		global_logger.log(inode_ops, "serialize symbolic link inode");
		memcpy(value.data() + REG_INODE_SIZE, (this->link_target_name), this->link_target_len);
	}
	return value;
}

void inode::deserialize(const char *value)
{
	global_logger.log(inode_ops, "Called inode.deserialize()");
	memcpy(&i_mode, value, REG_INODE_SIZE);

	if(S_ISLNK(this->i_mode)){
		char *raw = (char *)calloc(this->link_target_len + 1, sizeof(char));
		meta_pool->read(obj_category::INODE, uuid_to_string(this->i_ino), raw, this->link_target_len, REG_INODE_SIZE);
		this->link_target_name = raw;
		global_logger.log(inode_ops, "deserialized link target name : " + std::string(this->link_target_name));
	}

	global_logger.log(inode_ops, "deserialized ino : " + uuid_to_string(this->i_ino));
	global_logger.log(inode_ops, "deserialized size : " + std::to_string(this->i_size));
}

void inode::sync()
{
	global_logger.log(inode_ops, "Called inode.sync()");
	std::vector<char> raw = this->serialize();
	meta_pool->write(obj_category::INODE, uuid_to_string(this->i_ino), raw.data(), REG_INODE_SIZE + this->link_target_len, 0);
}

void inode::permission_check(int mask){
	global_logger.log(inode_ops, "Called permission_check");
	bool check_read = (mask & R_OK) ? true : false;
	bool check_write = (mask & W_OK) ? true : false;
	bool check_exec = (mask & X_OK) ? true : false;
	bool ret = true;

	mode_t target_mode;

	if(this_client->get_client_uid() == this->i_uid){
		target_mode = (this->i_mode & S_IRWXU) >> 6;
	} else if (this_client->get_client_gid() == this->i_gid){
		target_mode = (this->i_mode & S_IRWXG) >> 3;
	} else {
		target_mode = this->i_mode & S_IRWXO;
	}

	if(check_read){
		ret = ret && (target_mode & R_OK);
	}
	if(check_write){
		ret = ret && (target_mode & W_OK);
	}
	if(check_exec){
		ret = ret && (target_mode & X_OK);
	}

	if(!ret)
		throw permission_denied("Permission Denied: Local");
}

// getter
mode_t inode::get_mode(){
	return this->i_mode;
}
uid_t inode::get_uid(){
	return this->i_uid;
}
gid_t inode::get_gid(){
	return this->i_gid;
}
uuid inode::get_ino() {
	return this->i_ino;
}
nlink_t inode::get_nlink() {
	return this->i_nlink;
}
off_t inode::get_size(){
	return this->i_size;
}
struct timespec inode::get_atime(){
	return this->i_atime;
}
struct timespec inode::get_mtime(){
	return this->i_mtime;
}
struct timespec inode::get_ctime(){
	return this->i_ctime;
}

uint64_t inode::get_loc() {
	return this->loc;
}
int inode::get_link_target_len(){
	return this->link_target_len;
}
char *inode::get_link_target_name(){
	return this->link_target_name;
}

// setter
void inode::set_mode(mode_t mode){
	this->i_mode = mode;
}
void inode::set_uid(uid_t uid){
	this->i_uid = uid;
}
void inode::set_gid(gid_t gid){
	this->i_gid = gid;
}
void inode::set_ino(uuid ino){
	this->i_ino = ino;
}
void inode::set_nlink(nlink_t nlink){
	this->i_nlink = nlink;
}
void inode::set_size(off_t size){
	this->i_size = size;
}
void inode::set_atime(struct timespec atime){
	this->i_atime = atime;
}
void inode::set_mtime(struct timespec mtime){
	this->i_mtime = mtime;
}
void inode::set_ctime(struct timespec ctime){
	this->i_ctime = ctime;
}

void inode::set_loc(uint64_t loc) {
	this->loc = loc;
}
void inode::set_link_target_len(int len){
	this->link_target_len = len;
}
void inode::set_link_target_name(const char *name){
	this->link_target_name = (char *)calloc(this->link_target_len + 1, sizeof(char));
	memcpy(this->link_target_name, name, this->link_target_len);
}

uuid alloc_new_ino() {
	global_logger.log(inode_ops, "Called alloc_new_ino()");
	uuid new_ino = ino_controller->alloc_new_uuid();

	global_logger.log(inode_ops, "new inode number : " + uuid_to_string(new_ino));
	return new_ino;
}
