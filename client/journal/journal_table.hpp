#ifndef _JOURNAL_TABLE_HPP_
#define _JOURNAL_TABLE_HPP_

#include <memory>
#include <shared_mutex>
#include <tsl/robin_map.h>

#include "transaction.hpp"

class journal_table {
private:
	std::shared_mutex sm;
	std::unique_ptr<tsl::robin_map<std::string, std::shared_ptr<transaction>>> map;

public:
	journal_table(void);
	~journal_table(void) = default;

	std::shared_ptr<transaction> get_entry(uuid ino);
	std::unique_ptr<tsl::robin_map<std::string, std::shared_ptr<transaction>>> replace_map(void);
};

#endif /* _JOURNAL_TABLE_HPP_ */
