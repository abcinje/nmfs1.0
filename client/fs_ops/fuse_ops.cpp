#include "fuse_ops.hpp"
#include "../in_memory/directory_table.hpp"
#include "local_ops.hpp"
#include "remote_ops.hpp"
#include "../rpc/rpc_server.hpp"
#include "../journal/journal.hpp"
#include <cstring>
#include <mutex>
#include <thread>

using namespace std;
#define META_POOL "nmfs.meta"
#define DATA_POOL "nmfs.data"

std::shared_ptr<rados_io> meta_pool;
std::shared_ptr<rados_io> data_pool;

std::unique_ptr<Server> remote_handle;
std::shared_ptr<lease_client> lc;

std::unique_ptr<directory_table> indexing_table;
std::unique_ptr<uuid_controller> ino_controller;
std::unique_ptr<file_handler_list> open_context;
std::unique_ptr<journal> journalctl;

std::unique_ptr<thread> remote_server_thread;

std::unique_ptr<client> this_client;
unsigned int fuse_capable;

void *fuse_ops::init(struct fuse_conn_info *info, struct fuse_config *config) {
	global_logger.log(fuse_op, "Called init()");

	/* argument parsing */
	fuse_context *fuse_ctx = fuse_get_context();
	std::string arg((char *) fuse_ctx->private_data);
	size_t dot_pos = arg.find(',');
	std::string manager_ip = arg.substr(0, dot_pos);
	std::string remote_handle_ip = arg.substr(dot_pos + 1);
	global_logger.log(fuse_op, "manager IP: " + manager_ip + " remote_handle IP: " + remote_handle_ip);

	rados_io::conn_info ci = {"client.admin", "ceph", 0};
	meta_pool = std::make_shared<rados_io>(ci, META_POOL);
	data_pool = std::make_shared<rados_io>(ci, DATA_POOL);

	auto channel = grpc::CreateChannel(manager_ip, grpc::InsecureChannelCredentials());
	lc = std::make_shared<lease_client>(channel, remote_handle_ip);
	this_client = std::make_unique<client>(channel);
	this_client->set_client_uid(fuse_ctx->uid);
	this_client->set_client_gid(fuse_ctx->gid);

	global_logger.log(fuse_op, "Client(ID=" + std::to_string(this_client->get_client_id()) + ") is mounted");

	/* root */
	if (!meta_pool->exist(obj_category::INODE, uuid_to_string(get_root_ino()))) {
		inode i(get_root_ino(), this_client->get_client_uid(), this_client->get_client_gid(), S_IFDIR | 0777, true);

		dentry d(get_root_ino(), true);

		i.set_size(DIR_INODE_SIZE);
		i.sync();
		d.sync();
	}

	indexing_table = std::make_unique<directory_table>();
	ino_controller = std::make_unique<uuid_controller>();
	open_context = std::make_unique<file_handler_list>();
	journalctl = std::make_unique<journal>(meta_pool, lc);

	config->nullpath_ok = 0;
	fuse_capable = info->capable;

	remote_server_thread = std::make_unique<thread>(run_rpc_server, remote_handle_ip);
	return nullptr;
}

void fuse_ops::destroy(void *private_data) {
	global_logger.log(fuse_op, "Called destroy()");

	remote_handle->Shutdown();
}

int fuse_ops::getattr(const char *path, struct stat *stat, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called getattr()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	int ret = 0;
	try {
		while(true) {
			shared_ptr<inode> i;
			if (file_info) {
				shared_ptr<file_handler> handler = open_context->get_file_handler(file_info->fh);
				/* TODO : treat open file differently */
				i = handler->get_open_inode_info();
			} else {
				i = indexing_table->path_traversal(path);
			}

			if (i->get_loc() == LOCAL) {
				ret = local_getattr(i, stat);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (i->get_loc() == REMOTE) {
				ret = remote_getattr(std::dynamic_pointer_cast<remote_inode>(i), stat);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return ret;
}

int fuse_ops::access(const char *path, int mask) {
	global_logger.log(fuse_op, "Called access()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	int ret = 0;
	try {
		while(true) {
			shared_ptr<inode> i = indexing_table->path_traversal(path);

			if (i->get_loc() == LOCAL) {
				ret = local_access(i, mask);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (i->get_loc() == REMOTE) {
				ret = remote_access(std::dynamic_pointer_cast<remote_inode>(i), mask);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		/* from inode constructor */
		return -EACCES;
	}

	return ret;
}

int fuse_ops::symlink(const char *src, const char *dst) {
	global_logger.log(fuse_op, "Called symlink()");
	global_logger.log(fuse_op, "src : " + std::string(src) + " dst : " + std::string(dst));

	int ret = 0;
	try {
		while(true) {
			unique_ptr<std::string> dst_parent_name = get_parent_dir_path(dst);
			shared_ptr<inode> dst_parent_i = indexing_table->path_traversal(*dst_parent_name);
			shared_ptr<dentry_table> dst_parent_dentry_table = indexing_table->get_dentry_table(dst_parent_i->get_ino());

			if (dst_parent_dentry_table->get_loc() == LOCAL) {
				ret = local_symlink(dst_parent_i, src, dst);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (dst_parent_dentry_table->get_loc() == REMOTE) {
				shared_ptr<remote_inode> remote_i = std::make_shared<remote_inode>(dst_parent_dentry_table->get_leader_ip(), dst_parent_dentry_table->get_dir_ino(), *dst_parent_name);
				ret = remote_symlink(remote_i, src, dst);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return ret;
}

int fuse_ops::readlink(const char *path, char *buf, size_t size) {
	global_logger.log(fuse_op, "Called readlink()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	int ret = 0;
	try {
		while(true) {
			shared_ptr<inode> i = indexing_table->path_traversal(path);

			if (i->get_loc() == LOCAL) {
				ret = local_readlink(i, buf, size);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (i->get_loc() == REMOTE) {
				ret = remote_readlink(std::dynamic_pointer_cast<remote_inode>(i), buf, size);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return ret;
}

int fuse_ops::opendir(const char *path, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called opendir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	int ret = 0;
	try {
		while(true) {
			shared_ptr<inode> i = indexing_table->path_traversal(path);

			if (i->get_loc() == LOCAL) {
				ret = local_opendir(i, file_info);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (i->get_loc() == REMOTE) {
				ret = remote_opendir(std::dynamic_pointer_cast<remote_inode>(i), file_info);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return ret;
}

int fuse_ops::releasedir(const char *path, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called releasedir()");
	if (path != nullptr) {
		global_logger.log(fuse_op, "path : " + std::string(path));
	} else {
		global_logger.log(fuse_op, "path : nullpath");
	}

	int ret = 0;
	shared_ptr<inode> i;
	if(file_info){
		shared_ptr<file_handler> handler = open_context->get_file_handler(file_info->fh);
		i = handler->get_open_inode_info();
	} else {
		i = indexing_table->path_traversal(path);
	}
	ret = local_releasedir(i, file_info);

	return ret;
}

int fuse_ops::readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset,
		      struct fuse_file_info *file_info, enum fuse_readdir_flags readdir_flags) {
	global_logger.log(fuse_op, "Called readdir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	int ret = 0;
	while(true) {
		shared_ptr<inode> i;
		if (file_info) {
			shared_ptr<file_handler> handler = open_context->get_file_handler(file_info->fh);
			i = handler->get_open_inode_info();
		} else {
			i = indexing_table->path_traversal(path);
		}
		shared_ptr<dentry_table> target_dentry_table = indexing_table->get_dentry_table(i->get_ino());

		if (target_dentry_table->get_loc() == LOCAL) {
			ret = local_readdir(i, buffer, filler);
			if (ret == -ERETRAV) {
				continue;
			} else
				break;
		} else if (target_dentry_table->get_loc() == REMOTE) {
			shared_ptr<remote_inode> remote_i = std::make_shared<remote_inode>(target_dentry_table->get_leader_ip(), target_dentry_table->get_dir_ino(), *(get_filename_from_path(path)));
			ret = remote_readdir(remote_i, buffer, filler);
			if (ret == -ENOTLEADER) {
				continue;
			} else if (ret == -ENEEDRECOV) {
				throw std::runtime_error("Need Recovery of remote dentry_table");
			} else
				break;
		}
	}
	return ret;
}

int fuse_ops::mkdir(const char *path, mode_t mode) {
	global_logger.log(fuse_op, "Called mkdir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	std::shared_ptr<inode> new_dir_inode;
	std::shared_ptr<dentry> new_dir_dentry;
	int ret = 0;
	try {
		while(true) {
			std::unique_ptr<std::string> target_name = get_filename_from_path(path);

			shared_ptr<inode> parent_i = indexing_table->path_traversal(*(get_parent_dir_path(path).get()));
			shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());

			if (parent_dentry_table->get_loc() == LOCAL) {
				ret = local_mkdir(parent_i, *target_name, mode, new_dir_inode, new_dir_dentry);
				if (ret == -ERETRAV) {
					continue;
				} else {
					indexing_table->lease_dentry_table_mkdir(new_dir_inode, new_dir_dentry);
					break;
				}
			} else if (parent_dentry_table->get_loc() == REMOTE) {
				shared_ptr<remote_inode> remote_i = std::make_shared<remote_inode>(parent_dentry_table->get_leader_ip(), parent_dentry_table->get_dir_ino(), *target_name);
				ret = remote_mkdir(remote_i, *target_name, mode, new_dir_inode, new_dir_dentry);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else {
					indexing_table->lease_dentry_table_mkdir(new_dir_inode, new_dir_dentry);
					break;
				}
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::rmdir(const char *path) {
	global_logger.log(fuse_op, "Called rmdir()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	int ret = 0;
	try {
		while(true) {
			std::unique_ptr<std::string> target_name = get_filename_from_path(path);

			shared_ptr<inode> parent_i = indexing_table->path_traversal(*(get_parent_dir_path(path).get()));
			shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());

			uuid target_ino = parent_dentry_table->check_child_inode(*target_name);
			shared_ptr<inode> target_i = parent_dentry_table->get_child_inode(*(get_filename_from_path(path).get()));
			if (!S_ISDIR(target_i->get_mode()))
				return -ENOTDIR;

			shared_ptr<dentry_table> target_dentry_table = indexing_table->get_dentry_table(target_ino);

			if ((parent_dentry_table->get_loc() == LOCAL) && (target_dentry_table->get_loc() == LOCAL)) {
				ret = local_rmdir_top(target_i, target_ino);
				if (ret == -ERETRAV) {
					continue;
				}

				ret = local_rmdir_down(parent_i, target_ino, *target_name);
				if (ret == -ERETRAV) {
					continue;
				} else {
					/* TODO : down might fail and restart from rmdir_down */
					break;
				}
			} else if ((parent_dentry_table->get_loc() == LOCAL) && (target_dentry_table->get_loc() == REMOTE)) {
				shared_ptr<remote_inode> target_remote_i = std::make_shared<remote_inode>(target_dentry_table->get_leader_ip(), target_dentry_table->get_dir_ino(), *target_name);
				ret = remote_rmdir_top(target_remote_i, target_ino);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				}

				ret = local_rmdir_down(parent_i, target_ino, *target_name);
				if (ret == -ERETRAV) {
					continue;
				} else {
					/* TODO : down might fail and restart from rmdir_down */
					break;
				}
			} else if ((parent_dentry_table->get_loc() == REMOTE) && (target_dentry_table->get_loc() == LOCAL)) {
				ret = local_rmdir_top(target_i, target_ino);
				if (ret == -ERETRAV) {
					continue;
				}

				shared_ptr<remote_inode> parent_remote_i = std::make_shared<remote_inode>(parent_dentry_table->get_leader_ip(), parent_dentry_table->get_dir_ino(), *target_name);
				ret = remote_rmdir_down(parent_remote_i, target_ino, *target_name);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				}else {
					/* TODO : down might fail and restart from rmdir_down */
					break;
				}
			} else if ((parent_dentry_table->get_loc() == REMOTE) && (target_dentry_table->get_loc() == REMOTE)) {
				shared_ptr<remote_inode> target_remote_i = std::make_shared<remote_inode>(target_dentry_table->get_leader_ip(), target_dentry_table->get_dir_ino(), *target_name);
				ret = remote_rmdir_top(target_remote_i, target_ino);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				}

				shared_ptr<remote_inode> parent_remote_i = std::make_shared<remote_inode>(parent_dentry_table->get_leader_ip(), parent_dentry_table->get_dir_ino(), *target_name);
				ret = remote_rmdir_down(parent_remote_i, target_ino, *target_name);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				}else {
					/* TODO : down might fail and restart from rmdir_down */
					break;
				}
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return ret;
}

int fuse_ops::rename(const char *old_path, const char *new_path, unsigned int flags) {
	global_logger.log(fuse_op, "Called rename()");
	global_logger.log(fuse_op, "src : " + std::string(old_path) + " dst : " + std::string(new_path));

	if (std::string(old_path) == std::string(new_path))
		return -EEXIST;

	int ret = 0;
	try {
		while(true) {
			unique_ptr<std::string> src_parent_path = get_parent_dir_path(old_path);
			unique_ptr<std::string> dst_parent_path = get_parent_dir_path(new_path);

			if (*src_parent_path == *dst_parent_path) {
				shared_ptr<inode> parent_i = indexing_table->path_traversal(*src_parent_path);
				shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());

				if (parent_dentry_table->get_loc() == LOCAL) {
					ret = local_rename_same_parent(parent_i, old_path, new_path, flags);
					if (ret == -ERETRAV) {
						continue;
					} else {
						break;
					}
				} else if (parent_dentry_table->get_loc() == REMOTE) {
					shared_ptr<remote_inode> remote_i = std::make_shared<remote_inode>(parent_dentry_table->get_leader_ip(), parent_dentry_table->get_dir_ino(), new_path);
					while (true) {
						ret = remote_rename_same_parent(remote_i, old_path, new_path, flags);
						if (ret == -ENOTLEADER) {
							continue;
						} else if (ret == -ENEEDRECOV) {
							throw std::runtime_error("Need Recovery of remote dentry_table");
						} else
							break;
					}
				}
			} else {
				shared_ptr<inode> src_parent_i = indexing_table->path_traversal(*src_parent_path);
				shared_ptr<dentry_table> src_dentry_table = indexing_table->get_dentry_table(src_parent_i->get_ino());

				shared_ptr<inode> dst_parent_i = indexing_table->path_traversal(*dst_parent_path);
				shared_ptr<dentry_table> dst_dentry_table = indexing_table->get_dentry_table(dst_parent_i->get_ino());

				uuid check_dst_ino = dst_dentry_table->check_child_inode(*get_filename_from_path(new_path));
				std::shared_ptr<inode> target_inode;

				if ((src_dentry_table->get_loc() == LOCAL) && (dst_dentry_table->get_loc() == LOCAL)) {
					ret = local_rename_not_same_parent_src(src_parent_i, old_path, flags, target_inode);
					if (ret == -ERETRAV) {
						continue;
					}

					ret = local_rename_not_same_parent_dst(dst_parent_i, target_inode,check_dst_ino, new_path, flags);
					if (ret == -ERETRAV) {
						continue;
					} else {
						/* TODO : dst might fail and restart from rename_src */
						break;
					}
				} else if ((src_dentry_table->get_loc() == LOCAL) && (dst_dentry_table->get_loc() == REMOTE)) {
					ret = local_rename_not_same_parent_src(src_parent_i, old_path, flags, target_inode);
					if (ret == -ERETRAV) {
						continue;
					}

					shared_ptr<remote_inode> dst_remote_i = std::make_shared<remote_inode>(dst_dentry_table->get_leader_ip(), dst_dentry_table->get_dir_ino(), new_path);
					ret = remote_rename_not_same_parent_dst(dst_remote_i, target_inode,check_dst_ino, new_path, flags);
					if (ret == -ENOTLEADER) {
						continue;
					} else if (ret == -ENEEDRECOV) {
						throw std::runtime_error("Need Recovery of remote dentry_table");
					} else {
						/* dst might fail and restart from rename_src */
						break;
					}
				} else if ((src_dentry_table->get_loc() == REMOTE) && (dst_dentry_table->get_loc() == LOCAL)) {
					shared_ptr<remote_inode> src_remote_i = std::make_shared<remote_inode>(src_dentry_table->get_leader_ip(), src_dentry_table->get_dir_ino(), old_path);
					ret = remote_rename_not_same_parent_src(src_remote_i, old_path, flags,target_inode);
					if (ret == -ENOTLEADER) {
						continue;
					} else if (ret == -ENEEDRECOV) {
						throw std::runtime_error("Need Recovery of remote dentry_table");
					}

					ret = local_rename_not_same_parent_dst(dst_parent_i, target_inode, check_dst_ino, new_path, flags);
					if (ret == -ERETRAV) {
						continue;
					} else {
						/* dst might fail and restart from rename_src */
						break;
					}
				} else if ((src_dentry_table->get_loc() == REMOTE) && (dst_dentry_table->get_loc() == REMOTE)) {
					shared_ptr<remote_inode> src_remote_i = std::make_shared<remote_inode>(src_dentry_table->get_leader_ip(), src_dentry_table->get_dir_ino(), old_path);
					ret = remote_rename_not_same_parent_src(src_remote_i, old_path, flags, target_inode);
					if (ret == -ENOTLEADER) {
						continue;
					} else if (ret == -ENEEDRECOV) {
						throw std::runtime_error("Need Recovery of remote dentry_table");
					}

					shared_ptr<remote_inode> dst_remote_i = std::make_shared<remote_inode>(dst_dentry_table->get_leader_ip(), dst_dentry_table->get_dir_ino(), new_path);
					ret = remote_rename_not_same_parent_dst(dst_remote_i, target_inode, check_dst_ino, new_path, flags);
					if (ret == -ENOTLEADER) {
						continue;
					} else if (ret == -ENEEDRECOV) {
						throw std::runtime_error("Need Recovery of remote dentry_table");
					} else {
						/* dst might fail and restart from rename_src */
						break;
					}
				}

			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	} catch (std::runtime_error &e) {
		return -ENOSYS;
	}

	return ret;
}

/* file creation flags */

// O_CLOEXEC	- Unimplemented
// O_CREAT	- Handled by the kernel
// O_DIRECTORY	- Handled in this function
// O_EXCL	- Handled by the kernel
// O_NOCTTY	- Handled by the kernel
// O_NOFOLLOW	- Handled in this function
// O_TMPFILE	- Ignored
// O_TRUNC	- Handled in this function

/* file status flags */

// O_APPEND	- Handled in write() (Assume that writeback caching is disabled)
// O_ASYNC	- Ignored
// O_DIRECT	- Ignored
// O_DSYNC	- To be implemented
// O_LARGEFILE	- Ignored
// O_NOATIME	- Ignored
// O_NONBLOCK	- Ignored
// O_PATH	- Unimplemented
// O_SYNC	- To be implemented


int fuse_ops::open(const char *path, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called open()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	/* flags which are unimplemented and to be implemented */

	if (file_info->flags & O_CLOEXEC) throw std::runtime_error("O_CLOEXEC is ON");
	if (file_info->flags & O_PATH) throw std::runtime_error("O_PATH is ON");

	int ret = 0;
	try {
		while(true) {
			shared_ptr<inode> i = indexing_table->path_traversal(path);

			if (i->get_loc() == LOCAL) {
				ret = local_open(i, file_info);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (i->get_loc() == REMOTE) {
				ret = remote_open(std::dynamic_pointer_cast<remote_inode>(i), file_info);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return ret;
}

int fuse_ops::release(const char *path, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called release()");
	if (path != nullptr) {
		global_logger.log(fuse_op, "path : " + std::string(path));
	} else {
		global_logger.log(fuse_op, "path : nullpath");
	}

	int ret = 0;
	shared_ptr<inode> i;
	if(file_info){
		shared_ptr<file_handler> handler = open_context->get_file_handler(file_info->fh);
		i = handler->get_open_inode_info();
	} else {
		i = indexing_table->path_traversal(path);
	}

	ret = local_release(i, file_info);

	return ret;
}

int fuse_ops::create(const char *path, mode_t mode, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called create()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	if (S_ISDIR(mode))
		return -EISDIR;
	int ret = 0;
	try {
		while(true) {
			std::unique_ptr<std::string> target_name = get_filename_from_path(path);
			shared_ptr<inode> parent_i = indexing_table->path_traversal(*(get_parent_dir_path(path).get()));
			shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());

			if (parent_dentry_table->get_loc() == LOCAL) {
				ret = local_create(parent_i, *target_name, mode, file_info);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (parent_dentry_table->get_loc() == REMOTE) {
				shared_ptr<remote_inode> remote_i = std::make_shared<remote_inode>(parent_dentry_table->get_leader_ip(), parent_dentry_table->get_dir_ino(), *target_name);
				ret = remote_create(remote_i, *target_name, mode, file_info);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return ret;
}

int fuse_ops::unlink(const char *path) {
	global_logger.log(fuse_op, "Called unlink()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	int ret = 0;
	try {
		while(true) {
			std::unique_ptr<std::string> target_name = get_filename_from_path(path);
			shared_ptr<inode> parent_i = indexing_table->path_traversal(*(get_parent_dir_path(path).get()));
			shared_ptr<dentry_table> parent_dentry_table = indexing_table->get_dentry_table(parent_i->get_ino());

			if (parent_dentry_table->get_loc() == LOCAL) {
				ret = local_unlink(parent_i, *target_name);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (parent_dentry_table->get_loc() == REMOTE) {
				shared_ptr<remote_inode> remote_i = std::make_shared<remote_inode>(parent_dentry_table->get_leader_ip(), parent_dentry_table->get_dir_ino(), *target_name);
				ret = remote_unlink(remote_i, *target_name);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}
	return ret;
}

int fuse_ops::read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called read()");
	global_logger.log(fuse_op, "path : " + std::string(path) + " size : " + std::to_string(size) + " offset : " +
				   std::to_string(offset));

	ssize_t read_len = 0;
	try {
		shared_ptr<inode> i;
		if(file_info){
			shared_ptr<file_handler> handler = open_context->get_file_handler(file_info->fh);
			i = handler->get_open_inode_info();
		} else {
			i = indexing_table->path_traversal(path);
		}

		read_len = local_read(i, buffer, size, offset);
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	} catch (rados_io::no_such_object &e) {
		return (int) e.num_bytes;
	}

	return (int) read_len;
}

int fuse_ops::write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called write()");
	global_logger.log(fuse_op, "path : " + std::string(path) + " size : " + std::to_string(size) + " offset : " +
				   std::to_string(offset));

	ssize_t written_len = 0;
	try {
		while(true) {
			shared_ptr<inode> i;
			if (file_info) {
				shared_ptr<file_handler> handler = open_context->get_file_handler(file_info->fh);
				i = handler->get_open_inode_info();
			} else {
				i = indexing_table->path_traversal(path);
			}

			if (i->get_loc() == LOCAL) {
				written_len = local_write(i, buffer, size, offset, file_info->flags);
				if (written_len == -ERETRAV) {
					continue;
				} else
					break;
			} else if (i->get_loc() == REMOTE) {
				written_len = remote_write(std::dynamic_pointer_cast<remote_inode>(i), buffer, size,
							   offset, file_info->flags);
				if (written_len == -ENOTLEADER) {
					continue;
				} else if (written_len == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return (int) written_len;
}

int fuse_ops::chmod(const char *path, mode_t mode, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called chmod()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	int ret = 0;
	try {
		while(true) {
			shared_ptr<inode> i;
			if (file_info) {
				shared_ptr<file_handler> handler = open_context->get_file_handler(file_info->fh);
				i = handler->get_open_inode_info();
			} else {
				i = indexing_table->path_traversal(path);
			}

			if (i->get_loc() == LOCAL) {
				ret = local_chmod(i, mode);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (i->get_loc() == REMOTE) {
				ret = remote_chmod(std::dynamic_pointer_cast<remote_inode>(i), mode);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return ret;
}

int fuse_ops::chown(const char *path, uid_t uid, gid_t gid, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called chown()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	int ret = 0;
	try {
		while(true) {
			shared_ptr<inode> i;
			if (file_info) {
				shared_ptr<file_handler> handler = open_context->get_file_handler(file_info->fh);
				i = handler->get_open_inode_info();
			} else {
				i = indexing_table->path_traversal(path);
			}

			if (i->get_loc() == LOCAL) {
				ret = local_chown(i, uid, gid);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (i->get_loc() == REMOTE) {
				ret = remote_chown(std::dynamic_pointer_cast<remote_inode>(i), uid, gid);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called utimens()");
	global_logger.log(fuse_op, "path : " + std::string(path));

	int ret = 0;
	try {
		while(true) {
			shared_ptr<inode> i;
			if (file_info) {
				shared_ptr<file_handler> handler = open_context->get_file_handler(file_info->fh);
				i = handler->get_open_inode_info();
			} else {
				i = indexing_table->path_traversal(path);
			}

			if (i->get_loc() == LOCAL) {
				ret = local_utimens(i, tv);
				if (ret == -ERETRAV) {
					continue;
				} else
					break;
			} else if (i->get_loc() == REMOTE) {
				ret = remote_utimens(std::dynamic_pointer_cast<remote_inode>(i), tv);
				if (ret == -ENOTLEADER) {
					continue;
				} else if (ret == -ENEEDRECOV) {
					throw std::runtime_error("Need Recovery of remote dentry_table");
				} else
					break;
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return 0;
}

int fuse_ops::truncate(const char *path, off_t offset, struct fuse_file_info *file_info) {
	global_logger.log(fuse_op, "Called truncate()");
	global_logger.log(fuse_op, "path : " + std::string(path) + " offset : " + std::to_string(offset));

	int ret = 0;
	try {
		while(true) {
			shared_ptr<inode> i;
			if (file_info) {
				shared_ptr<file_handler> handler = open_context->get_file_handler(file_info->fh);
				i = handler->get_open_inode_info();
			} else {
				i = indexing_table->path_traversal(path);
			}

			if (i->get_loc() == LOCAL) {
				while (true) {
					ret = local_truncate(i, offset);
					if (ret == -ERETRAV) {
						continue;
					} else
						break;
				}
			} else if (i->get_loc() == REMOTE) {
				while (true) {
					ret = remote_truncate(std::dynamic_pointer_cast<remote_inode>(i), offset);
					if (ret == -ENOTLEADER) {
						continue;
					} else if (ret == -ENEEDRECOV) {
						throw std::runtime_error("Need Recovery of remote dentry_table");
					} else
						break;
				}
			}
		}
	} catch (inode::no_entry &e) {
		return -ENOENT;
	} catch (inode::permission_denied &e) {
		return -EACCES;
	}

	return ret;
}

fuse_operations fuse_ops::get_fuse_ops(void) {
	fuse_operations fops;
	memset(&fops, 0, sizeof(fuse_operations));

	fops.init = init;
	fops.destroy = destroy;
	fops.getattr = getattr;
	fops.access = access;
	fops.symlink = symlink;
	fops.readlink = readlink;

	fops.opendir = opendir;
	fops.releasedir = releasedir;

	fops.readdir = readdir;
	fops.mkdir = mkdir;
	fops.rmdir = rmdir;
	fops.rename = rename;

	fops.open = open;
	fops.release = release;

	fops.create = create;
	fops.unlink = unlink;

	fops.read = read;
	fops.write = write;

	fops.chmod = chmod;
	fops.chown = chown;
	fops.utimens = utimens;

	fops.truncate = truncate;
	return fops;
}
