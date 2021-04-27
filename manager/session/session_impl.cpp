#include "session_impl.hpp"

#include <stdexcept>

#include <boost/tokenizer.hpp>

Status session_impl::mount(ServerContext *context, const empty *dummy_in, client_id *id)
{
	std::unique_lock<std::mutex> lock(m);

	uint32_t new_id;
	std::string address;

	/* address */
	address = context->peer();

	/* Check if the address is already in addrmap */
	for (auto it = addrmap.cbegin(); it != addrmap.cend(); it++)
		if (it->second == address) {
			id->set_id(-1);
			return Status::OK;
		}

	/* Choose a client id and update map */
	if (map.size() == 0) {
		map.push_back(INVALID);
		map.push_back(VALID);
		new_id = 1;
	} else {
		uint32_t i;

		for (i = 1; i < map.size(); i++)
			if (map[i] == INVALID) {
				new_id = i;
				break;
			}

		if (i == map.size()) {
			if (i < (2 << 24)) {
				map.push_back(VALID);
				new_id = i;
			} else {
				id->set_id(-1);
				return Status::OK;
			}
		}
	}

	/* Record an <id, addr> pair */
	addrmap[new_id] = address;

	/* Store addrmap */
	std::string addrmap_string;
	for (auto it = addrmap.cbegin(); it != addrmap.cend(); it++)
		addrmap_string += std::to_string(it->first) + ">" + it->second + "/";
	pool->write(CLIENT, "client.addrmap", addrmap_string.data(), addrmap_string.size(), 0);

	/* Store map */
	pool->write(CLIENT, "client.map", map.data(), map.size(), 0);

	id->set_id(new_id);
	return Status::OK;
}

Status session_impl::umount(ServerContext *context, const empty *dummy_in, empty *dummy_out)
{
	std::unique_lock<std::mutex> lock(m);

	uint32_t deleted_id;
	std::string address;

	/* address */
	address = context->peer();

	/* Erase an <addr, id> pair */
	tsl::robin_map<uint32_t, std::string>::const_iterator it;
	for (it = addrmap.cbegin(); it != addrmap.cend(); it++)
		if (it->second == address) {
			deleted_id = it->first;
			break;
		}

	if (it == addrmap.cend())
		throw std::logic_error("session_impl::unmount() failed (Cannot find the client id)");

	addrmap.erase(deleted_id);

	/* Update map */
	map[deleted_id] = INVALID;

	/* Store map */
	pool->write(CLIENT, "client.map", map.data(), map.size(), 0);

	/* Store addrmap */
	std::string addrmap_string;
	for (it = addrmap.cbegin(); it != addrmap.cend(); it++)
		addrmap_string += std::to_string(it->first) + ">" + it->second + "/";
	pool->write(CLIENT, "client.addrmap", addrmap_string.data(), addrmap_string.size(), 0);

	return Status::OK;
}

session_impl::session_impl(std::shared_ptr<rados_io> meta_pool) : pool(meta_pool)
{
	size_t size;

	if (pool->stat(CLIENT, "client.map", size)) {
		map.resize(size);
		pool->read(CLIENT, "client.map", map.data(), size, 0);
	}

	if (pool->stat(CLIENT, "client.addrmap", size)) {
		std::string addrmap_string;
		addrmap_string.resize(size);
		pool->read(CLIENT, "client.addrmap", addrmap_string.data(), size, 0);

		std::string delimiters("/>");
		boost::char_separator<char> sep(delimiters.c_str());
		boost::tokenizer<boost::char_separator<char>> tok(addrmap_string, sep);

		for (auto it = tok.begin(); it != tok.end(); ) {
			uint32_t client_id = static_cast<uint32_t>(std::stoul(*(it++)));
			addrmap[client_id] = *(it++);
		}
	}
}
