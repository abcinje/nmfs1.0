#ifndef NMFS0_CLIENT_HPP
#define NMFS0_CLIENT_HPP

#include <mutex>
#include "../rados_io/rados_io.hpp"

class client {
private:
	/* client_id : natural number */
	uint64_t client_id;
	uint64_t per_client_ino_offset;

	std::recursive_mutex client_mutex;
public:
	client();
	client(int id);
	~client();

	uint64_t get_client_id();
	uint64_t get_per_client_ino_offset();

	void increase_ino_offset();
};


#endif //NMFS0_CLIENT_HPP
