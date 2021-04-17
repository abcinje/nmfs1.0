find_package(PkgConfig REQUIRED)
find_library(rados librados.so)
pkg_check_modules(FUSE3 REQUIRED fuse3)

#add_definitions(-DDEBUG)

add_executable(
	nmfs0

	main.cpp
	fs_ops/local_ops.cpp
	fs_ops/fuse_ops.cpp
	fs_ops/remote_ops.cpp
	util.cpp

	# logger
	logger/logger.cpp

	# meta
	meta/inode.cpp
	meta/dentry.cpp
	meta/file_handler.cpp

	# rados_io
	rados_io/rados_io.cpp

	# client
	client/client.cpp

	temp/lease_util.cpp

	in_memory/directory_table.cpp
	in_memory/dentry_table.cpp
)

target_link_libraries(nmfs0 ${FUSE3_LIBRARIES} rados)
target_include_directories(nmfs0 PUBLIC ${FUSE3_INCLUDE_DIRS})
target_compile_options(nmfs0 PUBLIC ${FUSE3_CFLAGS_OTHER})
