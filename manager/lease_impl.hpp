#ifndef _LEASE_IMPL_H_
#define _LEASE_IMPL_H_

#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

#include "lease.pb.h"
#include "lease.grpc.pb.h"

class lease_impl final : public lease::Service {
private:
	Status acquire(ServerContext *context, const lease_request *request, lease_response *response) override;
};

#endif /* _LEASE_IMPL_H_ */
