#include "lease_impl.hpp"

Status lease_impl::acquire(ServerContext *context, const lease_request *request, lease_response *response)
{
	response->set_ret(-1);
	response->set_due(0);
	return Status::OK;
}
