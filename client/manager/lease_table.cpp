#include "lease_table.hpp"

#include <iostream>

lease_table::lease_entry::lease_entry(const system_clock::time_point &new_due) : due(new_due)
{
}

system_clock::time_point lease_table::lease_entry::get_due(void) const
{
	std::shared_lock<std::shared_mutex>(sm);
	return due;
}

void lease_table::lease_entry::set_due(const system_clock::time_point &new_due)
{
	std::unique_lock<std::shared_mutex>(sm);
	due = new_due;
}

lease_table::~lease_table(void)
{
	std::cerr << "Some thread has called ~lease_table()." << std::endl;
	std::cerr << "Aborted." << std::endl;
	exit(1);
}

bool lease_table::within_due(ino_t ino) const
{
	lease_entry *e;

	{
		std::shared_lock<std::shared_mutex>(sm);
		auto it = map.find(ino);
		if (it != map.end()) {
			e = it->second;
		} else {
			return false;
		}
	}

	return system_clock::now() < e->get_due();
}

void lease_table::update_due(ino_t ino, const system_clock::time_point &new_due)
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

	if (found) {
		e->set_due(new_due);
		return;
	}

	{
		std::unique_lock<std::shared_mutex>(sm);
		if (map.find(ino) == map.end())
			map[ino] = new lease_entry(new_due);
	}
}
