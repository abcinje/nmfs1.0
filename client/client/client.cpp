#include "client.hpp"

/*
 * client(rados_io *meta_pool)
 * : constructor that get unused client id number from client.list which is in metadata pool
 * and allocate it to the field ""client_id"
 */

extern rados_io *meta_pool;

client::client(std::shared_ptr<Channel> channel) {
	session_client session(channel);
	client_id = session.mount();
}

uint64_t client::get_client_id() {
	return this->client_id;
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
