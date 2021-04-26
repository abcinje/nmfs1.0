#include "lease_impl.hpp"

#include <chrono>

using namespace std::chrono;

Status lease_impl::acquire(ServerContext *context, const lease_request *request, lease_response *response)
{
	system_clock::time_point due;
	std::string remote_addr = request->remote_addr();
	int ret = table.acquire(request->ino(), due, remote_addr);

	response->set_ret(ret);
	response->set_due(ret == -1 ? 0 : due.time_since_epoch().count());
	response->set_remote_addr(remote_addr);

	return Status::OK;
}
