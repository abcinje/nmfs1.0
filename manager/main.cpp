#include <iostream>
#include <memory>
#include <string>

using std::string;

#include "lease/lease_impl.hpp"

int main(int argc, char *argv[])
{
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " <ip_addr> <port>" << std::endl;
		return 1;
	}

	string server_address(string(argv[1]) + ":" + string(argv[2]));
	lease_impl lease_service;

	ServerBuilder builder;
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&lease_service);
	std::unique_ptr<Server> server(builder.BuildAndStart());
	server->Wait();

	return 0;
}
