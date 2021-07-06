#ifndef _JOURNAL_TABLE_HPP_
#define _JOURNAL_TABLE_HPP_

#include <memory>
#include <shared_mutex>

#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <tsl/robin_map.h>

#include "transaction.hpp"

using namespace boost::uuids;

using journal_map = tsl::robin_map<uuid, std::shared_ptr<transaction>, boost::hash<uuid>>;

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
