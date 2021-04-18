# Protobuf files
get_filename_component(lease_proto "proto/lease.proto" REALPATH)
get_filename_component(lease_proto_path "${lease_proto}" PATH)

add_custom_command(
  OUTPUT "lease.pb.cc" "lease.pb.h" "lease.grpc.pb.cc" "lease.grpc.pb.h"
  COMMAND ${PROTOBUF_PROTOC}
  ARGS
    --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${lease_proto_path}"
    --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${lease_proto}"
  DEPENDS "${lease_proto}"
)

# Include header files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

find_package(PkgConfig REQUIRED)
find_library(rados librados.so)
pkg_check_modules(FUSE3 REQUIRED fuse3)

add_executable(
  nmfs

  # main
  main.cpp

  # fs_ops
  fs_ops/local_ops.cpp
  fs_ops/fuse_ops.cpp
  fs_ops/remote_ops.cpp

  # util
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

  # temp
  temp/lease_util.cpp

  # in_memory
  in_memory/directory_table.cpp
  in_memory/dentry_table.cpp

  # lease
  "${CMAKE_CURRENT_BINARY_DIR}/lease.pb.cc"
  "${CMAKE_CURRENT_BINARY_DIR}/lease.grpc.pb.cc"
)

target_link_libraries(
  nmfs
  ${REFLECTION}
  ${GRPC_GRPCPP}
  ${PROTOBUF_LIBPROTOBUF}
  ${FUSE3_LIBRARIES}
  rados
)
target_include_directories(nmfs PUBLIC ${FUSE3_INCLUDE_DIRS})
target_compile_options(nmfs PUBLIC ${FUSE3_CFLAGS_OTHER})
