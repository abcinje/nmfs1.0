#ifndef NMFS_REMOTE_INODE_H
#define NMFS_REMOTE_INODE_H

#include "inode.hpp"

class rpc_client;

std::shared_ptr<rpc_client> get_rpc_client(const std::string& remote_address);

class remote_inode : public inode {
private :
    std::string leader_ip;

	ino_t dentry_table_ino;
	std::string file_name;

public:
    	remote_inode(std::string leader_ip, ino_t dentry_table_ino, std::string file_name);

	[[nodiscard]] const string &get_address() const;

	[[nodiscard]] ino_t get_dentry_table_ino() const;
	[[nodiscard]] const string &get_file_name() const;

	mode_t get_mode() override;
    	void permission_check(int mask) override;
};


#endif //NMFS_REMOTE_INODE_H
