#include "inode.hpp"

using std::runtime_error;

extern std::shared_ptr<rados_io> meta_pool;
extern std::unique_ptr<client> this_client;
extern std::unique_ptr<uuid_controller> ino_controller;

inode::no_entry::no_entry(const string &msg) : runtime_error(msg)
{
}

const char *inode::no_entry::what()
{
	return runtime_error::what();
}

inode::permission_denied::permission_denied(const string &msg) : runtime_error(msg)
{
}

const char *inode::permission_denied::what()
{
	return runtime_error::what();
}

inode::inode(const inode &copy)
{
	core.i_mode = copy.core.i_mode;
	core.i_uid = copy.core.i_uid;
	core.i_gid = copy.core.i_gid;
	core.i_ino = copy.core.i_ino;
	core.i_nlink = copy.core.i_nlink;
	core.i_size = copy.core.i_size;

	core.i_atime = copy.core.i_atime;
	core.i_mtime = copy.core.i_mtime;
	core.i_ctime = copy.core.i_ctime;

	core.link_target_len = copy.core.link_target_len;
	if (S_ISLNK(this->core.i_mode) && (this->core.link_target_len > 0)) {
		link_target_name = copy.link_target_name;
		//link_target_name = reinterpret_cast<char *>(calloc(this->core.link_target_len + 1, sizeof(char)));
		//memcpy(link_target_name, copy.link_target_name, core.link_target_len);
	}
}

inode::inode(uuid parent_ino, uid_t owner, gid_t group, mode_t mode, bool root) {
	global_logger.log(inode_ops, "Called inode(new file)");
	struct timespec ts;
	if (!timespec_get(&ts, TIME_UTC))
		runtime_error("timespec_get() failed");

	p_ino = parent_ino;
	core = {
			.i_mode = mode,
			.i_uid = owner,
			.i_gid = group,
			.i_ino = root ? get_root_ino() : alloc_new_ino(),
			.i_nlink = 1,
			.i_size = 0,
			.i_atime = ts,
			.i_mtime = ts,
			.i_ctime = ts,
			.link_target_len = 0
	};

	loc = LOCAL;
	link_target_name = nullptr;
}

inode::inode(uuid parent_ino, uid_t owner, gid_t group, mode_t mode, uuid &predefined_ino) {
	global_logger.log(inode_ops, "Called inode(new directory)");
	struct timespec ts;
	if (!timespec_get(&ts, TIME_UTC))
		runtime_error("timespec_get() failed");

	p_ino = parent_ino;
	core = {
			.i_mode = mode,
			.i_uid = owner,
			.i_gid = group,
			.i_ino = predefined_ino,
			.i_nlink = 1,
			.i_size = 0,
			.i_atime = ts,
			.i_mtime = ts,
			.i_ctime = ts,
			.link_target_len = 0
	};

	loc = LOCAL;
	link_target_name = nullptr;
}

/* TODO : allocated target name should be freed later */
inode::inode(uuid parent_ino, uid_t owner, gid_t group, mode_t mode, const char *passed_link_target_name) {
	uint32_t passed_link_target_len = strlen(passed_link_target_name);

	global_logger.log(inode_ops,"Called inode(symlink)");
	global_logger.log(inode_ops,"link_target_len : " + std::to_string(passed_link_target_len) + " link_target_name : " + std::string(passed_link_target_name));
	struct timespec ts{};
	if (!timespec_get(&ts, TIME_UTC))
		runtime_error("timespec_get() failed");

	p_ino = parent_ino;
	core = {
			.i_mode = mode,
			.i_uid = owner,
			.i_gid = group,
			.i_ino = alloc_new_ino(),
			.i_nlink = 1,
			.i_size = 0,
			.i_atime = ts,
			.i_mtime = ts,
			.i_ctime = ts
	};

	loc = LOCAL;

	this->set_link_target_len(passed_link_target_len);
	this->set_link_target_name(std::make_shared<std::string>(passed_link_target_name));
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

inode::inode(enum meta_location loc) : loc(loc){
}

void inode::fill_stat(struct stat *s)
{
	global_logger.log(inode_ops, "Called inode.fill_stat()");

	s->st_mode	= core.i_mode;
	s->st_uid	= core.i_uid;
	s->st_gid	= core.i_gid;
	s->st_ino	= ino_controller->get_postfix_from_uuid(core.i_ino);
	s->st_nlink	= core.i_nlink;
	s->st_size	= core.i_size;

	s->st_atim.tv_sec	= core.i_atime.tv_sec;
	s->st_atim.tv_nsec	= core.i_atime.tv_nsec;
	s->st_mtim.tv_sec	= core.i_mtime.tv_sec;
	s->st_mtim.tv_nsec	= core.i_mtime.tv_nsec;
	s->st_ctim.tv_sec	= core.i_ctime.tv_sec;
	s->st_ctim.tv_sec	= core.i_ctime.tv_nsec;
}

std::vector<char> inode::serialize()
{
	global_logger.log(inode_ops, "Called inode.serialize()");
	global_logger.log(inode_ops, "serialized ino : " + uuid_to_string(this->core.i_ino));
	std::vector<char> value(REG_INODE_SIZE + this->core.link_target_len);
	memcpy(value.data(), &core, REG_INODE_SIZE);

	if(S_ISLNK(this->core.i_mode) && (this->core.link_target_len > 0)){
		global_logger.log(inode_ops, "serialize symbolic link inode");
		memcpy(value.data() + REG_INODE_SIZE, (this->link_target_name->data()), this->core.link_target_len);
	}
	return value;
}

void inode::deserialize(const char *value)
{
	global_logger.log(inode_ops, "Called inode.deserialize()");
	memcpy(&core, value, REG_INODE_SIZE);

	if(S_ISLNK(this->core.i_mode)){
		this->link_target_name = std::make_shared<std::string>();
		(*this->link_target_name).resize(this->core.link_target_len);
		meta_pool->read(obj_category::INODE, uuid_to_string(this->core.i_ino), &((*this->link_target_name)[0]), this->core.link_target_len, REG_INODE_SIZE);
		global_logger.log(inode_ops, "deserialized link target name : " + *this->link_target_name);
	}

	global_logger.log(inode_ops, "deserialized ino : " + uuid_to_string(this->core.i_ino));
	global_logger.log(inode_ops, "deserialized size : " + std::to_string(this->core.i_size));
}

void inode::sync()
{
	global_logger.log(inode_ops, "Called inode.sync()");
	std::vector<char> raw = this->serialize();
	meta_pool->write(obj_category::INODE, uuid_to_string(this->core.i_ino), raw.data(), REG_INODE_SIZE + this->core.link_target_len, 0);
}

void inode::permission_check(int mask){
	global_logger.log(inode_ops, "Called permission_check");
	bool check_read = (mask & R_OK) ? true : false;
	bool check_write = (mask & W_OK) ? true : false;
	bool check_exec = (mask & X_OK) ? true : false;
	bool ret = true;

	mode_t target_mode;

	if(this_client->get_client_uid() == this->core.i_uid){
		target_mode = (this->core.i_mode & S_IRWXU) >> 6;
	} else if (this_client->get_client_gid() == this->core.i_gid){
		target_mode = (this->core.i_mode & S_IRWXG) >> 3;
	} else {
		target_mode = this->core.i_mode & S_IRWXO;
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
uuid inode::get_p_ino() {
	return p_ino;
}
mode_t inode::get_mode(){
	return this->core.i_mode;
}
uid_t inode::get_uid(){
	return this->core.i_uid;
}
gid_t inode::get_gid(){
	return this->core.i_gid;
}
uuid inode::get_ino() {
	return this->core.i_ino;
}
nlink_t inode::get_nlink() {
	return this->core.i_nlink;
}
off_t inode::get_size(){
	return this->core.i_size;
}
struct timespec inode::get_atime(){
	return this->core.i_atime;
}
struct timespec inode::get_mtime(){
	return this->core.i_mtime;
}
struct timespec inode::get_ctime(){
	return this->core.i_ctime;
}

uint64_t inode::get_loc() {
	return this->loc;
}
uint32_t inode::get_link_target_len(){
	return this->core.link_target_len;
}
std::shared_ptr<std::string> inode::get_link_target_name(){
	return this->link_target_name;
}

// setter
void inode::set_mode(mode_t mode){
	this->core.i_mode = mode;
}
void inode::set_uid(uid_t uid){
	this->core.i_uid = uid;
}
void inode::set_gid(gid_t gid){
	this->core.i_gid = gid;
}
void inode::set_ino(uuid ino){
	this->core.i_ino = ino;
}
void inode::set_nlink(nlink_t nlink){
	this->core.i_nlink = nlink;
}
void inode::set_size(off_t size){
	this->core.i_size = size;
}
void inode::set_atime(struct timespec atime){
	this->core.i_atime = atime;
}
void inode::set_mtime(struct timespec mtime){
	this->core.i_mtime = mtime;
}
void inode::set_ctime(struct timespec ctime){
	this->core.i_ctime = ctime;
}

void inode::set_loc(uint64_t loc) {
	this->loc = loc;
}
void inode::set_link_target_len(uint32_t len){
	this->core.link_target_len = len;
}
void inode::set_link_target_name(const std::shared_ptr<std::string> name){
	this->link_target_name = name;
	//this->link_target_name = (char *)calloc(this->core.link_target_len + 1, sizeof(char));
	//memcpy(this->link_target_name, name, this->core.link_target_len);
}

void inode::set_p_ino(const uuid &p_ino) {
	inode::p_ino = p_ino;
}

void inode::inode_to_rename_src_response(::rpc_rename_not_same_parent_src_respond *response) {
	response->set_target_i_mode(this->core.i_mode);
	response->set_target_i_uid(this->core.i_uid);
	response->set_target_i_gid(this->core.i_gid);
	response->set_target_i_ino_prefix(ino_controller->get_prefix_from_uuid(this->core.i_ino));
	response->set_target_i_ino_postfix(ino_controller->get_postfix_from_uuid(this->core.i_ino));
	response->set_target_i_nlink(this->core.i_nlink);
	response->set_target_i_size(this->core.i_size);
	response->set_target_a_sec(this->core.i_atime.tv_sec);
	response->set_target_a_nsec(this->core.i_atime.tv_nsec);
	response->set_target_m_sec(this->core.i_mtime.tv_sec);
	response->set_target_m_nsec(this->core.i_mtime.tv_nsec);
	response->set_target_c_sec(this->core.i_ctime.tv_sec);
	response->set_target_c_nsec(this->core.i_ctime.tv_nsec);
	response->set_target_i_link_target_len(this->core.link_target_len);
	if(S_ISLNK(this->core.i_mode)) {
		response->set_target_i_link_target_name(this->link_target_name->data());
	}
}

void inode::rename_src_response_to_inode(::rpc_rename_not_same_parent_src_respond &response) {
	this->core.i_mode = response.target_i_mode();
	this->core.i_uid = response.target_i_uid();
	this->core.i_gid = response.target_i_gid();
	this->core.i_ino = ino_controller->splice_prefix_and_postfix(response.target_i_ino_prefix(), response.target_i_ino_postfix());
	this->core.i_nlink = response.target_i_nlink();
	this->core.i_size = response.target_i_size();
	this->core.i_atime.tv_sec = response.target_a_sec();
	this->core.i_atime.tv_nsec = response.target_a_nsec();
	this->core.i_mtime.tv_sec = response.target_m_sec();
	this->core.i_mtime.tv_nsec = response.target_m_nsec();
	this->core.i_ctime.tv_sec = response.target_c_sec();
	this->core.i_ctime.tv_nsec = response.target_c_nsec();
	this->core.link_target_len = response.target_i_link_target_len();
	if(S_ISLNK(response.target_i_mode())) {
		this->link_target_name = std::make_shared<std::string>(response.target_i_link_target_name());
		//this->link_target_name = reinterpret_cast<char *>(calloc(response.target_i_link_target_len() + 1 , sizeof(char)));
		//memcpy(this->link_target_name, response.target_i_link_target_name().data(), response.target_i_link_target_len());
	}
}

void inode::inode_to_rename_dst_request(::rpc_rename_not_same_parent_dst_request &request) {
	request.set_target_i_mode(this->core.i_mode);
	request.set_target_i_uid(this->core.i_uid);
	request.set_target_i_gid(this->core.i_gid);
	request.set_target_i_ino_prefix(ino_controller->get_prefix_from_uuid(this->core.i_ino));
	request.set_target_i_ino_postfix(ino_controller->get_postfix_from_uuid(this->core.i_ino));
	request.set_target_i_nlink(this->core.i_nlink);
	request.set_target_i_size(this->core.i_size);
	request.set_target_a_sec(this->core.i_atime.tv_sec);
	request.set_target_a_nsec(this->core.i_atime.tv_nsec);
	request.set_target_m_sec(this->core.i_mtime.tv_sec);
	request.set_target_m_nsec(this->core.i_mtime.tv_nsec);
	request.set_target_c_sec(this->core.i_ctime.tv_sec);
	request.set_target_c_nsec(this->core.i_ctime.tv_nsec);
	request.set_target_i_link_target_len(this->core.link_target_len);
	if(S_ISLNK(this->core.i_mode)) {
		request.set_target_i_link_target_name(this->link_target_name->data());
	}
}

void inode::rename_dst_request_to_inode(const ::rpc_rename_not_same_parent_dst_request *request) {
	this->core.i_mode = request->target_i_mode();
	this->core.i_uid = request->target_i_uid();
	this->core.i_gid = request->target_i_gid();
	this->core.i_ino = ino_controller->splice_prefix_and_postfix(request->target_i_ino_prefix(), request->target_i_ino_postfix());
	this->core.i_nlink = request->target_i_nlink();
	this->core.i_size = request->target_i_size();
	this->core.i_atime.tv_sec = request->target_a_sec();
	this->core.i_atime.tv_nsec = request->target_a_nsec();
	this->core.i_mtime.tv_sec = request->target_m_sec();
	this->core.i_mtime.tv_nsec = request->target_m_nsec();
	this->core.i_ctime.tv_sec = request->target_c_sec();
	this->core.i_ctime.tv_nsec = request->target_c_nsec();
	this->core.link_target_len = request->target_i_link_target_len();
	if(S_ISLNK(request->target_i_mode())) {
		this->link_target_name = std::make_shared<std::string>(request->target_i_link_target_name());
		//this->link_target_name = reinterpret_cast<char *>(calloc(request->target_i_link_target_len() + 1 , sizeof(char)));
		//memcpy(this->link_target_name, request->target_i_link_target_name().data(), request->target_i_link_target_len());
	}
}

uuid alloc_new_ino() {
	global_logger.log(inode_ops, "Called alloc_new_ino()");
	uuid new_ino = ino_controller->alloc_new_uuid();

	global_logger.log(inode_ops, "new inode number : " + uuid_to_string(new_ino));
	return new_ino;
}
