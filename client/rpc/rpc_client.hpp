#ifndef NMFS_RPC_CLIENT_HPP
#define NMFS_RPC_CLIENT_HPP

#include <grpcpp/grpcpp.h>
#include <rpc.grpc.pb.h>
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

	void getattr(shared_ptr<remote_inode> i, struct stat* stat);
	void access(shared_ptr<remote_inode> i, int mask);
	int opendir(shared_ptr<remote_inode> i, struct fuse_file_info* file_info);
	void readdir(shared_ptr<remote_inode> i, void* buffer, fuse_fill_dir_t filler);
	void mkdir(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode);
	int rmdir(shared_ptr<remote_inode> parent_i, shared_ptr<inode> target_i, std::string target_name);
	int symlink(shared_ptr<remote_inode> dst_parent_i, const char *src, const char *dst);
	int readlink(shared_ptr<remote_inode> i, char *buf, size_t size);
	int rename_same_parent(shared_ptr<remote_inode> parent_i, const char* old_path, const char* new_path, unsigned int flags);
	int rename_not_same_parent(shared_ptr<remote_inode> src_parent_i, shared_ptr<inode> dst_parent_i, const char* old_path, const char* new_path, unsigned int flags);
	int open(shared_ptr<remote_inode> i, struct fuse_file_info* file_info);
	void create(shared_ptr<remote_inode> parent_i, std::string new_child_name, mode_t mode, struct fuse_file_info* file_info);
	void unlink(shared_ptr<remote_inode> parent_i, std::string child_name);
	size_t write(shared_ptr<remote_inode>i, const char* buffer, size_t size, off_t offset, int flags);
	void chmod(shared_ptr<remote_inode> i, mode_t mode);
	void chown(shared_ptr<remote_inode> i, uid_t uid, gid_t gid);
	void utimens(shared_ptr<remote_inode> i, const struct timespec tv[2]);
	int truncate(shared_ptr<remote_inode> i, off_t offset);
};


#endif //NMFS_RPC_CLIENT_HPP
