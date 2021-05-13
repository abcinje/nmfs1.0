#ifndef _LEASE_CLIENT_HPP_
#define _LEASE_CLIENT_HPP_

#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include <lease.pb.h>
#include <lease.grpc.pb.h>

#include "lease_table_client.hpp"

using grpc::Channel;

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
	 * access()
	 *
	 * Check whether the lease for the ino is valid.
	 * Note(1) This routine treats LOCAL and REMOTE inode in the same manner.
	 *         What it can tell us is whether the lease is expired or not.
	 * Note(2) The lease table doesn't guarantee that the lease information is up-to-date.
	 *         Do not trust the content of the table.
	 *         But you may trust the lease information about the client itself.
	 *
	 * This function returns false if the lease has been expired.
	 * Otherwise, it returns true.
	 */
	bool access(ino_t ino);

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
	int acquire(ino_t ino, std::string &remote_addr);
};

#endif /* _LEASE_CLIENT_HPP_ */
