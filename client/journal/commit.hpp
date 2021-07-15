#ifndef _COMMIT_HPP_
#define _COMMIT_HPP_

#include <memory>
#include <thread>

#include "lib/rados_io/rados_io.hpp"

#include "journal.hpp"
#include "journal_table.hpp"
#include "mqueue.hpp"

#define JOURNALING_PERIOD_MS 5000

class commit {
private:
	bool *stopped;
	std::shared_ptr<rados_io> meta;
	journal_table *table;
	mqueue<std::shared_ptr<transaction>> *q;

public:
	commit(bool *stopped_flag, std::shared_ptr<rados_io> meta_pool, journal_table *jtable, mqueue<std::shared_ptr<transaction>> *queue);
	~commit(void) = default;

	void operator()(void);
};

#endif /* _COMMIT_HPP_ */
