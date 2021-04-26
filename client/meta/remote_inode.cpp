//
// Created by bori1 on 2021-04-23.
//

#include "remote_inode.hpp"
remote_inode::remote_inode(std::string address) : inode(), address(address) {
}

std::string remote_inode::get_address() {
	return this->address;
}