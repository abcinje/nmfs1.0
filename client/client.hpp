#ifndef NMFS0_CLIENT_HPP
#define NMFS0_CLIENT_HPP

#include "../rados_io/rados_io.hpp"

class client {
private:
	int clinet_id;

public:
	client();
	client(int id);
	~client();

	int get_client_id();
};


#endif //NMFS0_CLIENT_HPP
