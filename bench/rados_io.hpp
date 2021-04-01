#ifndef _RADOS_IO_HPP_
#define _RADOS_IO_HPP_

#include <string>
#include <rados/librados.hpp>

#define POOL_NAME "mypool"
using std::string;

class rados_io {
public:
	librados::Rados cluster;
	librados::IoCtx ioctx;

	struct conn_info {
		string user;
		string cluster;
		int64_t flags;
	};

	rados_io(const conn_info &ci, string pool);
	~rados_io(void);

	/*
	 * synchronous operations
	 *
	 * When len == 0, read() function reads the whole value starting from offset.
	 */
	size_t read(const string &key, string &value, off_t offset = 0, size_t len = 0);
	size_t write(const string &key, const string &value, off_t offset = 0);
	bool exist(const string &key);
	void remove(const string &key);
};

#endif /* _RADOS_IO_HPP_ */
