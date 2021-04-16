#include "lease_table.hpp"

lease_table::lease_entry::lease_entry(system_clock::time_point due) : _due(due)
{
}

system_clock::time_point lease_table::lease_entry::get_time(void) const
{
	std::scoped_lock(_m);
	return _due;
}

void lease_table::lease_entry::set_time(system_clock::time_point due)
{
	std::scoped_lock(_m);
	_due = due;
}

lease_table::~lease_table(void)
{
	std::scoped_lock(m);
	for (const auto &e : umap)
		delete e.second;
}

int lease_table::acquire(ino_t ino)
{
	lease_entry *e;

	{
		std::scoped_lock(m);
		auto it = umap.find(ino);
		if (it != umap.end()) {
			e = it->second;
		} else {
			auto due = system_clock::now() + milliseconds(LEASE_PERIOD_MS);
			umap[ino] = new lease_entry(due);
			return 0;
		}
	}

	if (e->get_time() <= system_clock::now()) {
		auto due = system_clock::now() + milliseconds(LEASE_PERIOD_MS);
		e->set_time(due);
		return 0;
	}

	return -1;
}

void lease_table::release(ino_t ino)
{
	lease_entry *e;

	{
		std::scoped_lock(m);
		auto it = umap.find(ino);
		if (it != umap.end())
			e = it->second;
		else
			return;
	}

	e->set_time(system_clock::now());
}
