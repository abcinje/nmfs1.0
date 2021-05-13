#ifndef _LEASE_TABLE_CLIENT_HPP_
#define _LEASE_TABLE_CLIENT_HPP_

#include <chrono>
#include <shared_mutex>
#include <tuple>
#include <tsl/robin_map.h>
#include "../../lib/logger/logger.hpp"

using namespace std::chrono;

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
		void set_due(const system_clock::time_point &new_due, bool mine);
	};

	std::shared_mutex sm;
	tsl::robin_map<ino_t, lease_entry *> map;

public:
	lease_table_client(void) = default;
	~lease_table_client(void);

	bool is_valid(ino_t ino);
	bool is_mine(ino_t ino);
	void update(ino_t ino, const system_clock::time_point &new_due, bool mine);
};

#endif /* _LEASE_TABLE_CLIENT_HPP_ */
