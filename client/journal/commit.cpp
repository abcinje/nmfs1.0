#include "commit.hpp"

#include <thread>

commit::commit(bool *stopped_flag, std::shared_ptr<rados_io> meta_pool, journal_table *jtable, mqueue<std::shared_ptr<transaction>> *queue) : stopped(stopped_flag), meta(meta_pool), table(jtable), q(queue)
{
}

void commit::operator()(void)
{
	std::chrono::milliseconds period(JOURNALING_PERIOD_MS);

	std::this_thread::sleep_for(period);

	while (!(*stopped)) {
		auto cycle_end = std::chrono::system_clock::now() + period;

		auto map = table->replace_map();
		for (const auto &p : *map) {
			/* Write a transaction to its journal */
			auto tx = p.second;
			tx->commit(meta);

			/* Enqueue the committed transaction */
			q->issue(tx);
		}

		std::this_thread::sleep_until(cycle_end);
	}

	q->issue(nullptr);
}
