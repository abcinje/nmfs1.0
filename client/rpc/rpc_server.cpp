#include "rpc_server.hpp"

extern rados_io *meta_pool;
extern rados_io *data_pool;
extern directory_table *indexing_table;

extern std::map<ino_t, unique_ptr<file_handler>> fh_list;
extern std::mutex file_handler_mutex;

Status rpc_server::rpc_getattr(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::rpc_getattr_respond *response) {
	return Service::rpc_getattr(context, request, response);
}

Status rpc_server::rpc_access(::grpc::ServerContext *context, const ::rpc_common_request *request,
							  ::rpc_common_respond *response) {
	return Service::rpc_access(context, request, response);
}

Status rpc_server::rpc_opendir(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::rpc_common_respond *response) {
	return Service::rpc_opendir(context, request, response);
}

Status rpc_server::rpc_readdir(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::grpc::ServerWriter<::rpc_name_respond> *writer) {
	return Service::rpc_readdir(context, request, writer);
}

Status rpc_server::rpc_mkdir(::grpc::ServerContext *context, const ::rpc_common_request *request,
							 ::rpc_common_respond *response) {
	return Service::rpc_mkdir(context, request, response);
}

Status rpc_server::rpc_rmdir(::grpc::ServerContext *context, const ::rpc_common_request *request,
							 ::rpc_common_respond *response) {
	return Service::rpc_rmdir(context, request, response);
}

Status rpc_server::rpc_symlink(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::rpc_common_respond *response) {
	return Service::rpc_symlink(context, request, response);
}

Status rpc_server::rpc_readlink(::grpc::ServerContext *context, const ::rpc_common_request *request,
								::rpc_name_respond *response) {
	return Service::rpc_readlink(context, request, response);
}

Status rpc_server::rpc_rename_same_parent(::grpc::ServerContext *context, const ::rpc_common_request *request,
										  ::rpc_common_respond *response) {
	return Service::rpc_rename_same_parent(context, request, response);
}

Status rpc_server::rpc_rename_not_same_parent(::grpc::ServerContext *context, const ::rpc_common_request *request,
											  ::rpc_common_respond *response) {
	return Service::rpc_rename_not_same_parent(context, request, response);
}

Status rpc_server::rpc_open(::grpc::ServerContext *context, const ::rpc_common_request *request,
							::rpc_common_respond *response) {
	return Service::rpc_open(context, request, response);
}

Status rpc_server::rpc_create(::grpc::ServerContext *context, const ::rpc_common_request *request,
							  ::rpc_common_respond *response) {
	return Service::rpc_create(context, request, response);
}

Status rpc_server::rpc_unlink(::grpc::ServerContext *context, const ::rpc_common_request *request,
							  ::rpc_common_respond *response) {
	return Service::rpc_unlink(context, request, response);
}

Status rpc_server::rpc_read(::grpc::ServerContext *context, const ::rpc_common_request *request,
							::rpc_common_respond *response) {
	return Service::rpc_read(context, request, response);
}

Status rpc_server::rpc_write(::grpc::ServerContext *context, const ::rpc_common_request *request,
							 ::rpc_common_respond *response) {
	return Service::rpc_write(context, request, response);
}

Status rpc_server::rpc_chmod(::grpc::ServerContext *context, const ::rpc_common_request *request,
							 ::rpc_common_respond *response) {
	return Service::rpc_chmod(context, request, response);
}

Status rpc_server::rpc_chown(::grpc::ServerContext *context, const ::rpc_common_request *request,
							 ::rpc_common_respond *response) {
	return Service::rpc_chown(context, request, response);
}

Status rpc_server::rpc_utimens(::grpc::ServerContext *context, const ::rpc_common_request *request,
							   ::rpc_common_respond *response) {
	return Service::rpc_utimens(context, request, response);
}

Status rpc_server::rpc_truncate(::grpc::ServerContext *context, const ::rpc_common_request *request,
								::rpc_common_respond *response) {
	return Service::rpc_truncate(context, request, response);
}
