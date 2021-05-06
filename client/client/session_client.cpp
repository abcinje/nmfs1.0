#include "session_client.hpp"

#include <stdexcept>

using grpc::ClientContext;
using grpc::Status;

session_client::session_client(std::shared_ptr<Channel> channel) : stub(session::NewStub(channel))
{
}

uint64_t session_client::mount(void)
{
	empty dummy;
	client_id id;

	ClientContext context;

	Status status = stub->mount(&context, dummy, &id);

	if (status.ok()) {
		return id.id();
	} else {
		std::cout << "[" << status.error_code() << "] " << status.error_message() << std::endl;
		throw std::runtime_error("session_client::mount() failed");
	}
}

void session_client::umount(void)
{
	empty dummy;

	ClientContext context;

	Status status = stub->umount(&context, dummy, &dummy);

	if (status.ok()) {
		return;
	} else {
		std::cout << "[" << status.error_code() << "] " << status.error_message() << std::endl;
		throw std::runtime_error("session_client::umount() failed");
	}
}
