#include "remote_inode.hpp"
#include "../rpc/rpc_client.hpp"

remote_inode::remote_inode(std::string leader_ip, ino_t dentry_table_ino, std::string file_name) \
: inode(), leader_ip(leader_ip), dentry_table_ino(dentry_table_ino), file_name(file_name) {
}

const string &remote_inode::get_address() const {
	return leader_ip;
}

ino_t remote_inode::get_dentry_table_ino() const {
	return dentry_table_ino;
}

const string &remote_inode::get_file_name() const {
	return file_name;
}

void remote_inode::set_mode(mode_t mode) {
	std::string remote_address(this->leader_ip);
	std::unique_ptr<rpc_client> rc = std::make_unique<rpc_client>(grpc::CreateChannel(remote_address, grpc::InsecureChannelCredentials()));

	mode_t returend_mode = rc->get_mode(this->dentry_table_ino, this->file_name);

	/* store returned i_mode value for next called permission_check() */
	this->inode::set_mode(returend_mode);
}

