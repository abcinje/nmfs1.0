#include "rados_io.hpp"
#include "../logger/logger.hpp"

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static string get_prefix(obj_category category)
{
	switch (category) {
	case obj_category::INODE:
		return "i$";
	case obj_category::DENTRY:
		return "e$";
	case obj_category::DATA:
		return "d$";
	case obj_category::CLIENT:
		return "c$";
	case obj_category::JOURNAL:
		return "j$";
	default:
		throw logic_error("get_prefix() failed (unknown category " + std::to_string(static_cast<int>(category)) + ")");
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

void rados_io::zerofill(obj_category category, const string &key, size_t len, off_t offset)
{
	char *zeros = new char[MIN(len, OBJ_SIZE)]();

	string p_key = get_prefix(category) + key;

	off_t cursor = offset;
	off_t stop = offset + len;
	size_t sum = 0;

	while (cursor < stop) {
		uint64_t obj_num = cursor >> OBJ_BITS;
		string obj_key = p_key + get_postfix(obj_num);

		off_t next_bound = (cursor & OBJ_MASK) + OBJ_SIZE;
		size_t sub_len = MIN(next_bound - cursor, stop - cursor);

		write_obj(obj_key, zeros, sub_len, cursor & (~OBJ_MASK));

		cursor = next_bound;
	}

	delete []zeros;
}

void rados_io::truncate_obj(const string &key, uint64_t cut_size) {
	global_logger.log(rados_io_ops,"Called rados_io::write_obj()");
	global_logger.log(rados_io_ops,"key : " + key + " cut_size : " + std::to_string(cut_size));
	int ret;
	ret = ioctx.trunc(key, cut_size);

	if (ret == 0) {
		global_logger.log(rados_io_ops, "Truncate an object. (key: \"" + key + "\")");
	} else {
		throw runtime_error("rados_io::truncate_obj() failed");
	}
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

size_t rados_io::read(obj_category category, const string &key, char *value, size_t len, off_t offset)
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
			sum += read_obj(obj_key,value + sum, sub_len, cursor & (~OBJ_MASK));
		} catch (no_such_object &e) {
			throw no_such_object(e.what(), sum);
		}

		cursor = next_bound;
	}

	return sum;
}

size_t rados_io::write(obj_category category, const string &key, const char *value, size_t len, off_t offset)
{
	global_logger.log(rados_io_ops, "Called rados_io::write()");
	global_logger.log(rados_io_ops, "key : " + key + " length : " + std::to_string(len) + " offset : " + std::to_string(offset));

	string p_key = get_prefix(category) + key;

	/* Fill the "hole" with zeros if the given offset is greater than the file size.
	   To do this, get the file size and do zerofill via zerofill(). */
	int ret;
	size_t lb_file_size;

	for (int64_t prev_obj_num = offset >> OBJ_BITS; prev_obj_num >= 0; prev_obj_num--) {
		uint64_t size;
		time_t mtime;

		string prev_obj_key = p_key + get_postfix(prev_obj_num);

		ret = ioctx.stat(prev_obj_key, &size, &mtime);
		if (ret >= 0) {
			lb_file_size = (prev_obj_num << OBJ_BITS) + size;
			break;
		} else if (ret != -ENOENT) {
			throw runtime_error("rados_io::write() failed (stat() failed)");
		}
	}

	/* There are no such RADOS objects. */
	if (ret == -ENOENT)
		lb_file_size = 0;

	/* Do zerofill */
	/* If lb_file_size < offset, lb_file_size is equal to the file size */
	if (lb_file_size < offset)
		zerofill(category, key, offset - lb_file_size, lb_file_size);

	/* Now it's time to write. */
	off_t cursor = offset;
	off_t stop = offset + len;
	size_t sum = 0;

	while (cursor < stop) {
		uint64_t obj_num = cursor >> OBJ_BITS;
		string obj_key = p_key + get_postfix(obj_num);

		off_t next_bound = (cursor & OBJ_MASK) + OBJ_SIZE;
		size_t sub_len = MIN(next_bound - cursor, stop - cursor);

		sum += write_obj(obj_key,value + sum, sub_len, cursor & (~OBJ_MASK));

		cursor = next_bound;
	}

	return sum;
}

bool rados_io::exist(obj_category category, const string &key)
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

bool rados_io::stat(obj_category category, const string &key, size_t &size)
{
	global_logger.log(rados_io_ops, "Called rados_io::stat()");
	global_logger.log(rados_io_ops, "key : " + key);

	string p_key = get_prefix(category) + key;

	size = 0;
	int ret;
	uint64_t obj_size;
	time_t mtime;

	/* We need to check all the RADOS objects */
	for (uint64_t obj_num = 0; ; obj_num++) {
		ret = ioctx.stat(p_key + get_postfix(obj_num), &obj_size, &mtime);

		if (ret == -ENOENT && obj_num == 0) {
			global_logger.log(rados_io_ops, "The object with key \"" + key + "\" doesn't exist.");
			return false;
		} else if (ret == -ENOENT) {
			global_logger.log(rados_io_ops, "The object with key \"" + key + "\" has " + std::to_string(size) + " bytes of data.");
			return true;
		} else if (ret < 0) {
			throw runtime_error("rados_io::stat() failed");
		} else {
			size += obj_size;
		}
	}
}

void rados_io::remove(obj_category category, const string &key)
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

int rados_io::truncate(obj_category category, const string &key, size_t offset){
	global_logger.log(rados_io_ops, "Called rados_io::truncate()");
	global_logger.log(rados_io_ops, "key : " + key);

	string p_key = get_prefix(category) + key;

	int ret;
	size_t lb_file_size;

	for (int64_t prev_obj_num = offset >> OBJ_BITS; prev_obj_num >= 0; prev_obj_num--) {
		uint64_t size;
		time_t mtime;

		string prev_obj_key = p_key + get_postfix(prev_obj_num);

		ret = ioctx.stat(prev_obj_key, &size, &mtime);
		if (ret >= 0) {
			lb_file_size = (prev_obj_num << OBJ_BITS) + size;
			break;
		} else if (ret != -ENOENT) {
			throw runtime_error("rados_io::truncate() failed (stat() failed)");
		}
	}

	/* There are no such RADOS objects. */
	if (ret == -ENOENT) {
		zerofill(category, key, offset, 0);
		return 0;
	}

	/* Do zerofill */
	/* If lb_file_size < offset, lb_file_size is equal to the file size */
	if (lb_file_size < offset) {
		zerofill(category, key, offset - lb_file_size, lb_file_size);
		return 0;
	}

	/* Now it's time to truncate. */
	uint64_t obj_num = offset >> OBJ_BITS;
	string obj_key = p_key + get_postfix(obj_num);

	size_t cut_size = (offset - (obj_num << OBJ_BITS));
	truncate_obj(obj_key, cut_size);

	/* if there is remaining object, remove them */
	while(1) {
		uint64_t size;
		time_t mtime;

		obj_num = obj_num + 1;
		string removed_obj_key = p_key + get_postfix(obj_num);

		ret = ioctx.stat(removed_obj_key, &size, &mtime);
		if (ret >= 0) {
			ioctx.remove(removed_obj_key);
		} else if (ret == -ENOENT) {
			break;
		}
	}

	return 0;
}
