#include "lease_table_client.hpp"

#include <iostream>

lease_table_client::lease_entry::lease_entry(const system_clock::time_point &new_due) : due(new_due)
{
}

system_clock::time_point lease_table_client::lease_entry::get_due(void)
{
	std::shared_lock lock(sm);
	return due;
}

void lease_table_client::lease_entry::set_due(const system_clock::time_point &new_due)
{
	std::unique_lock lock(sm);
	due = new_due;
}

lease_table_client::~lease_table_client(void)
{
	std::cerr << "Some thread has called ~lease_table_client()." << std::endl;
	std::cerr << "Aborted." << std::endl;
	exit(1);
}

bool lease_table_client::check(ino_t ino)
{
	lease_entry *e;

	{
		std::shared_lock lock(sm);
		auto it = map.find(ino);
		if (it != map.end()) {
			e = it->second;
		} else {
			return false;
		}
	}

	return system_clock::now() < e->get_due();
}

void lease_table_client::update(ino_t ino, const system_clock::time_point &new_due)
{
	lease_entry *e;
	bool found = false;

	{
		std::shared_lock lock(sm);
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
		std::unique_lock lock(sm);
		if (map.find(ino) == map.end())
			map[ino] = new lease_entry(new_due);
	}
}
