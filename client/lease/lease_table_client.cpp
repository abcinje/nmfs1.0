#include "lease_table_client.hpp"

std::string serializeTimePoint(const std::chrono::system_clock::time_point& time, const std::string& format)
{
	std::time_t tt = std::chrono::system_clock::to_time_t(time);
	std::tm tm = *std::gmtime(&tt); //GMT (UTC)
	std::stringstream ss;
	ss << std::put_time( &tm, format.c_str() );
	return ss.str();
}

lease_table_client::lease_entry::lease_entry(const system_clock::time_point &new_due, bool mine) : due(new_due), leader(mine)
{
}

system_clock::time_point lease_table_client::lease_entry::get_due(void)
{
	std::shared_lock lock(sm);
	return due;
}

std::tuple<system_clock::time_point, bool> lease_table_client::lease_entry::get_info(void)
{
	std::shared_lock lock(sm);
	return std::make_tuple(due, leader);
}

void lease_table_client::lease_entry::set_info(const system_clock::time_point &new_due, bool mine)
{
	std::unique_lock lock(sm);
	if (new_due > due) {
		due = new_due;
		leader = mine;
	}
}

lease_table_client::~lease_table_client(void)
{
	std::cerr << "Some thread has called ~lease_table_client()." << std::endl;
	std::cerr << "Aborted." << std::endl;
	exit(1);
}

bool lease_table_client::is_valid(uuid ino)
{
	global_logger.log(lease_ops, "Called is_valid(" + to_string(ino) + ")");
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

bool lease_table_client::is_mine(uuid ino)
{
	global_logger.log(lease_ops, "Called is_mine(" + to_string(ino) + ")");
	lease_entry *e;
	system_clock::time_point latest_due;
	bool mine;

	{
		std::shared_lock lock(sm);
		auto it = map.find(ino);
		if (it != map.end()) {
			e = it->second;
		} else {
			return false;
		}
	}

	std::tie(latest_due, mine) = e->get_info();
	return mine && (system_clock::now() < latest_due);
}

void lease_table_client::update(uuid ino, const system_clock::time_point &new_due, bool mine)
{
	global_logger.log(lease_ops, "Called update(" + to_string(ino) + ")");
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
		e->set_info(new_due, mine);
		return;
	}

	{
		std::unique_lock lock(sm);
		auto ret = map.insert({ino, nullptr});
		if (ret.second) {
			ret.first.value() = new lease_entry(new_due, mine);
		} else {
			ret.first->second->set_info(new_due, mine);
		}
	}
}
