cmake_minimum_required(VERSION 3.0)
project(nmfs0)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
find_library(rados librados.so)
pkg_check_modules(FUSE3 REQUIRED fuse3)

add_executable(
	nmfs0

	main.cpp
	fuse_ops.cpp

	# logger
	logger/logger.cpp

	# meta
	meta/inode.cpp

	# rados_io
	rados_io/rados_io.cpp

	# client
	client/client.cpp meta/dentry.cpp meta/dentry.hpp meta/file_handler.cpp meta/file_handler.hpp util.cpp util.hpp)

target_link_libraries(nmfs0 ${FUSE3_LIBRARIES} rados)
target_include_directories(nmfs0 PUBLIC ${FUSE3_INCLUDE_DIRS})
target_compile_options(nmfs0 PUBLIC ${FUSE3_CFLAGS_OTHER})
