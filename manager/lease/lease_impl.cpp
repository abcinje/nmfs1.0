#include "lease_impl.hpp"

#include <chrono>

using namespace std::chrono;

Status lease_impl::acquire(ServerContext *context, const lease_request *request, lease_response *response)
{
	system_clock::time_point due;
	std::string remote_addr = request->remote_addr();
	int ret = table.acquire(uuid_controller::splice_prefix_and_postfix(request->ino_prefix(), request->ino_postfix()), due, remote_addr);

	response->set_ret(ret);
	response->set_due(due.time_since_epoch().count());

	if (ret)
		response->set_remote_addr(remote_addr);

	return Status::OK;
}
