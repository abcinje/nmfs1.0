#include "checkpoint.hpp"

void checkpoint::operator()(void)
{
	while (true) {
		auto tx = q->dispatch();
		if (tx == nullptr)	/* stopped */
			break;

		tx->checkpoint(meta);
	}
}
