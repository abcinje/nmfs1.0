#include "inode.hpp"

using std::runtime_error;

extern rados_io *meta_pool;
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

inode::inode(uid_t owner, gid_t group, mode_t mode, bool root) : i_mode(mode), i_uid(owner), i_gid(group), i_nlink(1), i_size(0), link_target_len(0), link_target_name(NULL)
{
	global_logger.log(inode_ops, "Called inode(new file)");
	struct timespec ts;
	if (!timespec_get(&ts, TIME_UTC))
		runtime_error("timespec_get() failed");
	i_atime = i_mtime = i_ctime = ts;
	loc = LOCAL;
	i_ino = root ? 0 : alloc_new_ino();
}


/* TODO : allocated target name should be freed later */
inode::inode(uid_t owner, gid_t group, mode_t mode, int link_target_len, const char *link_target_name) : i_mode(mode), i_uid(owner), i_gid(group), i_nlink(1), i_size(0)
{
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


inode::inode(ino_t ino)
{
	global_logger.log(inode_ops, "Called inode(" + std::to_string(ino) + ")");
	unique_ptr<char[]> raw_data = std::make_unique<char[]>(REG_INODE_SIZE);
	try {
		meta_pool->read(INODE, std::to_string(ino), raw_data.get(), REG_INODE_SIZE, 0);
		this->deserialize(raw_data.get());
	} catch(rados_io::no_such_object &e){
		throw no_entry("No such file or Directory: in inode(ino) constructor");
	}
}

inode::inode(){
	loc = REMOTE;
}

void inode::copy(inode *src)
{
	global_logger.log(inode_ops, "Called inode.copy()");
	std::scoped_lock scl{this->inode_mutex};
	memcpy(this, src, REG_INODE_SIZE);
}

void inode::fill_stat(struct stat *s)
{
	global_logger.log(inode_ops, "Called inode.fill_stat()");

	std::scoped_lock scl{this->inode_mutex};
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
	global_logger.log(inode_ops, "Called inode.serialize()");
	std::scoped_lock scl{this->inode_mutex};
	unique_ptr<char> value(new char[REG_INODE_SIZE + this->link_target_len]);
	memcpy(value.get(), this, REG_INODE_SIZE);

	if(S_ISLNK(this->get_mode()) && (this->link_target_len > 0)){
		global_logger.log(inode_ops, "serialize symbolic link inode");
		memcpy(value.get() + REG_INODE_SIZE, (this->link_target_name), this->link_target_len);
	}
	return std::move(value);
}

void inode::deserialize(const char *value)
{
	global_logger.log(inode_ops, "Called inode.deserialize()");
	std::scoped_lock scl{this->inode_mutex};
	memcpy(this, value, REG_INODE_SIZE);

	if(S_ISLNK(this->get_mode())){
		char *raw = (char *)calloc(this->link_target_len + 1, sizeof(char));
		meta_pool->read(INODE, std::to_string(this->i_ino), raw, this->link_target_len, REG_INODE_SIZE);
		this->link_target_name = raw;
		global_logger.log(inode_ops, "serialized link target name : " + std::string(this->link_target_name));
	}

	global_logger.log(inode_ops, "serialized ino : " + std::to_string(this->i_ino));
	global_logger.log(inode_ops, "serialized size : " + std::to_string(this->i_size));
}

void inode::sync()
{
	global_logger.log(inode_ops, "Called inode.sync()");

	std::scoped_lock scl{this->inode_mutex};
	unique_ptr<char> raw = this->serialize();
	meta_pool->write(INODE, std::to_string(this->i_ino), raw.get(), REG_INODE_SIZE + this->link_target_len, 0);
}

void inode::permission_check(int mask){
	global_logger.log(inode_ops, "Called permission_check");
	bool check_read = (mask & R_OK) ? true : false;
	bool check_write = (mask & W_OK) ? true : false;
	bool check_exec = (mask & X_OK) ? true : false;
	bool ret = true;

	mode_t target_mode;
	fuse_context *fuse_ctx = fuse_get_context();

	std::scoped_lock scl{this->inode_mutex};
	if(fuse_ctx->uid == get_uid()){
		target_mode = (get_mode() & S_IRWXU) >> 6;
	} else if (fuse_ctx->gid == get_gid()){
		target_mode = (get_mode() & S_IRWXG) >> 3;
	} else {
		target_mode = get_mode() & S_IRWXO;
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
		throw permission_denied("Permission Denied: ");
}

// getter
mode_t inode::get_mode(){
	std::scoped_lock scl{this->inode_mutex};
	return this->i_mode;
}
uid_t inode::get_uid(){
	std::scoped_lock scl{this->inode_mutex};
	return this->i_uid;
}
gid_t inode::get_gid(){
	std::scoped_lock scl{this->inode_mutex};
	return this->i_gid;
}
ino_t inode::get_ino() {
	std::scoped_lock scl{this->inode_mutex};
	return this->i_ino;
}
nlink_t inode::get_nlink() {
	std::scoped_lock scl{this->inode_mutex};
	return this->i_nlink;
}
off_t inode::get_size(){
	std::scoped_lock scl{this->inode_mutex};
	return this->i_size;
}
struct timespec inode::get_atime(){
	std::scoped_lock scl{this->inode_mutex};
	return this->i_atime;
}
struct timespec inode::get_mtime(){
	std::scoped_lock scl{this->inode_mutex};
	return this->i_mtime;
}
struct timespec inode::get_ctime(){
	std::scoped_lock scl{this->inode_mutex};
	return this->i_ctime;
}

uint64_t inode::get_loc() {
	std::scoped_lock scl{this->inode_mutex};
	return this->loc;
}
int inode::get_link_target_len(){
	std::scoped_lock scl{this->inode_mutex};
	return this->link_target_len;
}
char *inode::get_link_target_name(){
	std::scoped_lock scl{this->inode_mutex};
	return this->link_target_name;
}

// setter
void inode::set_mode(mode_t mode){
	std::scoped_lock scl{this->inode_mutex};
	this->i_mode = mode;
}
void inode::set_uid(uid_t uid){
	std::scoped_lock scl{this->inode_mutex};
	this->i_uid = uid;
}
void inode::set_gid(gid_t gid){
	std::scoped_lock scl{this->inode_mutex};
	this->i_gid = gid;
}
void inode::set_ino(ino_t ino){
	std::scoped_lock scl{this->inode_mutex};
	this->i_ino = ino;
}
void inode::set_nlink(nlink_t nlink){
	std::scoped_lock scl{this->inode_mutex};
	this->i_nlink = nlink;
}
void inode::set_size(off_t size){
	std::scoped_lock scl{this->inode_mutex};
	this->i_size = size;
}
void inode::set_atime(struct timespec atime){
	std::scoped_lock scl{this->inode_mutex};
	this->i_atime = atime;
}
void inode::set_mtime(struct timespec mtime){
	std::scoped_lock scl{this->inode_mutex};
	this->i_mtime = mtime;
}

void inode::set_loc(uint64_t loc) {
	std::scoped_lock scl{this->inode_mutex};
	this->loc = loc;
}
void inode::set_link_target_len(int len){
	std::scoped_lock scl{this->inode_mutex};
	this->link_target_len = len;
}
void inode::set_link_target_name(const char *name){
	std::scoped_lock scl{this->inode_mutex};
	this->link_target_name = (char *)calloc(this->link_target_len + 1, sizeof(char));
	memcpy(this->link_target_name, name, this->link_target_len);
}

ino_t alloc_new_ino() {
	global_logger.log(inode_ops, "Called alloc_new_ino()");
	std::scoped_lock{alloc_mutex};
	fuse_context *fuse_ctx = fuse_get_context();
	client *c = (client *) (fuse_ctx->private_data);
	ino_t new_ino;
	/* new_ino use client_id for first 24 bit and use ino_offset for next 40 bit, total 64bit(8bytes) */
	if (c != NULL) {
		new_ino = (c->get_client_id()) << 40;
		new_ino = new_ino + (c->get_per_client_ino_offset() & INO_OFFSET_MASK);
		global_logger.log(inode_ops, "new inode number : " + std::to_string(new_ino));
	} else { /* for very first client */
		new_ino = (uint64_t)(1) << 40;
		new_ino = new_ino + (c->get_per_client_ino_offset() & INO_OFFSET_MASK);
		global_logger.log(inode_ops, "new inode number : " + std::to_string(new_ino));
	}

	c->increase_ino_offset();
	return new_ino;
}
