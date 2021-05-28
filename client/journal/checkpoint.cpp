#include "checkpoint.hpp"

checkpoint::checkpoint(rados_io *meta_pool, mqueue<std::shared_ptr<transaction>> *queue) : meta(meta_pool), q(queue)
{
}

void checkpoint::operator()(void)
{
	while (true) {
		auto tx = q->dispatch();
		if (tx == nullptr)	/* stopped */
			break;

		tx->checkpoint(meta);
	}
}
