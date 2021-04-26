#include "lease_client.hpp"

using grpc::ClientContext;
using grpc::Status;

lease_client::lease_client(std::shared_ptr<Channel> channel, const std::string &self_remote)
		: stub(lease::NewStub(channel)), remote(self_remote)
{
}

bool lease_client::access(ino_t ino)
{
	return table.check(ino);
}

int lease_client::acquire(ino_t ino, std::string &remote_addr)
{
	lease_request request;
	request.set_ino(ino);
	request.set_remote_addr(remote);

	lease_response response;

	ClientContext context;

	Status status = stub->acquire(&context, request, &response);

	if (status.ok()) {
		system_clock::time_point due{system_clock::duration{response.due()}};
		table.update(ino, due);
		remote_addr = response.remote_addr();
		return response.ret();
	} else {
		std::cerr << "[" << status.error_code() << "] " << status.error_message() << std::endl;
		throw std::runtime_error("lease_client::acquire() failed");
	}
}
