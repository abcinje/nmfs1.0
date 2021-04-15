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

set(_target "manager")
set(_source "manager.cpp" "lease_impl.cpp")

# Generate binary files
add_executable(
  ${_target}
  ${_source}
  "${CMAKE_CURRENT_BINARY_DIR}/lease.pb.cc"
  "${CMAKE_CURRENT_BINARY_DIR}/lease.grpc.pb.cc"
)
target_link_libraries(
  ${_target}
  ${REFLECTION}
  ${GRPC_GRPCPP}
  ${PROTOBUF_LIBPROTOBUF}
)
