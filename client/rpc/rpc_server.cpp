#include "rpc_server.hpp"
/* TODO : thread cannot read fuse_ctx */
extern rados_io *meta_pool;
extern rados_io *data_pool;
extern directory_table *indexing_table;

extern std::map<ino_t, unique_ptr<file_handler>> fh_list;
extern std::mutex file_handler_mutex;
extern std::mutex atomic_mutex;

extern std::unique_ptr<Server> remote_handle;
extern fuse_context *fuse_ctx;

void run_rpc_server(const std::string& remote_address){
	rpc_server rpc_service;
	ServerBuilder builder;
	builder.AddListeningPort(remote_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&rpc_service);
	remote_handle = builder.BuildAndStart();

	remote_handle->Wait();
}

Status rpc_server::rpc_check_child_inode(::grpc::ServerContext *context, const ::rpc_dentry_table_request *request,
										 ::rpc_dentry_table_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_check_child_inode()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	ino_t check_target_ino = parent_dentry_table->check_child_inode(request->filename());

	/* TODO : ino_t cannot be -1 */
	response->set_checked_ino(check_target_ino);
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_get_mode(::grpc::ServerContext *context, const ::rpc_inode_request *request,
								::rpc_inode_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_get_mode()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	response->set_i_mode(i->get_mode());
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_getattr(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::rpc_getattr_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_getattr(" + request->filename() + ")");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	try {
		std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(
			request->dentry_table_ino());
		i = parent_dentry_table->get_child_inode(request->filename());
	} catch (inode::no_entry &e){
		response->set_ret(-EACCES);
		return Status::OK;
	}

	response->set_i_mode(i->get_mode());
	response->set_i_uid(i->get_uid());
	response->set_i_gid(i->get_gid());
	response->set_i_ino(i->get_ino());
	response->set_i_nlink(i->get_nlink());
	response->set_i_size(i->get_size());

	response->set_a_sec(i->get_atime().tv_sec);
	response->set_a_nsec(i->get_atime().tv_nsec);
	response->set_m_sec(i->get_mtime().tv_sec);
	response->set_m_nsec(i->get_mtime().tv_nsec);
	response->set_c_sec(i->get_ctime().tv_sec);
	response->set_c_nsec(i->get_ctime().tv_nsec);

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_access(::grpc::ServerContext *context, const ::rpc_access_request *request,
							  ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_access(" + request->filename() + ")");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	try{
		i->permission_check(request->mask());
	} catch(inode::permission_denied &e) {
		response->set_ret(-EACCES);
		return Status::OK;
	}

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_opendir(::grpc::ServerContext *context, const ::rpc_open_opendir_request *request,
							   ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_opendir(" + request->filename() + ")");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	if(!S_ISDIR(i->get_mode())) {
		response->set_ret(-ENOTDIR);
		return Status::OK;
	}

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_readdir(::grpc::ServerContext *context, const ::rpc_readdir_request *request,
							   ::grpc::ServerWriter<::rpc_name_respond> *writer) {
	global_logger.log(rpc_server_ops, "Called rpc_readdir()");
	rpc_name_respond response;
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response.set_ret(-ENOTLEADER);
		writer->Write(response);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::recursive_mutex& dentry_table_mutex = parent_dentry_table->get_dentry_table_mutex();

	std::scoped_lock scl{dentry_table_mutex};
	std::map<std::string, shared_ptr<inode>>::iterator it;
	response.set_filename(".");
	writer->Write(response);
	response.set_filename("..");
	writer->Write(response);
	for(it = parent_dentry_table->get_child_inode_begin(); it != parent_dentry_table->get_child_inode_end(); it++){
		response.set_filename(it->first);
		writer->Write(response);
	}

	return Status::OK;
}

Status rpc_server::rpc_mkdir(::grpc::ServerContext *context, const ::rpc_mkdir_request *request,
							 ::rpc_mkdir_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_mkdir(" + request->new_dir_name() + ")");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::scoped_lock scl{atomic_mutex};
	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	shared_ptr<inode> i = std::make_shared<inode>(0, 0, request->new_mode() | S_IFDIR);
	parent_dentry_table->create_child_inode(request->new_dir_name(), i);

	i->set_size(DIR_INODE_SIZE);
	i->sync();

	shared_ptr<dentry> new_d = std::make_shared<dentry>(i->get_ino(), true);
	new_d->sync();

	response->set_new_dir_ino(i->get_ino());
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_rmdir_top(::grpc::ServerContext *context, const ::rpc_rmdir_request *request,
							 ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_rmdir_top()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::scoped_lock scl{atomic_mutex};
	std::shared_ptr<dentry_table> target_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	if(target_dentry_table == nullptr) {
		throw std::runtime_error("directory table is corrupted : Can't find leased directory");
	}

	if(target_dentry_table->get_child_num() > 0) {
		response->set_ret(-ENOTEMPTY);
		return Status::OK;
	}

	meta_pool->remove(obj_category::DENTRY, std::to_string(request->target_ino()));
	indexing_table->delete_dentry_table(request->target_ino());

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_rmdir_down(::grpc::ServerContext *context, const ::rpc_rmdir_request *request,
			     ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_rmdir_down()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::scoped_lock scl{atomic_mutex};
	int ret = 0;
	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	parent_dentry_table->delete_child_inode(request->target_name());

	meta_pool->remove(obj_category::INODE, std::to_string(request->target_ino()));

	/* It may be failed if parent and child dir is located in same leader */
	ret = indexing_table->delete_dentry_table(request->target_ino());
	if(ret == -1) {
		global_logger.log(rpc_server_ops, "this dentry table already removed in rpc_rmdir_top()");
	}

	response->set_ret(0);
	return Status::OK;
}


Status rpc_server::rpc_symlink(::grpc::ServerContext *context, const ::rpc_symlink_request *request,
							   ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_symlink()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::scoped_lock scl{atomic_mutex};
	shared_ptr<dentry_table> dst_parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());

	std::unique_ptr<std::string> symlink_name = get_filename_from_path(request->dst());

	if (dst_parent_dentry_table->check_child_inode(symlink_name->data()) != -1){
		response->set_ret(-EEXIST);
		return Status::OK;
	}

	shared_ptr<inode> symlink_i = std::make_shared<inode>(0, 0, S_IFLNK | 0777, request->src().c_str());

	symlink_i->set_size(request->src().length());

	dst_parent_dentry_table->create_child_inode(symlink_name->data(), symlink_i);
	symlink_i->sync();

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_readlink(::grpc::ServerContext *context, const ::rpc_readlink_request *request,
								::rpc_name_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_readlink()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}
	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	if (!S_ISLNK(i->get_mode())) {
		response->set_ret(-EINVAL);
		return Status::OK;
	}

	response->set_filename(i->get_link_target_name());
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_rename_same_parent(::grpc::ServerContext *context, const ::rpc_rename_same_parent_request *request,
										  ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_rename_same_parent()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::scoped_lock scl{atomic_mutex};
	unique_ptr<std::string> old_name = get_filename_from_path(request->old_path());
	unique_ptr<std::string> new_name = get_filename_from_path(request->new_path());

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());

	shared_ptr<inode> target_i = parent_dentry_table->get_child_inode(*old_name);
	ino_t check_dst_ino = parent_dentry_table->check_child_inode(*new_name);

	if (request->flags() == 0) {
		if(check_dst_ino != -1) {
			parent_dentry_table->delete_child_inode(*new_name);
			meta_pool->remove(obj_category::INODE, std::to_string(check_dst_ino));
		}
		parent_dentry_table->delete_child_inode(*old_name);
		parent_dentry_table->create_child_inode(*new_name, target_i);

	} else {
		response->set_ret(-ENOSYS);
		return Status::OK;
	}

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_rename_not_same_parent_src(::grpc::ServerContext *context,
						  const ::rpc_rename_not_same_parent_src_request *request,
						  ::rpc_rename_not_same_parent_src_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_rename_not_same_parent_src()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::scoped_lock scl{atomic_mutex};
	unique_ptr<std::string> old_name = get_filename_from_path(request->old_path());

	std::shared_ptr<dentry_table> src_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	shared_ptr<inode> target_i = src_dentry_table->get_child_inode(*old_name);
	ino_t target_ino = target_i->get_ino();

	if (request->flags() == 0) {
		src_dentry_table->delete_child_inode(*old_name);
	} else {
		response->set_ret(-ENOSYS);
		return Status::OK;
	}

	response->set_target_ino(target_ino);
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_rename_not_same_parent_dst(::grpc::ServerContext *context,
						  const ::rpc_rename_not_same_parent_dst_request *request,
						  ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_rename_not_same_parent_dst()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::scoped_lock scl{atomic_mutex};
	unique_ptr<std::string> new_name = get_filename_from_path(request->new_path());

	std::shared_ptr<dentry_table> dst_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	/* TODO : need other method to use cache */
	shared_ptr<inode> target_i = std::make_shared<inode>(request->target_ino());

	if (request->flags() == 0) {
		if(request->check_dst_ino() != -1) {
			dst_dentry_table->delete_child_inode(*new_name);
			meta_pool->remove(obj_category::INODE, std::to_string(request->check_dst_ino()));
		}
		dst_dentry_table->create_child_inode(*new_name, target_i);
	} else {
		response->set_ret(-ENOSYS);
		return Status::OK;
	}

	response->set_ret(0);
	return Status::OK;
}


Status rpc_server::rpc_open(::grpc::ServerContext *context, const ::rpc_open_opendir_request *request,
							::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_open()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	if ((request->flags() & O_DIRECTORY) && !S_ISDIR(i->get_mode())) {
		response->set_ret(-ENOTDIR);
		return Status::OK;
	}

	if ((request->flags() & O_NOFOLLOW) && S_ISLNK(i->get_mode())) {
		response->set_ret(-ELOOP);
		return Status::OK;
	}

	if ((request->flags() & O_TRUNC) && !(request->flags() & O_PATH)) {
		i->set_size(0);
		i->sync();
	}

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_create(::grpc::ServerContext *context, const ::rpc_create_request *request,
							  ::rpc_create_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_create(" + request->new_file_name() + ")");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::scoped_lock scl{atomic_mutex};
	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	shared_ptr<inode> i = std::make_shared<inode>(0, 0, request->new_mode() | S_IFREG);

	i->sync();

	parent_dentry_table->create_child_inode(request->new_file_name(), i);

	response->set_new_ino(i->get_ino());
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_unlink(::grpc::ServerContext *context, const ::rpc_unlink_request *request,
							  ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_unlink(" + request->filename() + ")");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::scoped_lock scl{atomic_mutex};
	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> target_i = parent_dentry_table->get_child_inode(request->filename());

	nlink_t nlink = target_i->get_nlink() - 1;
	if (nlink == 0) {
		/* data */
		data_pool->remove(obj_category::DATA, std::to_string(target_i->get_ino()));

		/* inode */
		meta_pool->remove(obj_category::INODE, std::to_string(target_i->get_ino()));

		/* parent dentry */
		parent_dentry_table->delete_child_inode(request->filename());
	} else {
		target_i->set_nlink(nlink);
		target_i->sync();
	}

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_write(::grpc::ServerContext *context, const ::rpc_write_request *request,
							 ::rpc_write_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_write()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::scoped_lock scl{atomic_mutex};
	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	off_t offset = request->offset();
	size_t size = request->size();

	if(request->flags() & O_APPEND) {
		offset = i->get_size();
	}

	if (i->get_size() < offset + size) {
		i->set_size(offset + size);
		i->sync();
	}

	response->set_offset(offset);
	response->set_size(size);
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_chmod(::grpc::ServerContext *context, const ::rpc_chmod_request *request,
							 ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_chmod(" + request->filename() + ")");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	mode_t type = i->get_mode() & S_IFMT;

	i->set_mode(request->mode() | type);
	i->sync();

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_chown(::grpc::ServerContext *context, const ::rpc_chown_request *request,
							 ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_chown(" + request->filename() + ")");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	if (((int32_t) request->uid()) >= 0)
		i->set_uid(request->uid());

	if (((int32_t) request->gid()) >= 0)
		i->set_gid(request->gid());

	i->sync();

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_utimens(::grpc::ServerContext *context, const ::rpc_utimens_request *request,
							   ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_utimens(" + request->filename() + ")");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	if (request->a_nsec() == UTIME_NOW) {
		struct timespec ts;
		if (!timespec_get(&ts, TIME_UTC))
			runtime_error("timespec_get() failed");
		i->set_atime(ts);
	} else if (request->a_nsec() == UTIME_OMIT) { ;
	} else {
		struct timespec tv;
		tv.tv_sec = request->a_sec();
		tv.tv_nsec = request->a_nsec();
		i->set_atime(tv);
	}

	if (request->m_nsec() == UTIME_NOW) {
		struct timespec ts;
		if (!timespec_get(&ts, TIME_UTC))
			runtime_error("timespec_get() failed");
		i->set_mtime(ts);
	} else if (request->m_nsec()  == UTIME_OMIT) { ;
	} else {
		struct timespec tv;
		tv.tv_sec = request->m_sec();
		tv.tv_nsec = request->m_nsec();
		i->set_mtime(tv);
	}

	i->sync();

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_truncate(::grpc::ServerContext *context, const ::rpc_truncate_request *request,
								::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_truncate()");
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	i->set_size(request->offset());
	i->sync();

	response->set_ret(0);
	return Status::OK;
}
