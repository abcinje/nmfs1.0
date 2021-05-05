#include "rpc_client.hpp"
extern rados_io *data_pool;

extern std::map<ino_t, unique_ptr<file_handler>> fh_list;
extern std::mutex file_handler_mutex;

rpc_client::rpc_client(std::shared_ptr<Channel> channel) : stub_(remote_ops::NewStub(channel)){}
/* TODO : Handle -ENOTLEADER */
/* dentry_table operations */
ino_t rpc_client::check_child_inode(ino_t dentry_table_ino, std::string filename){
	global_logger.log(rpc_client_ops, "Called check_child_inode()");
	ClientContext context;
	rpc_dentry_table_request Input;
	rpc_dentry_table_respond Output;

	Input.set_dentry_table_ino(dentry_table_ino);
	Input.set_filename(filename);

	Status status = stub_->rpc_check_child_inode(&context, Input, &Output);
	if(status.ok()){
		return Output.checked_ino();
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::check_child_inode() failed");
	}
}

/* inode operations */
mode_t rpc_client::get_mode(ino_t dentry_table_ino, std::string filename){
	global_logger.log(rpc_client_ops, "Called get_mode()");
	ClientContext context;
	rpc_inode_request Input;
	rpc_inode_respond Output;

	Input.set_dentry_table_ino(dentry_table_ino);
	Input.set_filename(filename);

	Status status = stub_->rpc_get_mode(&context, Input, &Output);
	if(status.ok()){
		return Output.i_mode();
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::get_mode() failed");
	}
}

/* file system operations */
void rpc_client::getattr(shared_ptr<remote_inode> i, struct stat* s) {
	global_logger.log(rpc_client_ops, "Called getattr()");
	ClientContext context;
	rpc_common_request Input;
	rpc_getattr_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());

	Status status = stub_->rpc_getattr(&context, Input, &Output);
	if(status.ok()){
		s->st_mode	= Output.i_mode();
		s->st_uid	= Output.i_uid();
		s->st_gid	= Output.i_gid();
		s->st_ino	= Output.i_ino();
		s->st_nlink	= Output.i_nlink();
		s->st_size	= Output.i_size();

		s->st_atim.tv_sec	= Output.a_sec();
		s->st_atim.tv_nsec	= Output.a_nsec();
		s->st_mtim.tv_sec	= Output.m_sec();
		s->st_mtim.tv_nsec	= Output.m_nsec();
		s->st_ctim.tv_sec	= Output.c_sec();
		s->st_ctim.tv_sec	= Output.c_nsec();
		return;
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::getattr() failed");
	}
}

void rpc_client::access(shared_ptr<remote_inode> i, int mask) {
	global_logger.log(rpc_client_ops, "Called access()");
	ClientContext context;
	rpc_access_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());
	Input.set_mask(mask);

	Status status = stub_->rpc_access(&context, Input, &Output);
	if(status.ok()){
		if(Output.ret() == -EACCES)
			throw inode::permission_denied("Permission Denied: Remote");
		return;
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::access() failed");
	}
}

int rpc_client::opendir(shared_ptr<remote_inode> i, struct fuse_file_info* file_info) {
	global_logger.log(rpc_client_ops, "Called opendir()");
	ClientContext context;
	rpc_open_opendir_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());

	Status status = stub_->rpc_opendir(&context, Input, &Output);
	if(status.ok()){
		{
			std::scoped_lock<std::mutex> lock{file_handler_mutex};
			unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
			file_info->fh = reinterpret_cast<uint64_t>(fh.get());

			fh->set_fhno((void *) file_info->fh);
			fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));
		}
		return Output.ret();
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::opendir() failed");
	}
}

void rpc_client::readdir(shared_ptr<remote_inode> i, void* buffer, fuse_fill_dir_t filler) {
	global_logger.log(rpc_client_ops, "Called readdir()");
	ClientContext context;
	rpc_readdir_request Input;
	rpc_name_respond Output;
	std::unique_ptr<ClientReader<rpc_name_respond>> reader(stub_->rpc_readdir(&context, Input));

	while(reader->Read(&Output)){
		if(Output.ret() == 0)
			filler(buffer, Output.filename().c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
		else
			break;
	}

	Status status = reader->Finish();
	if(status.ok()){
		return;
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::readdir() failed");
	}
}

void rpc_client::mkdir(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode) {
	global_logger.log(rpc_client_ops, "Called mkdir()");
	ClientContext context;
	rpc_mkdir_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(parent_i->get_dentry_table_ino());
	Input.set_new_dir_name(new_child_name);
	Input.set_new_mode(mode);

	Status status = stub_->rpc_mkdir(&context, Input, &Output);
	if(status.ok()){

	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::mkdir() failed");
	}
}

int rpc_client::rmdir(shared_ptr<remote_inode> parent_i, shared_ptr<inode> target_i, std::string target_name) {
	return -ENOSYS;
}

int rpc_client::symlink(shared_ptr<remote_inode> dst_parent_i, const char *src, const char *dst) {
	global_logger.log(rpc_client_ops, "Called symlink()");
	ClientContext context;
	rpc_symlink_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(dst_parent_i->get_dentry_table_ino());
	Input.set_filename(dst_parent_i->get_file_name());
	Input.set_src(src);
	Input.set_dst(dst);

	Status status = stub_->rpc_symlink(&context, Input, &Output);
	if(status.ok()){
		return Output.ret();
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::symlink() failed");
	}
}

int rpc_client::readlink(shared_ptr<remote_inode> i, char *buf, size_t size) {
	global_logger.log(rpc_client_ops, "Called readlink()");
	ClientContext context;
	rpc_readlink_request Input;
	rpc_name_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());
	Input.set_size(size);

	Status status = stub_->rpc_readlink(&context, Input, &Output);
	if(status.ok()){
		/* fill buffer */
		size_t len = MIN(Output.filename().length(), size-1);
		memcpy(buf, Output.filename().c_str(), len);
		buf[len] = '\0';
		return Output.ret();
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::readlink() failed");
	}
}

int rpc_client::rename_same_parent(shared_ptr<remote_inode> parent_i, const char* old_path, const char* new_path, unsigned int flags) {
	global_logger.log(rpc_client_ops, "Called access()");
	ClientContext context;
	rpc_rename_same_parent_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(parent_i->get_dentry_table_ino());
	Input.set_old_path(old_path);
	Input.set_new_path(new_path);
	Input.set_flags(flags);

	Status status = stub_->rpc_rename_same_parent(&context, Input, &Output);
	if(status.ok()){
		return Output.ret();
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::readlink() failed");
	}
}

int rpc_client::rename_not_same_parent(shared_ptr<remote_inode> src_parent_i, shared_ptr<inode> dst_parent_i, const char* old_path, const char* new_path, unsigned int flags) {
	return -ENOSYS;
}

int rpc_client::open(shared_ptr<remote_inode> i, struct fuse_file_info* file_info) {
	global_logger.log(rpc_client_ops, "Called open()");
	ClientContext context;
	rpc_open_opendir_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());
	Input.set_flags(file_info->flags);

	Status status = stub_->rpc_open(&context, Input, &Output);
	if(status.ok()){
		{
			std::scoped_lock<std::mutex> lock{file_handler_mutex};
			unique_ptr<file_handler> fh = std::make_unique<file_handler>(i->get_ino());
			file_info->fh = reinterpret_cast<uint64_t>(fh.get());

			fh->set_fhno((void *) file_info->fh);
			fh_list.insert(std::make_pair(i->get_ino(), std::move(fh)));
		}
		return Output.ret();
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::open() failed");
	}
}

void rpc_client::create(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info* file_info) {
	global_logger.log(rpc_client_ops, "Called create()");
	ClientContext context;
	rpc_create_request Input;
	rpc_create_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(parent_i->get_dentry_table_ino());
	Input.set_new_file_name(new_child_name);
	Input.set_new_mode(mode);

	Status status = stub_->rpc_create(&context, Input, &Output);
	if(status.ok()){
		{
			std::scoped_lock<std::mutex> lock{file_handler_mutex};
			unique_ptr<file_handler> fh = std::make_unique<file_handler>(Output.new_ino());
			file_info->fh = reinterpret_cast<uint64_t>(fh.get());

			fh->set_fhno((void *) file_info->fh);
			fh_list.insert(std::make_pair(Output.new_ino(), std::move(fh)));
		}
		return;
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::create() failed");
	}
}

void rpc_client::unlink(shared_ptr<remote_inode> parent_i, std::string child_name) {
	global_logger.log(rpc_client_ops, "Called unlink()");
	ClientContext context;
	rpc_unlink_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(parent_i->get_dentry_table_ino());
	Input.set_filename(child_name);

	Status status = stub_->rpc_unlink(&context, Input, &Output);
	if(status.ok()){
		return;
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::unlink() failed");
	}
}


size_t rpc_client::write(shared_ptr<remote_inode> i, const char* buffer, size_t size, off_t offset, int flags) {
	global_logger.log(rpc_client_ops, "Called write()");
	ClientContext context;
	rpc_write_request Input;
	rpc_write_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());
	Input.set_offset(offset);
	Input.set_size(size);
	Input.set_flags(flags);

	Status status = stub_->rpc_write(&context, Input, &Output);
	if(status.ok()){
		size_t written_len = data_pool->write(DATA, std::to_string(i->get_ino()), buffer, Output.size(), Output.offset());
		return written_len;
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::write() failed");
	}
}

void rpc_client::chmod(shared_ptr<remote_inode> i, mode_t mode) {
	global_logger.log(rpc_client_ops, "Called chmod()");
	ClientContext context;
	rpc_chmod_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());
	Input.set_mode(mode);

	Status status = stub_->rpc_chmod(&context, Input, &Output);
	if(status.ok()){

	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::chmod() failed");
	}
}

void rpc_client::chown(shared_ptr<remote_inode> i, uid_t uid, gid_t gid) {
	global_logger.log(rpc_client_ops, "Called chown()");
	ClientContext context;
	rpc_chown_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());
	Input.set_uid(uid);
	Input.set_gid(gid);

	Status status = stub_->rpc_chown(&context, Input, &Output);
	if(status.ok()){

	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::chown() failed");
	}
}

void rpc_client::utimens(shared_ptr<remote_inode> i, const struct timespec tv[2]) {
	global_logger.log(rpc_client_ops, "Called utimens()");
	ClientContext context;
	rpc_utimens_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());
	Input.set_a_sec(tv[0].tv_sec);
	Input.set_a_nsec(tv[0].tv_nsec);
	Input.set_m_sec(tv[1].tv_sec);
	Input.set_m_nsec(tv[1].tv_nsec);

	Status status = stub_->rpc_utimens(&context, Input, &Output);
	if(status.ok()){

	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::utimens() failed");
	}
}

int rpc_client::truncate(shared_ptr<remote_inode> i, off_t offset) {
	global_logger.log(rpc_client_ops, "Called truncate()");
	ClientContext context;
	rpc_truncate_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());
	Input.set_offset(offset);

	Status status = stub_->rpc_truncate(&context, Input, &Output);
	if(status.ok()){
		int ret = data_pool->truncate(DATA, std::to_string(i->get_ino()), offset);
		return ret;
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::truncate() failed");
	}
}