#ifndef _FUSE_OPS_HPP_
#define _FUSE_OPS_HPP_

#define FUSE_USE_VERSION 30

#include <fuse.h>

namespace fuse_ops {

void* init(struct fuse_conn_info* info, struct fuse_config *config);
void destroy(void* private_data);
int getattr(const char* path, struct stat* stat, struct fuse_file_info* file_info);
int access(const char* path, int mask);
int opendir(const char* path, struct fuse_file_info* file_info);
int releasedir(const char* path, struct fuse_file_info* file_info);
int readdir(const char* path, void* buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* file_info, enum fuse_readdir_flags readdir_flags);
int mkdir(const char* path, mode_t mode);
int rmdir(const char* path);
int symlink(const char *src, const char *dst);
int rename(const char* old_path, const char* new_path, unsigned int flags);
int open(const char* path, struct fuse_file_info* file_info);
int release(const char* path, struct fuse_file_info* file_info);
int create(const char* path, mode_t mode, struct fuse_file_info* file_info);
int unlink(const char* path);
int read(const char* path, char* buffer, size_t size, off_t offset, struct fuse_file_info* file_info);
int write(const char* path, const char* buffer, size_t size, off_t offset, struct fuse_file_info* file_info);
int chmod(const char* path, mode_t mode, struct fuse_file_info* file_info);
int chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* file_info);
int utimens(const char *, const struct timespec tv[2], struct fuse_file_info *fi);

fuse_operations get_fuse_ops(void);

} /* fuse_ops */

#endif /* _FUSE_OPS_HPP_ */
