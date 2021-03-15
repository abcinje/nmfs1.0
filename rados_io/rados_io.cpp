#include "rados_io.hpp"
#include "../logger/logger.hpp"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static string get_prefix(enum obj_category category)
{
	switch (category) {
	case INODE:
		return "i$";
	case DENTRY:
		return "e$";
	case DATA:
		return "d$";
	case CLIENT:
		return "c$";
	default:
		throw logic_error("get_prefix() failed (unknown category " + std::to_string(category) + ")");
	}
}

static string get_postfix(uint64_t num)
{
	return "#" + std::to_string(num);
}

size_t rados_io::read_obj(const string &key, char *value, size_t len, off_t offset)
{
	global_logger.log(rados_io_ops,"Called rados_io::read_obj()");
	global_logger.log(rados_io_ops,"key : " + key + " length : " + std::to_string(len) + " offset : " + std::to_string(offset));
	uint64_t size;
	time_t mtime;
	int ret;

	librados::bufferlist bl = librados::bufferlist::static_from_mem(value, len);

	ret = ioctx.read(key, bl, len, offset);
	if (ret >= 0) {
		global_logger.log(rados_io_ops,"Read an object. (key: \"" + key + "\")");
	} else if (ret == -ENOENT) {
		throw no_such_object("rados_io::read_obj() failed (key: \"" + key + "\")");
	} else {
		throw runtime_error("rados_io::read_obj() failed");
	}

	memcpy(value, bl.c_str(), ret);

	return ret;
}

size_t rados_io::write_obj(const string &key, const char *value, size_t len, off_t offset)
{
	global_logger.log(rados_io_ops,"Called rados_io::write_obj()");
	global_logger.log(rados_io_ops,"key : " + key + " length : " + std::to_string(len) + " offset : " + std::to_string(offset));
	int ret;

	librados::bufferlist bl = librados::bufferlist::static_from_mem(const_cast<char *>(value), len);
	ret = ioctx.write(key, bl, len, offset);

	if (ret >= 0) {
		global_logger.log(rados_io_ops, "Wrote an object. (key: \"" + key + "\")");
	} else {
		throw runtime_error("rados_io::write_obj() failed");
	}

	return len;
}

rados_io::no_such_object::no_such_object(const string &msg, size_t nb) : runtime_error(msg), num_bytes(nb)
{
}

const char *rados_io::no_such_object::what(void)
{
	return runtime_error::what();
}

rados_io::rados_io(const conn_info &ci, string pool)
{
	int ret;

	if ((ret = cluster.init2(ci.user.c_str(), ci.cluster.c_str(), ci.flags)) < 0) {
		throw runtime_error("rados_io::rados_io() failed "
				"(couldn't initialize the cluster handle)");
	}
	global_logger.log(rados_io_ops, "Initialized the cluster handle. (user: \"" + ci.user + "\", cluster: \"" + ci.cluster + "\")");

	if ((ret = cluster.conf_read_file("/etc/ceph/ceph.conf")) < 0) {
		cluster.shutdown();
		throw runtime_error("rados_io::rados_io() failed "
				"(couldn't read the Ceph configuration file)");
	}
	global_logger.log(rados_io_ops, "Read a Ceph configuration file.");

	if ((ret = cluster.connect()) < 0) {
		cluster.shutdown();
		throw runtime_error("rados_io::rados_io() failed "
				"(couldn't connect to cluster)");
	}
	global_logger.log(rados_io_ops, "Connected to the cluster.");

	if ((ret = cluster.ioctx_create(pool.c_str(), ioctx)) < 0) {
		cluster.shutdown();
		throw runtime_error("rados_io::rados_io() failed "
				"(couldn't set up ioctx)");
	}
	global_logger.log(rados_io_ops, "Created an I/O context. "
			"(pool: \"" + pool + "\")");
}

rados_io::~rados_io(void)
{
	ioctx.close();
	global_logger.log(rados_io_ops, "Closed the connection.");

	cluster.shutdown();
	global_logger.log(rados_io_ops, "Shut down the handle.");
}

size_t rados_io::read(enum obj_category category, const string &key, char *value, size_t len, off_t offset)
{
	global_logger.log(rados_io_ops, "Called rados_io::read()");
	global_logger.log(rados_io_ops, "key : " + key + " length : " + std::to_string(len) + " offset : " + std::to_string(offset));

	string p_key = get_prefix(category) + key;

	off_t cursor = offset;
	off_t stop = offset + len;
	size_t sum = 0;

	while (cursor < stop) {
		uint64_t obj_num = cursor >> OBJ_BITS;
		string obj_key = p_key + get_postfix(obj_num);

		off_t next_bound = (cursor & OBJ_MASK) + OBJ_SIZE;
		size_t sub_len = MIN(next_bound - cursor, stop - cursor);

		try {
			sum += this->read_obj(obj_key,value + sum, sub_len, cursor & (~OBJ_MASK));
		} catch (no_such_object &e) {
			throw no_such_object(e.what(), sum);
		}

		cursor = next_bound;
	}

	return sum;
}

size_t rados_io::write(enum obj_category category, const string &key, const char *value, size_t len, off_t offset)
{
	global_logger.log(rados_io_ops, "Called rados_io::write()");
	global_logger.log(rados_io_ops, "key : " + key + " length : " + std::to_string(len) + " offset : " + std::to_string(offset));

	string p_key = get_prefix(category) + key;

	off_t cursor = offset;
	off_t stop = offset + len;
	size_t sum = 0;

	while (cursor < stop) {
		uint64_t obj_num = cursor >> OBJ_BITS;
		string obj_key = p_key + get_postfix(obj_num);

		off_t next_bound = (cursor & OBJ_MASK) + OBJ_SIZE;
		size_t sub_len = MIN(next_bound - cursor, stop - cursor);

		sum += this->write_obj(obj_key,value + sum, sub_len, cursor & (~OBJ_MASK));

		cursor = next_bound;
	}

	return sum;
}

bool rados_io::exist(enum obj_category category, const string &key)
{
	global_logger.log(rados_io_ops, "Called rados_io::exist()");
	global_logger.log(rados_io_ops, "key : " + key);

	/* It suffices to check if the first object exists. */
	string obj_key = get_prefix(category) + key + get_postfix(0);

	int ret;
	uint64_t size;
	time_t mtime;

	ret = ioctx.stat(obj_key, &size, &mtime);
	if (ret >= 0) {
		global_logger.log(rados_io_ops, "The object with key \""+ key + "\" exists.");
		return true;
	} else if (ret == -ENOENT) {
		global_logger.log(rados_io_ops, "The object with key \"" + key + "\" doesn't exist.");
		return false;
	} else {
		throw runtime_error("rados_io::exist() failed");
	}
}

void rados_io::remove(enum obj_category category, const string &key)
{
	global_logger.log(rados_io_ops, "Called rados_io::remove()");
	global_logger.log(rados_io_ops, "key : " + key);

	string p_key = get_prefix(category) + key;

	int ret;

	for (uint64_t obj_num = 0; ; obj_num++) {
		ret = ioctx.remove(p_key + get_postfix(obj_num));

		if (ret == -ENOENT && obj_num == 0) {
			global_logger.log(rados_io_ops, "Tried to remove a non-existent object. (key: \"" + key + "\")");
			return;
		} else if (ret == -ENOENT) {
			global_logger.log(rados_io_ops, "Removed an object. (key: \"" + key + "\")");
			return;
		} else if (ret < 0) {
			throw runtime_error("rados_io::remove() failed");
		}
	}
}
