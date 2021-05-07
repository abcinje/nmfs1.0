#ifndef _LEASE_TABLE_HPP_
#define _LEASE_TABLE_HPP_

#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <tsl/robin_map.h>

using namespace std::chrono;

#define LEASE_PERIOD_MS 10000

class lease_table {
private:
	class lease_entry {
	private:
		std::mutex m;
		system_clock::time_point due;
		std::string addr;

	public:
		lease_entry(system_clock::time_point &new_due, const std::string &remote_addr);
		~lease_entry(void) = default;

		/*
		 * cas() - Try to acquire the lease atomically
		 *
		 * On success
		 * - Return true
		 * - 'new_due' is set to the updated due
		 *
		 * On failure
		 * - Return false
		 * - 'remote_addr' is changed to the address of the current leader
		 */
		bool cas(system_clock::time_point &new_due, std::string &remote_addr);
	};

	std::shared_mutex sm;
	tsl::robin_map<ino_t, lease_entry *> map;

public:
	lease_table(void) = default;
	~lease_table(void);

	/*
	 * acquire() - Try to acquire the lease atomically
	 *
	 * On success
	 * - Return 0
	 * - 'new_due' is set to the updated due
	 *
	 * On failure
	 * - Return -1
	 * - 'remote_addr' is changed to the address of the current leader
	 */
	int acquire(ino_t ino, system_clock::time_point &new_due, std::string &remote_addr);
};

#endif /* _LEASE_TABLE_HPP_ */
