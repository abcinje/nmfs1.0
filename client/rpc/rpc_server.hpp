#ifndef NMFS_RPC_SERVER_HPP
#define NMFS_RPC_SERVER_HPP

#include <rpc.grpc.pb.h>
#include "../in_memory/directory_table.hpp"
#include "../meta/file_handler.hpp"
#include "../util.hpp"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

void run_rpc_server(const std::string& remote_address);

class rpc_server : public remote_ops::Service {
public:
    Status rpc_check_child_inode(::grpc::ServerContext *context, const ::rpc_dentry_table_request *request,
				 ::rpc_dentry_table_respond *response) override;

    Status rpc_get_mode(::grpc::ServerContext *context, const ::rpc_inode_request *request,
			::rpc_inode_respond *response) override;

    Status rpc_getattr(::grpc::ServerContext *context, const ::rpc_common_request *request,
		       ::rpc_getattr_respond *response) override;

    Status rpc_access(::grpc::ServerContext *context, const ::rpc_access_request *request,
		      ::rpc_common_respond *response) override;

    Status rpc_opendir(::grpc::ServerContext *context, const ::rpc_open_opendir_request *request,
		       ::rpc_common_respond *response) override;

    Status rpc_readdir(::grpc::ServerContext *context, const ::rpc_readdir_request *request,
		       ::grpc::ServerWriter<::rpc_name_respond> *writer) override;

    Status rpc_mkdir(::grpc::ServerContext *context, const ::rpc_mkdir_request *request,
		     ::rpc_mkdir_respond *response) override;

    Status rpc_rmdir_top(::grpc::ServerContext *context, const ::rpc_rmdir_request *request,
		     ::rpc_common_respond *response) override;

    Status rpc_rmdir_down(::grpc::ServerContext *context, const ::rpc_rmdir_request *request,
		     ::rpc_common_respond *response) override;

    Status rpc_symlink(::grpc::ServerContext *context, const ::rpc_symlink_request *request,
		       ::rpc_common_respond *response) override;

    Status rpc_readlink(::grpc::ServerContext *context, const ::rpc_readlink_request *request,
			::rpc_name_respond *response) override;

    Status rpc_rename_same_parent(::grpc::ServerContext *context, const ::rpc_rename_same_parent_request *request,
				  ::rpc_common_respond *response) override;

    Status rpc_rename_not_same_parent_src(::grpc::ServerContext *context, const ::rpc_rename_not_same_parent_src_request *request,
				      ::rpc_rename_not_same_parent_src_respond *response) override;

    Status rpc_rename_not_same_parent_dst(::grpc::ServerContext *context, const ::rpc_rename_not_same_parent_dst_request *request,
					  ::rpc_common_respond *response) override;

    Status rpc_open(::grpc::ServerContext *context, const ::rpc_open_opendir_request *request,
		    ::rpc_common_respond *response) override;

    Status rpc_create(::grpc::ServerContext *context, const ::rpc_create_request *request,
		      ::rpc_create_respond *response) override;

    Status rpc_unlink(::grpc::ServerContext *context, const ::rpc_unlink_request *request,
		      ::rpc_common_respond *response) override;

    Status rpc_write(::grpc::ServerContext *context, const ::rpc_write_request *request,
		     ::rpc_write_respond *response) override;

    Status rpc_chmod(::grpc::ServerContext *context, const ::rpc_chmod_request *request,
		     ::rpc_common_respond *response) override;

    Status rpc_chown(::grpc::ServerContext *context, const ::rpc_chown_request *request,
		     ::rpc_common_respond *response) override;

    Status rpc_utimens(::grpc::ServerContext *context, const ::rpc_utimens_request *request,
		       ::rpc_common_respond *response) override;

    Status rpc_truncate(::grpc::ServerContext *context, const ::rpc_truncate_request *request,
			::rpc_common_respond *response) override;

};


#endif //NMFS_RPC_SERVER_HPP
