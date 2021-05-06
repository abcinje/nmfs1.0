#include "client.hpp"

/*
 * client(rados_io *meta_pool)
 * : constructor that get unused client id number from client.list which is in metadata pool
 * and allocate it to the field ""client_id"
 */

#define MAX_CLIENT_NUM 4096
extern rados_io *meta_pool;
client::client() : per_client_ino_offset(0) {
	/* TODO : client id allocation */

}

client::~client() {

}

uint64_t client::get_client_id() {
	std::scoped_lock<std::recursive_mutex> scl{this->client_mutex};
	return this->client_id;
}
uint64_t client::get_per_client_ino_offset(){
	std::scoped_lock<std::recursive_mutex> scl{this->client_mutex};
	return this->per_client_ino_offset;
}
void client::increase_ino_offset() {
	std::scoped_lock<std::recursive_mutex> scl{this->client_mutex};
	this->per_client_ino_offset++;
	meta_pool->write(CLIENT, std::to_string(this->get_client_id()), reinterpret_cast<const char *>(&(this->per_client_ino_offset)), sizeof(uint64_t), 0);
}
