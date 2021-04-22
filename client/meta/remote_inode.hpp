//
// Created by bori1 on 2021-04-23.
//

#ifndef NMFS_REMOTE_INODE_H
#define NMFS_REMOTE_INODE_H

#include "inode.hpp"

class remote_inode : public inode {
private :
    std::string address;
public:
    remote_inode(std::string address);
};


#endif //NMFS_REMOTE_INODE_H
