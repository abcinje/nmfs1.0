#include "lease_table.hpp"

lease_table::lease_entry::lease_entry(system_clock::time_point &latest_due, const std::string &remote_addr) : due(system_clock::now() + milliseconds(LEASE_PERIOD_MS)), addr(remote_addr)
{
	latest_due = due;
}

std::tuple<system_clock::time_point, std::string> lease_table::lease_entry::get_info(void)
{
	std::shared_lock lock(sm);
	return std::make_tuple(due, addr);
}

bool lease_table::lease_entry::cas(system_clock::time_point &latest_due, std::string &remote_addr)
{
	std::unique_lock lock(sm);

	if (system_clock::now() >= due) {
		latest_due = due = system_clock::now() + milliseconds(LEASE_PERIOD_MS);
		addr = remote_addr;
		return true;
	} else if (addr == remote_addr) {
		latest_due = due;
		return true;
	} else {
		latest_due = due;
		remote_addr = addr;
		return false;
	}
}

lease_table::~lease_table(void)
{
	std::cerr << "Some thread has called ~lease_table()." << std::endl;
	std::cerr << "Aborted." << std::endl;
	exit(1);
}

int lease_table::acquire(uuid ino, system_clock::time_point &latest_due, std::string &remote_addr)
{
	lease_entry *e;
	std::string addr;
	bool found = false;

	{
		std::shared_lock lock(sm);
		auto it = map.find(uuid_to_string(ino));
		if (it != map.end()) {
			found = true;
			e = it->second;
		}
	}

	if (found)
		return e->cas(latest_due, remote_addr) ? 0 : -1;

	{
		std::unique_lock lock(sm);
		auto ret = map.insert({uuid_to_string(ino), nullptr});
		if (ret.second) {
			ret.first.value() = new lease_entry(latest_due, remote_addr);
			return 0;
		} else {
			std::tie(latest_due, addr) = ret.first->second->get_info();
			return (addr == remote_addr) ? 0 : -1;
		}
	}
}
