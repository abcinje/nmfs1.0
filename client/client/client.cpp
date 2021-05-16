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
	meta_pool->write(obj_category::CLIENT, std::to_string(this->client_id), reinterpret_cast<const char *>(&offset), sizeof(uint64_t), 0);
}

uid_t client::get_client_uid() const {
	return client_uid;
}

gid_t client::get_client_gid() const {
	return client_gid;
}

void client::set_client_uid(uid_t client_uid) {
	client::client_uid = client_uid;
}

void client::set_client_gid(gid_t client_gid) {
	client::client_gid = client_gid;
}
