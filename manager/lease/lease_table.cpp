#include "lease_table.hpp"

#include "../../lib/logger/logger.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

static std::string TimepointToString(const std::chrono::system_clock::time_point& p_tpTime)
{
	std::string p_sFormat("%H%M%S%.6f");
	auto converted_timep = std::chrono::system_clock::to_time_t(p_tpTime);
	std::ostringstream oss;
	oss << std::put_time(std::localtime(&converted_timep), p_sFormat.c_str());
	return oss.str();
}

lease_table::lease_entry::lease_entry(const std::string &remote_addr) : due(system_clock::now() + milliseconds(LEASE_PERIOD_MS)), addr(remote_addr)
{
}

bool lease_table::lease_entry::cas(system_clock::time_point &new_due, std::string &remote_addr)
{
	global_logger.log(manager_lease, "CAS has been called. Current time is " + TimepointToString(due));
	std::unique_lock lock(m);

	if (system_clock::now() >= due) {
		global_logger.log(manager_lease, "CAS: success :)");
		global_logger.log(manager_lease, "  (" + addr + ", " + TimepointToString(due) + ")");
		new_due = due = system_clock::now() + milliseconds(LEASE_PERIOD_MS);
		addr = remote_addr;
		global_logger.log(manager_lease, "  -> (" + addr + ", " + TimepointToString(due) + ")");
		return true;
	}

	global_logger.log(manager_lease, "CAS: failure :(");
	global_logger.log(manager_lease, "  (" + addr + ", " + TimepointToString(due) + ")");
	global_logger.log(manager_lease, "  -> Not expired yet");
	remote_addr = addr;
	return false;
}

lease_table::~lease_table(void)
{
	std::cerr << "Some thread has called ~lease_table()." << std::endl;
	std::cerr << "Aborted." << std::endl;
	exit(1);
}

int lease_table::acquire(ino_t ino, system_clock::time_point &new_due, std::string &remote_addr)
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

	if (found)
		return e->cas(new_due, remote_addr) ? 0 : -1;

	{
		std::unique_lock lock(sm);
		auto ret = map.insert({ino, nullptr});
		if (ret.second) {
			ret.first.value() = new lease_entry(remote_addr);
			return 0;
		} else {
			return -1;
		}
	}
}
