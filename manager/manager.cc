#include <iostream>
#include <memory>
#include <string>

using std::string;

#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

using grpc::Server;
using grpc::ServerBuilder;

#include "lease.pb.h"
#include "lease.grpc.pb.h"

class lease_impl final : public lease::Service {
};

int main(int argc, char *argv[])
{
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " <ip_addr> <port>" << std::endl;
		return 1;
	}

	string server_address(string(argv[1]) + ":" + string(argv[2]));
	lease_impl service;

	ServerBuilder builder;
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&service);
	std::unique_ptr<Server> server(builder.BuildAndStart());
	server->Wait();

	return 0;
}
