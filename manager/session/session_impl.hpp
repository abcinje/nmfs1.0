#ifndef _SESSION_IMPL_HPP_
#define _SESSION_IMPL_HPP_

#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

#include <session.pb.h>
#include <session.grpc.pb.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <tsl/robin_map.h>

#include "../../lib/rados_io/rados_io.hpp"

#define CLIENT_BITS 24

class session_impl final : public session::Service {
private:
	enum state {
		INVALID,
		VALID,
		RECOVERY,
	};

	std::mutex m;
	std::shared_ptr<rados_io> pool;
	std::vector<char> map;
	tsl::robin_map<uint64_t, std::string> addrmap;

	Status mount(ServerContext *context, const empty *dummy_in, client_id *id) override;
	Status umount(ServerContext *context, const empty *dummy_in, empty *dummy_out) override;

public:
	session_impl(std::shared_ptr<rados_io> meta_pool);
	~session_impl(void) = default;
};

#endif /* _SESSION_IMPL_HPP_ */
