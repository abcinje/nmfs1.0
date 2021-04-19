#ifndef _MANAGER_CLIENT_HPP_
#define _MANAEGR_CLIENT_HPP_

#include <memory>

#include <grpcpp/grpcpp.h>

#include <manager.pb.h>
#include <manager.grpc.pb.h>

#include "lease_table.hpp"

using grpc::Channel;

class manager_client {
private:
	std::unique_ptr<manager::Stub> stub;
	lease_table table;

public:
	manager_client(std::shared_ptr<Channel> channel);
	~manager_client(void) = default;

	bool lease_access(ino_t ino);
	int lease_acquire(ino_t ino);
};

#endif /* _MANAGER_CLIENT_HPP_ */
