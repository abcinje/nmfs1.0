#include "client.hpp"

/*
 * client(rados_io *meta_pool)
 * : constructor that get unused client id number from client.list which is in metadata pool
 * and allocate it to the field ""client_id"
 */

#define MAX_CLIENT_NUM 4096
extern rados_io *meta_pool;
client::client() {
	std::string client_list;
	int client_list_len;

	client_list.reserve(MAX_CLIENT_NUM);

	client_list_len = meta_pool->read("client.list", client_list.data(), MAX_CLIENT_NUM, 0);

	/* there is reusable client id in client_list */
	for(int i = 1; i < client_list_len; i++){
		if(client_list.at(i) == 'x'){
			this->clinet_id = i;
			client_list.replace(i, 1, "o");
			meta_pool->write("client.list", client_list.data(), client_list_len, 0);
			return;
		}
	}

	this->clinet_id = client_list_len;
	// if reserve doesn't really span str.data(), client_list += "o";
	client_list.replace(this->clinet_id, 1, "o");
	meta_pool->write("client.list", client_list.data(), client_list_len + 1, 0);
}

/*
 * client(int id)
 * : constructor for really first created client
 */
client::client(int id) : clinet_id(id) {

}

client::~client() {
	std::string client_list;
	int client_list_len;

	client_list.reserve(MAX_CLIENT_NUM);

	client_list_len = meta_pool->read("client.list", client_list.data(), MAX_CLIENT_NUM, 0);
	client_list.replace(this->clinet_id, 1, "x");
	meta_pool->write("client.list", client_list.data(), client_list_len, 0);
}