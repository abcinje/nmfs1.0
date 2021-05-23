#ifndef _COMMIT_HPP_
#define _COMMIT_HPP_

#include <memory>

#include "journal_table.hpp"
#include "mqueue.hpp"

#define JOURNALING_PERIOD_MS 5000

class commit {
private:
	bool *stopped;
	rados_io *meta;
	journal_table *table;
	mqueue<std::shared_ptr<transaction>> *q;

public:
	commit(bool *stopped_flag, rados_io *meta_pool, journal_table *jtable, mqueue<std::shared_ptr<transaction>> *queue);
	~commit(void) = default;

	void operator()(void);
};

#endif /* _COMMIT_HPP_ */
