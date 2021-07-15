#include "rpc_server.hpp"

/* TODO : thread cannot read fuse_ctx, so only work with root uid and gid*/
extern std::shared_ptr<rados_io> meta_pool;
extern std::shared_ptr<rados_io> data_pool;
extern std::unique_ptr<directory_table> indexing_table;
extern std::unique_ptr<uuid_controller> ino_controller;

extern std::unique_ptr<Server> remote_handle;
extern std::unique_ptr<client> this_client;

extern std::unique_ptr<journal> journalctl;
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
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	uuid check_target_ino{};
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		check_target_ino = parent_dentry_table->check_child_inode(request->filename());
	}

	response->set_checked_ino_prefix(ino_controller->get_prefix_from_uuid(check_target_ino));
	response->set_checked_ino_postfix(ino_controller->get_postfix_from_uuid(check_target_ino));
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_get_mode(::grpc::ServerContext *context, const ::rpc_inode_request *request,
								::rpc_inode_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_get_mode()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		i = parent_dentry_table->get_child_inode(request->filename());
	}

	{
		std::scoped_lock scl{i->inode_mutex};
		response->set_i_mode(i->get_mode());
	}
	response->set_ret(0);
	return Status::OK;
}


Status rpc_server::rpc_permission_check(::grpc::ServerContext *context, const ::rpc_inode_request *request,
					::rpc_inode_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_permission_check()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		if (request->target_is_parent()) {
			global_logger.log(rpc_server_ops, "target is parent");
			i = parent_dentry_table->get_this_dir_inode();
		} else {
			global_logger.log(rpc_server_ops, "target is child");
			i = parent_dentry_table->get_child_inode(request->filename());
		}
	}

	try {
		std::scoped_lock scl{i->inode_mutex};
		i->permission_check(request->mask());
	} catch (inode::permission_denied &e) {
		response->set_ret(-EACCES);
		return Status::OK;
	}

	response->set_ret(0);
	return Status::OK;
}


Status rpc_server::rpc_getattr(::grpc::ServerContext *context, const ::rpc_getattr_request *request,
							   ::rpc_getattr_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_getattr(" + request->filename() + ")");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	try {
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		if(request->target_is_parent()) {
			global_logger.log(rpc_server_ops, "target is parent");
			i = parent_dentry_table->get_this_dir_inode();
		} else {
			global_logger.log(rpc_server_ops, "target is child");
			i = parent_dentry_table->get_child_inode(request->filename());
		}
	} catch (inode::no_entry &e){
		response->set_ret(-EACCES);
		return Status::OK;
	}

	{
		std::scoped_lock scl{i->inode_mutex};
		response->set_i_mode(i->get_mode());
		response->set_i_uid(i->get_uid());
		response->set_i_gid(i->get_gid());
		response->set_i_ino_prefix(ino_controller->get_prefix_from_uuid(i->get_ino()));
		response->set_i_ino_postfix(ino_controller->get_postfix_from_uuid(i->get_ino()));
		response->set_i_nlink(i->get_nlink());
		response->set_i_size(i->get_size());

		response->set_a_sec(i->get_atime().tv_sec);
		response->set_a_nsec(i->get_atime().tv_nsec);
		response->set_m_sec(i->get_mtime().tv_sec);
		response->set_m_nsec(i->get_mtime().tv_nsec);
		response->set_c_sec(i->get_ctime().tv_sec);
		response->set_c_nsec(i->get_ctime().tv_nsec);
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_access(::grpc::ServerContext *context, const ::rpc_access_request *request,
							  ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_access(" + request->filename() + ")");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		if (request->target_is_parent()) {
			global_logger.log(rpc_server_ops, "target is parent");
			i = parent_dentry_table->get_this_dir_inode();
		} else {
			global_logger.log(rpc_server_ops, "target is child");
			i = parent_dentry_table->get_child_inode(request->filename());
		}
	}

	try{
		std::scoped_lock scl{i->inode_mutex};
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
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		if (request->target_is_parent()) {
			global_logger.log(rpc_server_ops, "target is parent");
			i = parent_dentry_table->get_this_dir_inode();
		} else {
			global_logger.log(rpc_server_ops, "target is child");
			i = parent_dentry_table->get_child_inode(request->filename());
		}
	}

	{
		std::scoped_lock scl{i->inode_mutex};
		if (!S_ISDIR(i->get_mode())) {
			response->set_ret(-ENOTDIR);
			return Status::OK;
		}
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_readdir(::grpc::ServerContext *context, const ::rpc_readdir_request *request,
							   ::grpc::ServerWriter<::rpc_name_respond> *writer) {
	global_logger.log(rpc_server_ops, "Called rpc_readdir()");
	rpc_name_respond response;
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response.set_ret(-ENOTLEADER);
		return Status::OK;
	}

	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		std::map<std::string, shared_ptr<inode>>::iterator it;
		response.set_filename(".");
		writer->Write(response);
		response.set_filename("..");
		writer->Write(response);
		for (it = parent_dentry_table->get_child_inode_begin(); it != parent_dentry_table->get_child_inode_end(); it++) {
			response.set_filename(it->first);
			writer->Write(response);
		}
	}
	return Status::OK;
}

Status rpc_server::rpc_mkdir(::grpc::ServerContext *context, const ::rpc_mkdir_request *request,
							 ::rpc_mkdir_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_mkdir(" + request->new_dir_name() + ")");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		shared_ptr<inode> parent_i = parent_dentry_table->get_this_dir_inode();
		shared_ptr<inode> i = std::make_shared<inode>(dentry_table_ino, request->uid(), request->gid(), request->new_mode() | S_IFDIR);
		parent_dentry_table->create_child_inode(request->new_dir_name(), i);

		i->set_size(DIR_INODE_SIZE);

		struct timespec ts{};
		timespec_get(&ts, TIME_UTC);
		parent_i->set_mtime(ts);
		parent_i->set_ctime(ts);
		journalctl->mkdir(parent_i, request->new_dir_name(), i->get_ino());

		response->set_new_dir_ino_prefix(ino_controller->get_prefix_from_uuid(i->get_ino()));
		response->set_new_dir_ino_postfix(ino_controller->get_postfix_from_uuid(i->get_ino()));
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_rmdir_top(::grpc::ServerContext *context, const ::rpc_rmdir_request *request,
							 ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_rmdir_top()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> target_dentry_table;
	try {
		target_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	if(target_dentry_table == nullptr) {
		throw std::runtime_error("directory table is corrupted : Can't find leased directory");
	}

	uuid target_ino = ino_controller->splice_prefix_and_postfix(request->target_ino_prefix(), request->target_ino_postfix());
	{
		std::scoped_lock scl{target_dentry_table->dentry_table_mutex};
		if (target_dentry_table->get_child_num() > 0) {
			response->set_ret(-ENOTEMPTY);
			return Status::OK;
		}

		journalctl->rmself(target_ino);
		indexing_table->delete_dentry_table(target_ino);
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_rmdir_down(::grpc::ServerContext *context, const ::rpc_rmdir_request *request,
			     ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_rmdir_down()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	int ret = 0;
	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	uuid target_ino = ino_controller->splice_prefix_and_postfix(request->target_ino_prefix(), request->target_ino_postfix());
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		std::shared_ptr<inode> parent_i = parent_dentry_table->get_this_dir_inode();
		parent_dentry_table->delete_child_inode(request->target_name());

		struct timespec ts{};
		timespec_get(&ts, TIME_UTC);
		parent_i->set_mtime(ts);
		parent_i->set_ctime(ts);
		journalctl->rmdir(parent_i, request->target_name(), target_ino);

		/* It may be failed if parent and child dir is located in same leader */
		ret = indexing_table->delete_dentry_table(target_ino);
		if (ret == -1) {
			global_logger.log(rpc_server_ops, "this dentry table already removed in rpc_rmdir_top()");
		}
	}
	response->set_ret(0);
	return Status::OK;
}


Status rpc_server::rpc_symlink(::grpc::ServerContext *context, const ::rpc_symlink_request *request,
							   ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_symlink()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> dst_parent_dentry_table;
	try {
		dst_parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::unique_ptr<std::string> symlink_name = get_filename_from_path(request->dst());
	{
		std::scoped_lock scl{dst_parent_dentry_table->dentry_table_mutex};
		if (!dst_parent_dentry_table->check_child_inode(*symlink_name).is_nil()) {
			response->set_ret(-EEXIST);
			return Status::OK;
		}

		shared_ptr<inode> dst_parent_i = dst_parent_dentry_table->get_this_dir_inode();
		shared_ptr<inode> symlink_i = std::make_shared<inode>(dentry_table_ino, this_client->get_client_uid(), this_client->get_client_gid(), S_IFLNK | 0777, request->src().c_str());

		symlink_i->set_size(static_cast<off_t>(request->src().length()));

		dst_parent_dentry_table->create_child_inode(*symlink_name, symlink_i);

		struct timespec ts{};
		timespec_get(&ts, TIME_UTC);
		journalctl->mkreg(dst_parent_i, *symlink_name, symlink_i);
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_readlink(::grpc::ServerContext *context, const ::rpc_readlink_request *request,
								::rpc_name_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_readlink()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		i = parent_dentry_table->get_child_inode(request->filename());
	}

	{
		std::scoped_lock scl{i->inode_mutex};
		if (!S_ISLNK(i->get_mode())) {
			response->set_ret(-EINVAL);
			return Status::OK;
		}

		response->set_filename(i->get_link_target_name()->data());
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_rename_same_parent(::grpc::ServerContext *context, const ::rpc_rename_same_parent_request *request,
										  ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_rename_same_parent()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	unique_ptr<std::string> old_name = get_filename_from_path(request->old_path());
	unique_ptr<std::string> new_name = get_filename_from_path(request->new_path());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	shared_ptr<inode> target_i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		target_i = parent_dentry_table->get_child_inode(*old_name);
	}

	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex, target_i->inode_mutex};
		std::shared_ptr<inode> parent_i = parent_dentry_table->get_this_dir_inode();
		uuid check_dst_ino = parent_dentry_table->check_child_inode(*new_name);

		if (request->flags() == 0) {
			if (!check_dst_ino.is_nil()) {
				std::shared_ptr<inode> check_dst_inode = parent_dentry_table->get_child_inode(*new_name);
				parent_dentry_table->delete_child_inode(*new_name);
				journalctl->rmreg(parent_i, *new_name, check_dst_inode);
			}
			parent_dentry_table->delete_child_inode(*old_name);
			journalctl->rmreg(parent_i, *old_name, target_i);
			parent_dentry_table->create_child_inode(*new_name, target_i);
			journalctl->mkreg(parent_i, *new_name, target_i);

		} else {
			response->set_ret(-ENOSYS);
			return Status::OK;
		}
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_rename_not_same_parent_src(::grpc::ServerContext *context,
						  const ::rpc_rename_not_same_parent_src_request *request,
						  ::rpc_rename_not_same_parent_src_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_rename_not_same_parent_src()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	unique_ptr<std::string> old_name = get_filename_from_path(request->old_path());

	std::shared_ptr<dentry_table> src_dentry_table;
	try {
		src_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	shared_ptr<inode> target_i;
	{
		std::scoped_lock scl{src_dentry_table->dentry_table_mutex};
		target_i = src_dentry_table->get_child_inode(*old_name);
	}

	{
		std::scoped_lock scl{src_dentry_table->dentry_table_mutex, target_i->inode_mutex};
		std::shared_ptr<inode> src_parent_i = src_dentry_table->get_this_dir_inode();

		if (request->flags() == 0) {
			src_dentry_table->delete_child_inode(*old_name);
			journalctl->rmreg(src_parent_i, *old_name, target_i);
		} else {
			response->set_ret(-ENOSYS);
			return Status::OK;
		}
	}

	target_i->inode_to_rename_src_response(response);
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_rename_not_same_parent_dst(::grpc::ServerContext *context,
						  const ::rpc_rename_not_same_parent_dst_request *request,
						  ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_rename_not_same_parent_dst()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	unique_ptr<std::string> new_name = get_filename_from_path(request->new_path());
	uuid check_dst_ino = ino_controller->splice_prefix_and_postfix(request->check_dst_ino_prefix(), request->check_dst_ino_postfix());

	std::shared_ptr<dentry_table> dst_dentry_table;
	try {
		dst_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	shared_ptr<inode> target_inode = std::make_shared<inode>(LOCAL);
	target_inode->rename_dst_request_to_inode(request);
	target_inode->set_p_ino(dentry_table_ino);

	{
		std::scoped_lock scl{dst_dentry_table->dentry_table_mutex, target_inode->inode_mutex};
		std::shared_ptr<inode> dst_parent_i = dst_dentry_table->get_this_dir_inode();
		if (request->flags() == 0) {
			if (!check_dst_ino.is_nil()) {
				std::shared_ptr<inode> check_dst_inode = dst_dentry_table->get_child_inode(*new_name);
				dst_dentry_table->delete_child_inode(*new_name);
				journalctl->rmreg(dst_parent_i, *new_name, check_dst_inode);
			}
			dst_dentry_table->create_child_inode(*new_name, target_inode);
			journalctl->mkreg(dst_parent_i, *new_name, target_inode);
		} else {
			response->set_ret(-ENOSYS);
			return Status::OK;
		}
	}
	response->set_ret(0);
	return Status::OK;
}


Status rpc_server::rpc_open(::grpc::ServerContext *context, const ::rpc_open_opendir_request *request,
							::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_open()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		i = parent_dentry_table->get_child_inode(request->filename());
	}

	{
		std::scoped_lock scl{i->inode_mutex};
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
			journalctl->chreg(i->get_p_ino(), i);
		}
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_create(::grpc::ServerContext *context, const ::rpc_create_request *request,
							  ::rpc_create_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_create(" + request->new_file_name() + ")");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		shared_ptr<inode> parent_i = parent_dentry_table->get_this_dir_inode();
		shared_ptr<inode> i = std::make_shared<inode>(dentry_table_ino, this_client->get_client_uid(), this_client->get_client_gid(), request->new_mode() | S_IFREG);
		parent_dentry_table->create_child_inode(request->new_file_name(), i);

		struct timespec ts{};
		timespec_get(&ts, TIME_UTC);
		parent_i->set_mtime(ts);
		parent_i->set_ctime(ts);
		journalctl->mkreg(parent_i, request->new_file_name(), i);

		response->set_new_ino_prefix(ino_controller->get_prefix_from_uuid(i->get_ino()));
		response->set_new_ino_postfix(ino_controller->get_postfix_from_uuid(i->get_ino()));
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_unlink(::grpc::ServerContext *context, const ::rpc_unlink_request *request,
							  ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_unlink(" + request->filename() + ")");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		shared_ptr<inode> parent_i = parent_dentry_table->get_this_dir_inode();
		std::shared_ptr<inode> target_i = parent_dentry_table->get_child_inode(request->filename());
		nlink_t nlink = target_i->get_nlink() - 1;
		if (nlink == 0) {
			uuid target_ino = target_i->get_ino();
			/* data */
			data_pool->remove(obj_category::DATA, uuid_to_string(target_ino));

			/* parent dentry */
			parent_dentry_table->delete_child_inode(request->filename());

			struct timespec ts{};
			timespec_get(&ts, TIME_UTC);
			parent_i->set_mtime(ts);
			parent_i->set_ctime(ts);
			journalctl->rmreg(parent_i, request->filename(), target_i);
		} else {
			target_i->set_nlink(nlink);
			journalctl->chreg(target_i->get_p_ino(), target_i);
		}
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_write(::grpc::ServerContext *context, const ::rpc_write_request *request,
							 ::rpc_write_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_write()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	off_t offset;
	size_t size;
	std::shared_ptr<inode> i = parent_dentry_table->get_child_inode(request->filename());
	{
		std::scoped_lock scl{i->inode_mutex};
		offset = request->offset();
		size = request->size();

		if (request->flags() & O_APPEND) {
			offset = i->get_size();
		}

		if (i->get_size() < offset + size) {
			i->set_size(offset + size);

			journalctl->chreg(i->get_p_ino(), i);
		}
	}
	response->set_offset(offset);
	response->set_size(size);
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_chmod(::grpc::ServerContext *context, const ::rpc_chmod_request *request,
							 ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_chmod(" + request->filename() + ")");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		if (request->target_is_parent()) {
			global_logger.log(rpc_server_ops, "target is parent");
			i = parent_dentry_table->get_this_dir_inode();
		} else {
			global_logger.log(rpc_server_ops, "target is child");
			i = parent_dentry_table->get_child_inode(request->filename());
		}
	}

	{
		std::scoped_lock scl{i->inode_mutex};
		mode_t type = i->get_mode() & S_IFMT;

		i->set_mode(request->mode() | type);

		if(S_ISDIR(i->get_mode()))
			journalctl->chself(i);
		else
			journalctl->chreg(i->get_p_ino(), i);
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_chown(::grpc::ServerContext *context, const ::rpc_chown_request *request,
							 ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_chown(" + request->filename() + ")");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		if (request->target_is_parent()) {
			global_logger.log(rpc_server_ops, "target is parent");
			i = parent_dentry_table->get_this_dir_inode();
		} else {
			global_logger.log(rpc_server_ops, "target is child");
			i = parent_dentry_table->get_child_inode(request->filename());
		}
	}

	{
		std::scoped_lock scl{i->inode_mutex};
		if (((int32_t) request->uid()) >= 0)
			i->set_uid(request->uid());

		if (((int32_t) request->gid()) >= 0)
			i->set_gid(request->gid());

		if(S_ISDIR(i->get_mode()))
			journalctl->chself(i);
		else
			journalctl->chreg(i->get_p_ino(), i);
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_utimens(::grpc::ServerContext *context, const ::rpc_utimens_request *request,
							   ::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_utimens(" + request->filename() + ")");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		if (request->target_is_parent()) {
			global_logger.log(rpc_server_ops, "target is parent");
			i = parent_dentry_table->get_this_dir_inode();
		} else {
			global_logger.log(rpc_server_ops, "target is child");
			i = parent_dentry_table->get_child_inode(request->filename());
		}
	}

	{
		std::scoped_lock scl{i->inode_mutex};
		if (request->a_nsec() == UTIME_NOW) {
			struct timespec ts{};
			if (!timespec_get(&ts, TIME_UTC))
				throw runtime_error("timespec_get() failed");
			i->set_atime(ts);
		} else if (request->a_nsec() == UTIME_OMIT) { ;
		} else {
			struct timespec tv{};
			tv.tv_sec = request->a_sec();
			tv.tv_nsec = request->a_nsec();
			i->set_atime(tv);
		}

		if (request->m_nsec() == UTIME_NOW) {
			struct timespec ts{};
			if (!timespec_get(&ts, TIME_UTC))
				throw runtime_error("timespec_get() failed");
			i->set_mtime(ts);
		} else if (request->m_nsec() == UTIME_OMIT) { ;
		} else {
			struct timespec tv{};
			tv.tv_sec = request->m_sec();
			tv.tv_nsec = request->m_nsec();
			i->set_mtime(tv);
		}

		if(S_ISDIR(i->get_mode()))
			journalctl->chself(i);
		else
			journalctl->chreg(i->get_p_ino(), i);
	}
	response->set_ret(0);
	return Status::OK;
}

Status rpc_server::rpc_truncate(::grpc::ServerContext *context, const ::rpc_truncate_request *request,
								::rpc_common_respond *response) {
	global_logger.log(rpc_server_ops, "Called rpc_truncate()");
	uuid dentry_table_ino = ino_controller->splice_prefix_and_postfix(request->dentry_table_ino_prefix(), request->dentry_table_ino_postfix());

	std::shared_ptr<dentry_table> parent_dentry_table;
	try {
		parent_dentry_table = indexing_table->get_dentry_table(dentry_table_ino, true);
	} catch (dentry_table::not_leader &e){
		response->set_ret(-ENOTLEADER);
		return Status::OK;
	}

	std::shared_ptr<inode> i;
	{
		std::scoped_lock scl{parent_dentry_table->dentry_table_mutex};
		if (request->target_is_parent()) {
			global_logger.log(rpc_server_ops, "target is parent");
			i = parent_dentry_table->get_this_dir_inode();
		} else {
			global_logger.log(rpc_server_ops, "target is child");
			i = parent_dentry_table->get_child_inode(request->filename());
		}
	}

	{
		std::scoped_lock scl{i->inode_mutex};
		if (S_ISDIR(i->get_mode())) {
			response->set_ret(-EISDIR);
			return Status::OK;
		}

		i->set_size(request->offset());

		if(S_ISDIR(i->get_mode()))
			journalctl->chself(i);
		else
			journalctl->chreg(i->get_p_ino(), i);
	}
	response->set_ret(0);
	return Status::OK;
}
