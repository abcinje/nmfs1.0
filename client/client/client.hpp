#ifndef NMFS0_CLIENT_HPP
#define NMFS0_CLIENT_HPP

#include <atomic>
#include <mutex>

#include "../../lib/rados_io/rados_io.hpp"
#include "session_client.hpp"

class client {
private:
	/* client_id : natural number */
	uint64_t client_id;

	uid_t client_uid;
	gid_t client_gid;
public:
	client(std::shared_ptr<Channel> channel);
	~client() = default;

	uint64_t get_client_id();
	uid_t get_client_uid() const;
	gid_t get_client_gid() const;
	void set_client_uid(uid_t client_uid);
	void set_client_gid(gid_t client_gid);
};

#endif //NMFS0_CLIENT_HPP
