#include "lease_impl.hpp"

Status lease_impl::acquire(ServerContext *context, const lease_request *request, lease_response *response)
{
	int64_t due;
	int ret = table.acquire(request->ino(), &due);

	response->set_ret(ret);
	response->set_due(ret == -1 ? 0 : due);

	return Status::OK;
}
