#ifndef _MANAGER_IMPL_HPP_
#define _MANAGER_IMPL_HPP_

#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

#include <manager.pb.h>
#include <manager.grpc.pb.h>

#include "lease_table.hpp"

class manager_impl final : public manager::Service {
private:
	lease_table table;

	Status lease_acquire(ServerContext *context, const lease_request *request, lease_response *response) override;
};

#endif /* _MANAGER_IMPL_HPP_ */
