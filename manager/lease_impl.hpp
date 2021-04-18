#ifndef _LEASE_IMPL_HPP_
#define _LEASE_IMPL_HPP_

#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

#include "lease.pb.h"
#include "lease.grpc.pb.h"

#include "lease_table.hpp"

class lease_impl final : public lease::Service {
private:
	lease_table table;

	Status acquire(ServerContext *context, const lease_request *request, lease_response *response) override;
};

#endif /* _LEASE_IMPL_HPP_ */
