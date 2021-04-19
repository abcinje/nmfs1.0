#include "manager_impl.hpp"

#include <chrono>

using namespace std::chrono;

Status manager_impl::lease_acquire(ServerContext *context, const lease_request *request, lease_response *response)
{
	system_clock::time_point due;
	int ret = table.acquire(request->ino(), due);

	response->set_ret(ret);
	response->set_due(ret == -1 ? 0 : due.time_since_epoch().count());

	return Status::OK;
}
