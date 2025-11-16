cmake_minimum_required(VERSION 3.16)

project(volta LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(PROTO_FILE ${CMAKE_SOURCE_DIR}/libs/definitions/volta.proto)
get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILE})
protobuf_generate_grpc_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_FILE})

# --- compile .proto ---
add_library(volta_proto ${PROTO_SRCS} ${GRPC_SRCS})
target_include_directories(volta_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(volta_proto PRIVATE ${PROTOBUF_LIBRARIES} gRPC::grpc++)
# --- end compile .proto ---

add_subdirectory(source/agent)
# add_subdirectory(source/server)
