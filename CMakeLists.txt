cmake_minimum_required(VERSION 3.0)
project(nmfs0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Release)

find_package(PkgConfig REQUIRED)
find_library(rados librados.so)
pkg_check_modules(FUSE3 REQUIRED fuse3)

add_definitions(-DDEBUG)

add_executable(
	nmfs0

        client/main.cpp
        client/fs_ops/local_ops.cpp
        client/fs_ops/fuse_ops.cpp
        client/fs_ops/remote_ops.cpp
        client/util.cpp

	# logger
        client/logger/logger.cpp

	# meta
        client/meta/inode.cpp
        client/meta/dentry.cpp
        client/meta/file_handler.cpp

	# rados_io
        client/rados_io/rados_io.cpp

	# client
        client/client/client.cpp

        client/lease/lease_util.cpp

        client/in_memory/directory_table.cpp
        client/in_memory/dentry_table.cpp
)

target_link_libraries(nmfs0 ${FUSE3_LIBRARIES} rados)
target_include_directories(nmfs0 PUBLIC ${FUSE3_INCLUDE_DIRS})
target_compile_options(nmfs0 PUBLIC ${FUSE3_CFLAGS_OTHER})
