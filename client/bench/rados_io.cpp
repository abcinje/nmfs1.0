#include "rados_io.hpp"

#include <stdexcept>

using std::runtime_error;

rados_io::rados_io(const conn_info &ci, string pool)
{
	int ret;

	if ((ret = cluster.init2(ci.user.c_str(), ci.cluster.c_str(), ci.flags)) < 0) {
		throw runtime_error("rados_io::rados_io() failed "
				"(couldn't initialize the cluster handle)");
	} else {
	}


	if ((ret = cluster.conf_read_file("/etc/ceph/ceph.conf")) < 0) {
		cluster.shutdown();
		throw runtime_error("rados_io::rados_io() failed "
				"(couldn't read the Ceph configuration file)");
	} else {
	}

	if ((ret = cluster.connect()) < 0) {
		cluster.shutdown();
		throw runtime_error("rados_io::rados_io() failed "
				"(couldn't connect to cluster)");
	} else {
	}

	if ((ret = cluster.ioctx_create(pool.c_str(), ioctx)) < 0) {
		cluster.shutdown();
		throw runtime_error("rados_io::rados_io() failed "
				"(couldn't set up ioctx)");
	} else {
	}
}

rados_io::~rados_io(void)
{
	ioctx.close();

	cluster.shutdown();
}

size_t rados_io::read(const string &key, string &value, off_t offset, size_t len)
{
	uint64_t size;
	time_t mtime;
	int ret;

	if (!len) {
		if ((ret = ioctx.stat(key, &size, &mtime)) < 0) {
			if (ret == -ENOENT) {
				throw runtime_error("rados_io::read() failed \
						(object doesn't exist)");
			} else {
				throw runtime_error("rados_io::read() failed");
			}
		}

		len = (offset < size) ? (size - offset) : 0;
	}

	value.resize(len);
	librados::bufferlist bl = librados::bufferlist::static_from_string(value);

	ret = ioctx.read(key, bl, len, offset);
	if (ret >= 0) {
	} else if (ret == -ENOENT) {
		throw runtime_error("rados_io::read() failed (object doesn't exist)");
	} else {
		throw runtime_error("rados_io::read() failed");
	}

	value = bl.to_str();
	return ret;
}

size_t rados_io::write(const string &key, const string &value, off_t offset)
{
	int ret;
	size_t len;

	librados::bufferlist bl = librados::bufferlist::static_from_string
			(const_cast<string &>(value));

	if (offset) {
		ret = ioctx.write(key, bl, value.length(), offset);
	} else {
		ret = ioctx.write_full(key, bl);
	}

	if (ret >= 0) {
	} else {
		throw runtime_error("rados_io::write() failed");
	}

	return value.length();
}

bool rados_io::exist(const string &key)
{
	int ret;
	uint64_t size;
	time_t mtime;

	ret = ioctx.stat(key, &size, &mtime);
	if (ret >= 0) {
		return true;
	} else if (ret == -ENOENT) {
		return false;
	} else {
		throw runtime_error("rados_io::exist() failed");
	}
}

void rados_io::remove(const string &key)
{
	int ret;

	ret = ioctx.remove(key);
	if (ret >= 0) {
	} else if (ret == -ENOENT) {
	} else {
		throw runtime_error("rados_io::remove() failed");
	}
}
