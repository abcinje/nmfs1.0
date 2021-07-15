#ifndef _LEASE_CLIENT_HPP_
#define _LEASE_CLIENT_HPP_

#include <memory>
#include <string>

#include <boost/uuid/uuid.hpp>
#include <grpcpp/grpcpp.h>

#include "lease.pb.h"
#include "lease.grpc.pb.h"

#include "lease_table_client.hpp"

using grpc::Channel;
using namespace boost::uuids;

class lease_client {
private:
	std::unique_ptr<lease::Stub> stub;
	std::string remote;
	lease_table_client table;

public:
	/*
	 * lease_client()
	 *
	 * 'self_remote' should be the remote server address of itself
	 */
	lease_client(std::shared_ptr<Channel> channel, const std::string &self_remote);
	~lease_client(void) = default;

	/*
	 * is_valid()
	 *
	 * Check whether the lease for the ino is valid.
	 * Note(1) This routine treats LOCAL and REMOTE inode in the same manner.
	 *         What it can tell us is whether the lease is expired or not.
	 * Note(2) The lease table doesn't guarantee that the lease information is up-to-date.
	 *         Do not trust the content of the table.
	 *         But you may trust the lease information about the client itself.
	 *
	 * This function returns true if the lease is valid now.
	 * Otherwise, it returns false.
	 */
	bool is_valid(uuid ino);

	/*
	 * is_mine()
	 *
	 * Check whether the lease for the ino is valid *AND* the lease is mine.
	 * The journal module may need to call this function before taking directory journals.
	 *
	 * This function returns true if the lease is valid
	 * and I am the leader of the corresponding directory.
	 * Otherwise, it returns false.
	 */
	bool is_mine(uuid ino);

	/*
	 * acquire()
	 *
	 * On success
	 * - Return 0
	 *
	 * On failure
	 * - Return -1
	 * - 'remote_addr' is changed to the address of the directory leader'
	 */
	int acquire(uuid ino, std::string &remote_addr);
};

#endif /* _LEASE_CLIENT_HPP_ */
