#ifndef _LEASE_CLIENT_HPP_
#define _LEASE_CLIENT_HPP_

#include <memory>

#include <grpcpp/grpcpp.h>

#include <lease.pb.h>
#include <lease.grpc.pb.h>

#include "lease_table_client.hpp"

using grpc::Channel;

class lease_client {
private:
	std::unique_ptr<lease::Stub> stub;
	lease_table_client table;

public:
	lease_client(std::shared_ptr<Channel> channel);
	~lease_client(void) = default;

	bool access(ino_t ino);
	int acquire(ino_t ino);
};

#endif /* _LEASE_CLIENT_HPP_ */
