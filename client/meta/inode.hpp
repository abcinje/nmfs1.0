#ifndef _INODE_HPP_
#define _INODE_HPP_

#include <memory>
#include <sys/stat.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <mutex>
#include <rpc.grpc.pb.h>
#include "../../lib/rados_io/rados_io.hpp"
#include "../../lib/logger/logger.hpp"
#include "../client/client.hpp"
#include "../util.hpp"
#include "uuid_controller.hpp"

#define REG_INODE_SIZE (sizeof(struct _core))
#define DIR_INODE_SIZE 4096
#define ENOTLEADER 8000
#define ENEEDRECOV 8001

using std::unique_ptr;
using std::runtime_error;
using std::string;
using namespace boost::uuids;

enum meta_location {
    LOCAL = 0,
    REMOTE,
    UNKNOWN,
    JOURNAL,
    NOBODY /* temporaly status */
};

class inode {
private:
	uuid p_ino;

	struct _core {
		mode_t i_mode;
		uid_t i_uid;
		gid_t i_gid;
		uuid i_ino;
		nlink_t i_nlink;
		off_t i_size;

		struct timespec i_atime;
		struct timespec i_mtime;
		struct timespec i_ctime;
	} core;

	uint64_t loc;

	uint32_t link_target_len;
	char *link_target_name;

public:
	std::recursive_mutex inode_mutex;
	class no_entry : public runtime_error {
	public:
		explicit no_entry(const string &msg);
		const char *what();
	};

	class permission_denied : public runtime_error {
	public:
		explicit permission_denied(const string &msg);
		const char *what();
	};

	inode(const inode &copy);

	/* for normal reg file and root directory */
	inode(uuid parent_ino, uid_t owner, gid_t group, mode_t mode, bool root = false);
    	/* for normal directory */
    	inode(uuid parent_ino, uid_t owner, gid_t group, mode_t mode, uuid& predefined_ino);
	/* for symlink */
	inode(uuid parent_ino, uid_t owner, gid_t group, mode_t mode, const char *link_target_name);
	/* for pull metadata */
	inode(uuid ino);
	/* parent constructor for remote_inode and dummy_inode which used with file_handler */
	inode(enum meta_location loc);

	~inode();

	void fill_stat(struct stat *s);
	std::vector<char> serialize();
	void deserialize(const char *value);
	void sync();
	virtual void permission_check(int mask);

	// getter
	uuid get_p_ino();
	virtual mode_t get_mode();
	uid_t get_uid();
	gid_t get_gid();
	uuid get_ino();
	nlink_t get_nlink();
	off_t get_size();
	struct timespec get_atime();
	struct timespec get_mtime();
	struct timespec get_ctime();

	uint64_t get_loc();

	uint32_t get_link_target_len();
	char *get_link_target_name();

	// setter
	void set_p_ino(const uuid &p_ino);

	void set_mode(mode_t mode);
	void set_uid(uid_t uid);
	void set_gid(gid_t gid);
	void set_ino(uuid ino);
	void set_nlink(nlink_t nlink);
	void set_size(off_t size);
	void set_atime(struct timespec atime);
	void set_mtime(struct timespec mtime);
	void set_ctime(struct timespec ctime);

	void set_loc(uint64_t loc);
	void set_link_target_len(uint32_t len);
	void set_link_target_name(const char *name);

	void inode_to_rename_src_response(::rpc_rename_not_same_parent_src_respond *response);
    	void rename_src_response_to_inode(::rpc_rename_not_same_parent_src_respond &response);
    	void inode_to_rename_dst_request(::rpc_rename_not_same_parent_dst_request &request);
    	void rename_dst_request_to_inode(const ::rpc_rename_not_same_parent_dst_request *request);
};

uuid alloc_new_ino();

#endif /* _INODE_HPP_ */
