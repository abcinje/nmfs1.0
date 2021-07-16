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

get_filename_component(session_proto "proto/session.proto" REALPATH)
get_filename_component(session_proto_path "${session_proto}" PATH)

add_custom_command(
  OUTPUT "session.pb.cc" "session.pb.h" "session.grpc.pb.cc" "session.grpc.pb.h"
  COMMAND ${PROTOBUF_PROTOC}
  ARGS
    --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${session_proto_path}"
    --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${session_proto}"
  DEPENDS "${session_proto}"
)

get_filename_component(rpc_proto "proto/rpc.proto" ABSOLUTE)
get_filename_component(rpc_proto_path "${rpc_proto}" PATH)

add_custom_command(
  OUTPUT "rpc.pb.cc" "rpc.pb.h" "rpc.grpc.pb.cc" "rpc.grpc.pb.h"
  COMMAND ${PROTOBUF_PROTOC}
  ARGS
    --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
    --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
    -I "${rpc_proto_path}"
    --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN_EXECUTABLE}"
    "${rpc_proto}"
  DEPENDS "${rpc_proto}"
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

  # meta
  meta/inode.cpp
  meta/remote_inode.cpp
  meta/dentry.cpp
  meta/file_handler.cpp

  # client
  client/client.cpp
  client/session_client.cpp

  # in_memory
  in_memory/directory_table.cpp
  in_memory/dentry_table.cpp

  # journal
  journal/checkpoint.cpp
  journal/commit.cpp
  journal/journal.cpp
  journal/journal_table.cpp
  journal/transaction.cpp

  # protobuf
  ${CMAKE_CURRENT_BINARY_DIR}/lease.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/lease.grpc.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/session.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/session.grpc.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/rpc.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/rpc.grpc.pb.cc

  # lease
  lease/lease_client.cpp
  lease/lease_table_client.cpp

  # rpc
  rpc/rpc_client.cpp
  rpc/rpc_server.cpp

  # util
  ${CMAKE_SOURCE_DIR}/util/dump.cpp
  ${CMAKE_SOURCE_DIR}/util/path.cpp
  ${CMAKE_SOURCE_DIR}/util/uuid.cpp
)

target_link_libraries(
  nmfs
  log
  rio
  ${LIBCONFIG++}
  ${REFLECTION}
  ${GRPC_GRPCPP}
  ${PROTOBUF_LIBPROTOBUF}
  ${FUSE3_LIBRARIES}
)
target_include_directories(nmfs PUBLIC ${FUSE3_INCLUDE_DIRS})
target_compile_options(nmfs PUBLIC ${FUSE3_CFLAGS_OTHER})
