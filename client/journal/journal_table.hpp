#ifndef _JOURNAL_TABLE_HPP_
#define _JOURNAL_TABLE_HPP_

#include <memory>
#include <shared_mutex>
#include <tsl/robin_map.h>

#include "transaction.hpp"

using journal_map = tsl::robin_map<std::string, std::shared_ptr<transaction>>;

class journal_table {
private:
	std::shared_mutex sm;
	std::unique_ptr<journal_map> map;

public:
	journal_table(void);
	~journal_table(void) = default;

	void delete_entry(const uuid &ino);				/* for check */
	std::shared_ptr<transaction> get_entry(const uuid &ino);	/* for operation */
	std::unique_ptr<journal_map> replace_map(void);			/* for commit */
};

#endif /* _JOURNAL_TABLE_HPP_ */
