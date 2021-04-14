#include "client.hpp"

/*
 * client(rados_io *meta_pool)
 * : constructor that get unused client id number from client.list which is in metadata pool
 * and allocate it to the field ""client_id"
 */

#define MAX_CLIENT_NUM 4096
extern rados_io *meta_pool;
client::client() {
	std::unique_ptr<char[]> client_list = std::make_unique<char[]>(MAX_CLIENT_NUM);
	int client_list_len;

	client_list_len = meta_pool->read(CLIENT, "client.list", client_list.get(), MAX_CLIENT_NUM, 0);

	/* there is reusable client id in client_list */
	for(int i = 1; i < client_list_len; i++){
		if(client_list[i] == 'x'){
			this->client_id = i;
			client_list[i] = 'o';
			meta_pool->write(CLIENT, "client.list", client_list.get(), client_list_len, 0);

			try {
				meta_pool->read(CLIENT, std::to_string(this->client_id),
								reinterpret_cast<char *>(&(this->per_client_ino_offset)), sizeof(uint64_t) , 0);
			} catch(rados_io::no_such_object &e){
				throw std::runtime_error("Failed to mount new client");
			}
			return;
		}
	}

	this->client_id = client_list_len;
	// if reserve doesn't really span str.data(), client_list += "o";
	client_list[this->client_id] = 'o';
	meta_pool->write(CLIENT, "client.list", client_list.get(), client_list_len + 1, 0);
	this->per_client_ino_offset = 0;
	this->increase_ino_offset();
}

/*
 * client(int id)
 * : constructor for really first created client
 */
client::client(int id) : client_id(id), per_client_ino_offset(0) {
	this->increase_ino_offset();
}

client::~client() {
	std::unique_ptr<char[]> client_list = std::make_unique<char[]>(MAX_CLIENT_NUM);
	int client_list_len;

	client_list_len = meta_pool->read(CLIENT, "client.list", client_list.get(), MAX_CLIENT_NUM, 0);
	client_list[this->client_id] = 'x';
	meta_pool->write(CLIENT, "client.list", client_list.get(), client_list_len, 0);
}

uint64_t client::get_client_id() {
	std::shared_lock<std::shared_mutex> sl(this->client_mutex);
	return this->client_id;
}
uint64_t client::get_per_client_ino_offset(){
	std::shared_lock<std::shared_mutex> sl(this->client_mutex);
	return this->per_client_ino_offset;
}
void client::increase_ino_offset() {
	std::unique_lock<std::shared_mutex> ul(this->client_mutex, std::defer_lock);
	ul.lock();
	this->per_client_ino_offset++;
	ul.unlock();
	meta_pool->write(CLIENT, std::to_string(this->get_client_id()), reinterpret_cast<const char *>(&(this->per_client_ino_offset)), sizeof(uint64_t), 0);
}
