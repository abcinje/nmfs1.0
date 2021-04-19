# Protobuf files
get_filename_component(manager_proto "proto/manager.proto" ABSOLUTE)
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

set(_target "manager")
set(_source "main.cpp" "manager_impl.cpp" "lease_table.cpp")

# Generate binary files
add_executable(
  ${_target}
  ${_source}
  "${CMAKE_CURRENT_BINARY_DIR}/manager.pb.cc"
  "${CMAKE_CURRENT_BINARY_DIR}/manager.grpc.pb.cc"
)
target_link_libraries(
  ${_target}
  ${REFLECTION}
  ${GRPC_GRPCPP}
  ${PROTOBUF_LIBPROTOBUF}
)
