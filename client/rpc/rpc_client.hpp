#ifndef NMFS_RPC_CLIENT_HPP
#define NMFS_RPC_CLIENT_HPP

#include <grpcpp/grpcpp.h>
#include <rpc.grpc.pb.h>
#include "../meta/dentry.hpp"
#include "../meta/remote_inode.hpp"
#include "../meta/file_handler.hpp"


using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::ClientReader;

using std::shared_ptr;

class rpc_client {
private:
	std::unique_ptr<remote_ops::Stub> stub_;

public:
	rpc_client(std::shared_ptr<Channel> channel);

	/* dentry_table operations */
	uuid check_child_inode(uuid dentry_table_ino, std::string filename);

	/* inode operations */
	mode_t get_mode(uuid dentry_table_ino, std::string filename);
	void permission_check(uuid dentry_table_ino, std::string filename, int mask, bool target_is_parent);
	/* file system operations */
	int getattr(shared_ptr<remote_inode> i, struct stat* s);
	int access(shared_ptr<remote_inode> i, int mask);
	int opendir(shared_ptr<remote_inode> i, struct fuse_file_info* file_info);
	int readdir(shared_ptr<remote_inode> i, void* buffer, fuse_fill_dir_t filler);
	int mkdir(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, std::shared_ptr<inode>& new_dir_inode);
	int rmdir_top(shared_ptr<remote_inode> target_i, uuid target_ino);
	int rmdir_down(shared_ptr<remote_inode> parent_i, uuid target_ino, std::string target_name);
	int symlink(shared_ptr<remote_inode> dst_parent_i, const char *src, const char *dst);
	int readlink(shared_ptr<remote_inode> i, char *buf, size_t size);
	int rename_same_parent(shared_ptr<remote_inode> parent_i, const char* old_path, const char* new_path, unsigned int flags);
	int rename_not_same_parent_src(shared_ptr<remote_inode> src_parent_i, const char* old_path, unsigned int flags, std::shared_ptr<inode>& target_inode);
	int rename_not_same_parent_dst(shared_ptr<remote_inode> dst_parent_i, std::shared_ptr<inode>& target_inode, uuid check_dst_ino, const char* new_path, unsigned int flags);
	int open(shared_ptr<remote_inode> i, struct fuse_file_info* file_info);
	int create(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info* file_info);
	int unlink(shared_ptr<remote_inode> parent_i, std::string child_name);
	ssize_t write(shared_ptr<remote_inode>i, const char* buffer, size_t size, off_t offset, int flags);
	int chmod(shared_ptr<remote_inode> i, mode_t mode);
	int chown(shared_ptr<remote_inode> i, uid_t uid, gid_t gid);
	int utimens(shared_ptr<remote_inode> i, const struct timespec tv[2]);
	int truncate(shared_ptr<remote_inode> i, off_t offset);
};


#endif //NMFS_RPC_CLIENT_HPP
