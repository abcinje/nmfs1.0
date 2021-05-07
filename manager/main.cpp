#include <iostream>
#include <memory>
#include <string>

using std::string;

#include "../lib/rados_io/rados_io.hpp"

#include "lease/lease_impl.hpp"
#include "session/session_impl.hpp"

#define META_POOL "nmfs.meta"

int main(int argc, char *argv[])
{
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " <manager_ip> <manager_port>" << std::endl;
		return 1;
	}

	rados_io::conn_info ci = {"client.admin", "ceph", 0};
	auto meta_pool = std::make_shared<rados_io>(ci, META_POOL);

	string server_address(string(argv[1]) + ":" + string(argv[2]));
	lease_impl lease_service;
	session_impl session_service(meta_pool);

	ServerBuilder builder;
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	builder.RegisterService(&lease_service);
	builder.RegisterService(&session_service);
	std::unique_ptr<Server> server(builder.BuildAndStart());
	server->Wait();

	return 0;
}
