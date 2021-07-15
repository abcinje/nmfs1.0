#ifndef _LEASE_TABLE_CLIENT_HPP_
#define _LEASE_TABLE_CLIENT_HPP_

#include <chrono>
#include <iomanip>
#include <iostream>
#include <shared_mutex>
#include <sstream>
#include <tuple>

#include <boost/functional/hash.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <tsl/robin_map.h>

#include "lib/logger/logger.hpp"
#include "util/uuid.hpp"

using namespace std::chrono;
using namespace boost::uuids;

class lease_table_client {
private:
	class lease_entry {
	private:
		std::shared_mutex sm;
		system_clock::time_point due;
		bool leader;

	public:
		lease_entry(const system_clock::time_point &new_due, bool mine);
		~lease_entry(void) = default;

		system_clock::time_point get_due(void);
		std::tuple<system_clock::time_point, bool> get_info(void);
		void set_info(const system_clock::time_point &new_due, bool mine);
	};

	std::shared_mutex sm;
	tsl::robin_map<uuid, lease_entry *, boost::hash<uuid>> map;

public:
	lease_table_client(void) = default;
	~lease_table_client(void);

	bool is_valid(uuid ino);
	bool is_mine(uuid ino);
	void update(uuid ino, const system_clock::time_point &new_due, bool mine);
};

#endif /* _LEASE_TABLE_CLIENT_HPP_ */
