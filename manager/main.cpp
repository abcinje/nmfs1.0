#include <iostream>
#include <memory>
#include <string>

#include <libconfig.h++>

#include "lib/rados_io/rados_io.hpp"

#include "lease/lease_impl.hpp"
#include "session/session_impl.hpp"

using namespace libconfig;

template <typename T>
static T lookup_config(const Config &config, const char *field)
{
	try {
		return config.lookup(field);
	} catch (const SettingNotFoundException &e) {
		std::cerr << "No \'" << field << "\' setting in configuration file." << std::endl;
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <config_path>" << std::endl;
		return 1;
	}

	/* Get configuration */

	Config cfg;

	try {
		cfg.readFile(argv[1]);
	} catch (const FileIOException &e) {
		std::cerr << "I/O error while reading file." << std::endl;
		exit(1);
	}

	std::string manager_ip = lookup_config<std::string>(cfg, "manager_ip");
	std::string manager_port = lookup_config<std::string>(cfg, "manager_port");
	std::string meta_pool_name = lookup_config<std::string>(cfg, "meta_pool_name");

	/* Launch service */

	rados_io::conn_info ci = {"client.admin", "ceph", 0};
	auto meta_pool = std::make_shared<rados_io>(ci, meta_pool_name);

	std::string server_address(manager_ip + ":" + manager_port);
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
