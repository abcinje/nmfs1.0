#include "journal_table.hpp"

journal_table::journal_table(void)
{
	map = std::make_unique<tsl::robin_map<std::string, std::shared_ptr<transaction>>>();
}

std::shared_ptr<transaction> journal_table::get_entry(uuid ino)
{
	{
		std::shared_lock lock(sm);
		auto it = map->find(uuid_to_string(ino));
		if (it != map->end())
			return it->second;
	}

	{
		std::unique_lock lock(sm);
		auto ret = map->insert({uuid_to_string(ino), nullptr});
		if (ret.second) {
			return ret.first.value() = std::make_shared<transaction>();
		} else {
			return ret.first->second;
		}
	}
}

std::unique_ptr<tsl::robin_map<std::string, std::shared_ptr<transaction>>> journal_table::replace_map(void)
{
	std::unique_lock lock(sm);

	auto temp = std::move(map);
	map = std::make_unique<tsl::robin_map<std::string, std::shared_ptr<transaction>>>();
	return temp;
}
