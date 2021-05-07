#include "lease_table_client.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

std::string serializeTimePoint(const std::chrono::system_clock::time_point& time, const std::string& format)
{
	std::time_t tt = std::chrono::system_clock::to_time_t(time);
	std::tm tm = *std::gmtime(&tt); //GMT (UTC)
	std::stringstream ss;
	ss << std::put_time( &tm, format.c_str() );
	return ss.str();
}

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
	global_logger.log(lease_ops, "Called check(" + std::to_string(ino) + ")");
	lease_entry *e;

	{
		std::shared_lock lock(sm);
		auto it = map.find(ino);
		if (it != map.end()) {
			global_logger.log(lease_ops, "Find lease in the table!");
			e = it->second;
		} else {
			global_logger.log(lease_ops, "Failed to find lease duration");
			return false;
		}
	}
	const std::chrono::system_clock::time_point input = system_clock::now();
	global_logger.log(lease_ops, "system_clock::now: " + serializeTimePoint(input, "UTC: %Y-%m-%d %H:%M:%S"));
	global_logger.log(lease_ops, "e->get_due(): " + serializeTimePoint(e->get_due(), "UTC: %Y-%m-%d %H:%M:%S"));

	if(input < e->get_due())
		global_logger.log(lease_ops, "TRUE : LEASE IS VALID");
	else
		global_logger.log(lease_ops, "FALSE : LEASE IS EXPIRED");

	return system_clock::now() < e->get_due();
}

void lease_table_client::update(ino_t ino, const system_clock::time_point &new_due)
{
	global_logger.log(lease_ops, "Called update(" + std::to_string(ino) + ")");
	global_logger.log(lease_ops, "new_due: " + serializeTimePoint(new_due, "UTC: %Y-%m-%d %H:%M:%S"));

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
		auto ret = map.insert({ino, nullptr});
		if (ret.second)
			ret.first.value() = new lease_entry(new_due);
	}
}
