#include "lease_table.hpp"

#include <iostream>

lease_table::lease_entry::lease_entry(void) : due(system_clock::now() + milliseconds(LEASE_PERIOD_MS))
{
}

bool lease_table::lease_entry::cas(system_clock::time_point &new_due)
{
	std::unique_lock<std::mutex>(m);

	if (system_clock::now() >= due) {
		new_due = due = system_clock::now() + milliseconds(LEASE_PERIOD_MS);
		return true;
	}

	return false;
}

lease_table::~lease_table(void)
{
	std::cerr << "Some thread has called ~lease_table()." << std::endl;
	std::cerr << "Aborted." << std::endl;
	exit(1);
}

int lease_table::acquire(ino_t ino, system_clock::time_point &new_due)
{
	lease_entry *e;
	bool found = false;

	{
		std::shared_lock<std::shared_mutex>(sm);
		auto it = map.find(ino);
		if (it != map.end()) {
			found = true;
			e = it->second;
		}
	}

	if (found)
		return e->cas(new_due) ? 0 : -1;

	{
		std::unique_lock<std::shared_mutex>(sm);
		auto it = map.find(ino);
		if (it != map.end()) {
			return -1;
		} else {
			map[ino] = new lease_entry();
			return 0;
		}
	}
}
