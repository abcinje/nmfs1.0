//
// Created by bori1 on 2021-04-23.
//

#include "remote_inode.hpp"
remote_inode::remote_inode(std::string address) : inode(), address(address) {
}

const string &remote_inode::get_address() const {
	return address;
}

ino_t remote_inode::get_dentry_table_ino() const {
	return dentry_table_ino;
}

const string &remote_inode::get_file_name() const {
	return file_name;
}

