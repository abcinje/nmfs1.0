cmake_minimum_required(VERSION 3.5.1)
project(nmfs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Release)

find_package(Threads REQUIRED)

# Find Protobuf
set(protobuf_MODULE_COMPATIBLE TRUE)
find_package(Protobuf CONFIG REQUIRED)
message(STATUS "Using Protobuf ${Protobuf_VERSION}")
set(PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
set(PROTOBUF_LIBPROTOBUF protobuf::libprotobuf)
set(REFLECTION gRPC::grpc++_reflection)

# Find gRPC
find_package(gRPC CONFIG REQUIRED)
message(STATUS "Using gRPC ${gRPC_VERSION}")
set(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
set(GRPC_GRPCPP gRPC::grpc++)

add_compile_options(-Wconversion)

add_subdirectory(client)
add_subdirectory(manager)
