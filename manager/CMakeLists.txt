# Protobuf files
get_filename_component(lease_proto "proto/lease.proto" ABSOLUTE)
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

get_filename_component(session_proto "proto/session.proto" ABSOLUTE)
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

# Include header files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(_target "manager")
set(
  _source
  "main.cpp"
  "lease/lease_impl.cpp"
  "lease/lease_table.cpp"
  "session/session_impl.cpp"
  "../client/meta/uuid_controller.cpp"
)

# Generate binary files
add_executable(
  ${_target}
  ${_source}
  ${CMAKE_CURRENT_BINARY_DIR}/lease.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/lease.grpc.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/session.pb.cc
  ${CMAKE_CURRENT_BINARY_DIR}/session.grpc.pb.cc
)
target_link_libraries(
  ${_target}
  log
  rio
  ${REFLECTION}
  ${GRPC_GRPCPP}
  ${PROTOBUF_LIBPROTOBUF}
)
