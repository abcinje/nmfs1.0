#include "remote_inode.hpp"
#include "../rpc/rpc_client.hpp"

/* <address, channel> */
std::map<std::string, std::shared_ptr<rpc_client>> rc_list;

std::shared_ptr<rpc_client> get_rpc_client(const std::string& remote_address) {
	global_logger.log(remote_fs_op, "Called  get_rpc_client()");
	std::map<std::string, std::shared_ptr<rpc_client>>::iterator it;
	it = rc_list.find(remote_address);

	if(it != rc_list.end()){
		return it->second;
	} else {
		std::shared_ptr<rpc_client> rc = std::make_shared<rpc_client>(grpc::CreateChannel(remote_address, grpc::InsecureChannelCredentials()));
		rc_list.insert(std::make_pair(remote_address,rc));
		return rc;
	}
}

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

mode_t remote_inode::get_mode() {
	std::string remote_address(this->leader_ip);
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	mode_t returend_mode = rc->get_mode(this->dentry_table_ino, this->file_name);

	/* store returned i_mode value for next called permission_check() */
	this->inode::set_mode(returend_mode);

	return returend_mode;
}

void remote_inode::permission_check(int mask) {
	std::string remote_address(this->leader_ip);
	std::shared_ptr<rpc_client> rc = get_rpc_client(remote_address);

	rc->permission_check(this->dentry_table_ino, this->file_name, mask);
}

