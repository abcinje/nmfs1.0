#include "session_impl.hpp"

#include <stdexcept>

#include <boost/tokenizer.hpp>

Status session_impl::mount(ServerContext *context, const empty *dummy_in, client_id *id)
{
	std::scoped_lock lock(m);

	if (next_id < (1 << CLIENT_BITS)) {
		id->set_id(next_id++);
		pool->write(CLIENT, "next_id", reinterpret_cast<const char *>(&next_id), sizeof(uint64_t), 0);
	} else {
		id->set_id(-1);
	}

	return Status::OK;
}

Status session_impl::umount(ServerContext *context, const empty *dummy_in, empty *dummy_out)
{
	return Status::OK;
}

session_impl::session_impl(std::shared_ptr<rados_io> meta_pool) : pool(meta_pool)
{
	size_t size;

	if (pool->stat(CLIENT, "next_id", size)) {
		pool->read(CLIENT, "next_id", reinterpret_cast<char *>(&next_id), size, 0);
	} else {
		next_id = 1;
	}
}
