#include "journal_table.hpp"

std::shared_ptr<transaction> journal_table::get_entry(uuid ino)
{
	{
		std::shared_lock lock(sm);
		auto it = map.find(ino);
		if (it != map.end())
			return it->second;
	}

	{
		std::unique_lock lock(sm);
		auto ret = map.insert({ino, nullptr});
		if (ret.second) {
			return ret.first.value() = std::make_shared<transaction>();
		} else {
			return ret.first->second;
		}
	}
}
