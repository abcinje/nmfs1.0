#ifndef NMFS0_CLIENT_HPP
#define NMFS0_CLIENT_HPP

#include "../rados_io/rados_io.hpp"

class client {
private:
	uint64_t clinet_id;

public:
	client();
	client(int id);
	~client();

	uint64_t get_client_id();
};


#endif //NMFS0_CLIENT_HPP
