#ifndef _LEASE_TABLE_H_
#define _LEASE_TABLE_H_

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <tsl/robin_map.h>

using namespace std::chrono;

#define LEASE_PERIOD_MS 10000

class lease_table {
private:
	class lease_entry {
	private:
		std::mutex m;
		system_clock::time_point due;

	public:
		lease_entry(void);
		~lease_entry(void) = default;

		bool cas(system_clock::time_point &new_due);
	};

	std::shared_mutex sm;
	tsl::robin_map<ino_t, lease_entry *> map;

public:
	lease_table(void) = default;
	~lease_table(void);

	int acquire(ino_t ino, system_clock::time_point &new_due);
};

#endif /* _LEASE_TABLE_H_ */
