#include "../rados_io/rados_io.hpp"

#include <iostream>
#include <string>
#include <sys/stat.h>

class inode {
public:
	mode_t	i_mode;
	uid_t	i_uid;
	gid_t	i_gid;
	ino_t	i_ino;
	nlink_t	i_nlink;
	off_t	i_size;
};

int main(void)
{
	std::string pool_name;
	ino_t ino;

	std::cout << "Pool: ";
	std::cin >> pool_name;
	std::cout << "Inode number: ";
	std::cin >> ino;

	rados_io::conn_info ci = {
		.user = "client.admin",
		.cluster = "ceph",
		.flags = 0
	};

	rados_io rio(ci, pool_name);

	char *buf = new char[sizeof(inode)];
	rio.read("i$" + std::to_string(ino), buf, sizeof(inode), 0);
	inode *i = reinterpret_cast<inode *>(buf);

	std::cout << "mode:	" << i->i_mode	<< std::endl;
	std::cout << "uid:	" << i->i_uid	<< std::endl;
	std::cout << "gid:	" << i->i_gid	<< std::endl;
	std::cout << "ino:	" << i->i_ino	<< std::endl;
	std::cout << "nlink:	" << i->i_nlink	<< std::endl;
	std::cout << "size:	" << i->i_size	<< std::endl;

	delete i;

	return 0;
}
