# Protobuf files
get_filename_component(manager_proto "proto/manager.proto" REALPATH)
get_filename_component(manager_proto_path "${manager_proto}" PATH)

add_custom_command(
  OUTPUT "manager.pb.cc" "manager.pb.h" "manager.grpc.pb.cc" "manager.grpc.pb.h"
  COMMAND ${PROTOBUF_PROTOC}
  ARGS
    --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${manager_proto_path}"
    --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${manager_proto}"
  DEPENDS "${manager_proto}"
)

# Include header files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

find_package(PkgConfig REQUIRED)
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

  # meta
  meta/inode.cpp
  meta/dentry.cpp
  meta/file_handler.cpp

  # client
  client/client.cpp

  # temp
  temp/lease_util.cpp

  # in_memory
  in_memory/directory_table.cpp
  in_memory/dentry_table.cpp

  # manager protobuf
  "${CMAKE_CURRENT_BINARY_DIR}/manager.pb.cc"
  "${CMAKE_CURRENT_BINARY_DIR}/manager.grpc.pb.cc"

  # lease
  manager/lease_table.cpp
  manager/manager_client.cpp
)

target_link_libraries(
  nmfs
  log
  rio
  ${REFLECTION}
  ${GRPC_GRPCPP}
  ${PROTOBUF_LIBPROTOBUF}
  ${FUSE3_LIBRARIES}
)
target_include_directories(nmfs PUBLIC ${FUSE3_INCLUDE_DIRS})
target_compile_options(nmfs PUBLIC ${FUSE3_CFLAGS_OTHER})
