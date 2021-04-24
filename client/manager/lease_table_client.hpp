#ifndef _LEASE_TABLE_CLIENT_HPP_
#define _LEASE_TABLE_CLIENT_HPP_

#include <chrono>
#include <shared_mutex>
#include <tsl/robin_map.h>

using namespace std::chrono;

class lease_table_client {
private:
	class lease_entry {
	private:
		std::shared_mutex sm;
		system_clock::time_point due;

	public:
		lease_entry(const system_clock::time_point &new_due);
		~lease_entry(void) = default;

		system_clock::time_point get_due(void) const;
		void set_due(const system_clock::time_point &new_due);
	};

	std::shared_mutex sm;
	tsl::robin_map<ino_t, lease_entry *> map;

public:
	lease_table_client(void) = default;
	~lease_table_client(void);

	bool check(ino_t ino) const;
	void update(ino_t ino, const system_clock::time_point &new_due);
};

#endif /* _LEASE_TABLE_CLIENT_HPP_ */
