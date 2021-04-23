#include "session_impl.hpp"

#include <stdexcept>

#include <boost/tokenizer.hpp>

Status session_impl::mount(ServerContext *context, const empty *dummy_in, client_id *id)
{
	uint32_t new_id;
	std::string address;

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

	/* address */
	address = context->peer();

	/* Record an <addr, id> pair */
	imap[address] = new_id;

	/* Store imap */
	std::string imap_string;
	for (auto it = imap.cbegin(); it != imap.cend(); it++)
		imap_string += it->first + "<" + std::to_string(it->second) + "/";
	pool->write(CLIENT, "client.imap", imap_string.data(), imap_string.size(), 0);

	/* Store map */
	pool->write(CLIENT, "client.map", map.data(), map.size(), 0);

	id->set_id(new_id);
	return Status::OK;
}

Status session_impl::umount(ServerContext *context, const empty *dummy_in, empty *dummy_out)
{
	uint32_t deleted_id;
	std::string address;

	/* address */
	address = context->peer();

	/* Erase an <addr, id> pair */
	auto it = imap.find(address);
	if (it != imap.end()) {
		deleted_id = it->second;
	} else {
		throw std::logic_error("session_impl::unmount() failed (Cannot find the client id)");
	}
	imap.erase(address);

	/* Update map */
	map[deleted_id] = INVALID;

	/* Store map */
	pool->write(CLIENT, "client.map", map.data(), map.size(), 0);

	/* Store imap */
	std::string imap_string;
	for (auto it = imap.cbegin(); it != imap.cend(); it++)
		imap_string += it->first + "<" + std::to_string(it->second) + "/";
	pool->write(CLIENT, "client.imap", imap_string.data(), imap_string.size(), 0);

	return Status::OK;
}

session_impl::session_impl(std::shared_ptr<rados_io> meta_pool) : pool(meta_pool)
{
	size_t size;

	if (pool->stat(CLIENT, "client.map", size)) {
		map.resize(size);
		pool->read(CLIENT, "client.map", map.data(), size, 0);
	}

	if (pool->stat(CLIENT, "client.imap", size)) {
		std::string imap_value;
		imap_value.resize(size);
		pool->read(CLIENT, "client.imap", imap_value.data(), size, 0);

		std::string delimiters("/<");
		boost::char_separator<char> sep(delimiters.c_str());
		boost::tokenizer<boost::char_separator<char>> tok(imap_value, sep);

		for (auto it = tok.begin(); it != tok.end(); ) {
			std::string address(*(it++));
			uint32_t client_id = static_cast<uint32_t>(std::stoul(*(it++)));
			imap[address] = client_id;
		}
	}
}
