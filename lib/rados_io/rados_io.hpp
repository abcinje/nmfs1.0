#ifndef _RADOS_IO_HPP_
#define _RADOS_IO_HPP_

#include <stdexcept>
#include <string>
#include <rados/librados.hpp>

using std::logic_error;
using std::runtime_error;
using std::string;

#define OBJ_SIZE	(4194304)
#define OBJ_BITS	(22)
#define OBJ_MASK	((~0) << OBJ_BITS)

enum class obj_category {
	INODE,
	DENTRY,
	DATA,
	CLIENT,
};

class rados_io {
private:
	librados::Rados cluster;
	librados::IoCtx ioctx;

	size_t read_obj(const string &key, char *value, size_t len, off_t offset);
	size_t write_obj(const string &key, const char *value, size_t len, off_t offset);
	void zerofill(obj_category category, const string &key, size_t len, off_t offset);
	void truncate_obj(const string &key, uint64_t cut_size);

public:
	class no_such_object : public runtime_error {
	public:
		const size_t num_bytes;

		no_such_object(const string &msg, size_t nb = 0);
		const char *what(void);
	};

	struct conn_info {
		string user;
		string cluster;
		int64_t flags;
	};

	rados_io(const conn_info &ci, string pool);
	~rados_io(void);

	size_t read(obj_category category, const string &key, char *value, size_t len, off_t offset);
	size_t write(obj_category category, const string &key, const char *value, size_t len, off_t offset);
	bool exist(obj_category category, const string &key);
	bool stat(obj_category category, const string &key, size_t &size);
	void remove(obj_category category, const string &key);
	int truncate(obj_category category, const string &key, size_t offset);
};

#endif /* _RADOS_IO_HPP_ */
