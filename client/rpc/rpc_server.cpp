#include "rpc_server.hpp"

extern rados_io *meta_pool;
extern rados_io *data_pool;
extern directory_table *indexing_table;

extern std::map<ino_t, unique_ptr<file_handler>> fh_list;
extern std::mutex file_handler_mutex;

Status rpc_server::rpc_getattr(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::rpc_getattr_respond *response) {

	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

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

Status rpc_server::rpc_access(::grpc::ServerContext *context, const ::rpc_common_request *request,
							  ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());

	try{
		i->permission_check(request->i_mode());
	} catch(inode::permission_denied &e) {
		response->set_ret(-EACCES);
		return Status::OK;
	}

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_opendir(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::rpc_common_respond *response) {
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

Status rpc_server::rpc_readdir(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::grpc::ServerWriter<::rpc_name_respond> *writer) {

	return Service::rpc_readdir(context, request, writer);
}

Status rpc_server::rpc_mkdir(::grpc::ServerContext *context, const ::rpc_common_request *request,
							 ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_mkdir(context, request, response);
}

Status rpc_server::rpc_rmdir(::grpc::ServerContext *context, const ::rpc_common_request *request,
							 ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_rmdir(context, request, response);
}

Status rpc_server::rpc_symlink(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	fuse_context *fuse_ctx = fuse_get_context();
	shared_ptr<dentry_table> dst_parent_dentry_table = indexing_table->get_dentry_table(request->dentry_table_ino());

	std::unique_ptr<std::string> symlink_name = get_filename_from_path(request->dst());

	if (dst_parent_dentry_table->check_child_inode(symlink_name->data()) != -1){
		response->set_ret(-EEXIST);
		return Status::OK;
	}

	shared_ptr<inode> symlink_i = std::make_shared<inode>(fuse_ctx->uid, fuse_ctx->gid, S_IFLNK | 0777, request->src().length(), request->src().c_str());

	symlink_i->set_size(request->src().length());

	dst_parent_dentry_table->create_child_inode(symlink_name->data(), symlink_i);
	symlink_i->sync();

	/* TODO : should we change directory inode's size?
	dst_parent_i->set_size(dst_parent_dentry_table->get_total_name_legth());
	dst_parent_i->sync();
	*/

	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_readlink(::grpc::ServerContext *context, const ::rpc_common_request *request,
								::rpc_name_respond *response) {
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

Status rpc_server::rpc_rename_same_parent(::grpc::ServerContext *context, const ::rpc_common_request *request,
										  ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_rename_same_parent(context, request, response);
}

Status rpc_server::rpc_rename_not_same_parent(::grpc::ServerContext *context, const ::rpc_common_request *request,
											  ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_rename_not_same_parent(context, request, response);
}

Status rpc_server::rpc_open(::grpc::ServerContext *context, const ::rpc_common_request *request,
							::rpc_common_respond *response) {
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

Status rpc_server::rpc_create(::grpc::ServerContext *context, const ::rpc_common_request *request,
							  ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_create(context, request, response);
}

Status rpc_server::rpc_unlink(::grpc::ServerContext *context, const ::rpc_common_request *request,
							  ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_unlink(context, request, response);
}

Status rpc_server::rpc_read(::grpc::ServerContext *context, const ::rpc_common_request *request,
							::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_read(context, request, response);
}

Status rpc_server::rpc_write(::grpc::ServerContext *context, const ::rpc_common_request *request,
							 ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_write(context, request, response);
}

Status rpc_server::rpc_chmod(::grpc::ServerContext *context, const ::rpc_common_request *request,
							 ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_chmod(context, request, response);
}

Status rpc_server::rpc_chown(::grpc::ServerContext *context, const ::rpc_common_request *request,
							 ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_chown(context, request, response);
}

Status rpc_server::rpc_utimens(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_utimens(context, request, response);
}

Status rpc_server::rpc_truncate(::grpc::ServerContext *context, const ::rpc_common_request *request,
								::rpc_common_respond *response) {
	if (indexing_table->check_dentry_table(request->dentry_table_ino()) != LOCAL) {
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	return Service::rpc_truncate(context, request, response);
}
