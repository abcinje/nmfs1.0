#ifndef _SESSION_CLIENT_HPP_
#define _SESSION_CLIENT_HPP_

#include <memory>

#include <grpcpp/grpcpp.h>

#include "session.pb.h"
#include "session.grpc.pb.h"

using grpc::Channel;

class session_client {
private:
	std::unique_ptr<session::Stub> stub;

public:
	session_client(std::shared_ptr<Channel> channel);
	~session_client(void) = default;

	uint64_t mount(void);
	void umount(void);
};

#endif /* _SESSION_CLIENT_HPP_ */
