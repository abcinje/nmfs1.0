#include "client.hpp"

/*
 * client(rados_io *meta_pool)
 * : constructor that get unused client id number from client.list which is in metadata pool
 * and allocate it to the field ""client_id"
 */

extern rados_io *meta_pool;

client::client(std::shared_ptr<Channel> channel) : per_client_ino_offset(0) {
	session_client session(channel);
	client_id = session.mount();
}

uint64_t client::get_client_id() {
	return this->client_id;
}

uint64_t client::get_per_client_ino_offset() {
	return this->per_client_ino_offset;
}

void client::increase_ino_offset() {
	this->per_client_ino_offset++;
	uint64_t offset = per_client_ino_offset;
	meta_pool->write(CLIENT, std::to_string(this->client_id), reinterpret_cast<const char *>(&offset), sizeof(uint64_t), 0);
}
