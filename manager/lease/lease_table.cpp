#include "lease_table.hpp"

#include "../../lib/logger/logger.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

static std::string TimepointToString(const std::chrono::system_clock::time_point& time, const std::string& format)
{
	std::time_t tt = std::chrono::system_clock::to_time_t(time);
	std::tm tm = *std::gmtime(&tt); //GMT (UTC)
	std::stringstream ss;
	ss << std::put_time( &tm, format.c_str() );
	return ss.str();
}

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
	global_logger.log(manager_lease, "CAS has been called. Current time is " + TimepointToString(due, "UTC: %Y-%m-%d %H:%M:%S"));
	std::unique_lock lock(sm);

	if (system_clock::now() >= due) {
		global_logger.log(manager_lease, "CAS: success :)");
		global_logger.log(manager_lease, "  (" + addr + ", " + TimepointToString(due, "UTC: %Y-%m-%d %H:%M:%S") + ")");
		latest_due = due = system_clock::now() + milliseconds(LEASE_PERIOD_MS);
		addr = remote_addr;
		global_logger.log(manager_lease, "  -> (" + addr + ", " + TimepointToString(due, "UTC: %Y-%m-%d %H:%M:%S") + ")");
		return true;
	}

	global_logger.log(manager_lease, "CAS: failure :(");
	global_logger.log(manager_lease, "  (" + addr + ", " + TimepointToString(due, "UTC: %Y-%m-%d %H:%M:%S") + ")");
	global_logger.log(manager_lease, "  -> Not expired yet");
	latest_due = due;
	remote_addr = addr;
	return false;
}

lease_table::~lease_table(void)
{
	std::cerr << "Some thread has called ~lease_table()." << std::endl;
	std::cerr << "Aborted." << std::endl;
	exit(1);
}

int lease_table::acquire(ino_t ino, system_clock::time_point &latest_due, std::string &remote_addr)
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
		return e->cas(latest_due, remote_addr) ? 0 : -1;

	{
		std::unique_lock lock(sm);
		auto ret = map.insert({ino, nullptr});
		if (ret.second) {
			ret.first.value() = new lease_entry(latest_due, remote_addr);
			return 0;
		} else {
			std::tie(latest_due, remote_addr) = ret.first->second->get_info();
			return -1;
		}
	}
}
