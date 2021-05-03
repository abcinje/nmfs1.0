#include "rpc_client.hpp"

extern std::map<ino_t, unique_ptr<file_handler>> fh_list;
extern std::mutex file_handler_mutex;

rpc_client::rpc_client(std::shared_ptr<Channel> channel) : stub_(remote_ops::NewStub(channel)){}

void rpc_client::getattr(shared_ptr<remote_inode> i, struct stat* stat) {
	global_logger.log(rpc_client_ops, "Called getattr()");
	ClientContext context;
	rpc_common_request Input;
	rpc_getattr_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());

	Status status = stub_->rpc_getattr(&context, Input, &Output);
	if(status.ok()){
		/* fill stat */
		return;
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::getattr() failed");
	}
}

void rpc_client::access(shared_ptr<remote_inode> i, int mask) {
	global_logger.log(rpc_client_ops, "Called access()");
	ClientContext context;
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());
	Input.set_i_mode(mask);

	Status status = stub_->rpc_access(&context, Input, &Output);
	if(status.ok()){

	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::access() failed");
	}
}

int rpc_client::opendir(shared_ptr<remote_inode> i, struct fuse_file_info* file_info) {
	global_logger.log(rpc_client_ops, "Called opendir()");
	ClientContext context;
	rpc_common_request Input;
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
	rpc_common_request Input;
	rpc_name_respond Output;
	std::unique_ptr<ClientReader<rpc_name_respond>> reader(stub_->rpc_readdir(&context, Input));

	while(reader->Read(&Output)){
		/* TODO : manage stream ouput */
		filler(buffer, Output.filename().c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
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
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */

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
	rpc_common_request Input;
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
	rpc_common_request Input;
	rpc_name_respond Output;

	/* prepare Input */
	Input.set_dentry_table_ino(i->get_dentry_table_ino());
	Input.set_filename(i->get_file_name());
	Input.set_i_size(size);

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
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */

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
	rpc_common_request Input;
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
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */

	Status status = stub_->rpc_create(&context, Input, &Output);
	if(status.ok()){

		return;
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::create() failed");
	}
}

void rpc_client::unlink(shared_ptr<remote_inode> parent_i, std::string child_name) {
	global_logger.log(rpc_client_ops, "Called unlink()");
	ClientContext context;
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */

	Status status = stub_->rpc_unlink(&context, Input, &Output);
	if(status.ok()){
		return;
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::unlink() failed");
	}
}

size_t rpc_client::read(shared_ptr<remote_inode> i, char* buffer, size_t size, off_t offset) {
	global_logger.log(rpc_client_ops, "Called read()");
	ClientContext context;
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */

	Status status = stub_->rpc_read(&context, Input, &Output);
	if(status.ok()){
		return Output.ret();
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::read() failed");
	}
}

size_t rpc_client::write(shared_ptr<remote_inode>i, const char* buffer, size_t size, off_t offset, int flags) {
	global_logger.log(rpc_client_ops, "Called write()");
	ClientContext context;
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */

	Status status = stub_->rpc_write(&context, Input, &Output);
	if(status.ok()){
		return Output.ret();
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::write() failed");
	}
}

void rpc_client::chmod(shared_ptr<remote_inode> i, mode_t mode) {
	global_logger.log(rpc_client_ops, "Called chmod()");
	ClientContext context;
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */

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
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */

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
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */

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
	rpc_common_request Input;
	rpc_common_respond Output;

	/* prepare Input */

	Status status = stub_->rpc_truncate(&context, Input, &Output);
	if(status.ok()){
		return Output.ret();
	} else {
		global_logger.log(rpc_client_ops, status.error_message());
		throw std::runtime_error("rpc_client::truncate() failed");
	}
}