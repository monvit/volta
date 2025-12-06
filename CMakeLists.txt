cmake_minimum_required(VERSION 3.16)

project(volta LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(PROTO_DIR ${CMAKE_SOURCE_DIR}/libs/proto)
set(PROTO_FILE ${PROTO_DIR}/volta.proto)
set(LOCAL_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/libs/include")

get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin LOCATION)

set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/volta.pb.cc")
set(PROTO_HDR "${CMAKE_CURRENT_BINARY_DIR}/volta.pb.h")
set(GRPC_SRC "${CMAKE_CURRENT_BINARY_DIR}/volta.grpc.pb.cc")
set(GRPC_HDR "${CMAKE_CURRENT_BINARY_DIR}/volta.grpc.pb.h")

add_custom_command(
    OUTPUT "${PROTO_SRC}" "${PROTO_HDR}" "${GRPC_SRC}" "${GRPC_HDR}"
    
    COMMAND protobuf::protoc
    
    ARGS --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
         --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"
         --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}"
         -I "${PROTO_DIR}"
         "${PROTO_FILE}"
         
    DEPENDS "${PROTO_FILE}" gRPC::grpc_cpp_plugin
    COMMENT "Generating C++ code from volta.proto..."
)

add_library(volta_proto ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR})

target_include_directories(volta_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(volta_proto PRIVATE protobuf::libprotobuf gRPC::grpc++)

include_directories("${LOCAL_INCLUDE_DIR}/nvidia")

add_subdirectory(source/agent)
#add_subdirectory(source/server)

# --- Custom Tools Targets ---
find_program(CLANG_FORMAT_EXE clang-format)

if(CLANG_FORMAT_EXE)
    # clang-format
    add_custom_target(format
        COMMAND find source/agent libs/include -name "*.cc" -o -name "*.h" | xargs ${CLANG_FORMAT_EXE} -i -style=file
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on source files..."
        VERBATIM
    )
    
    # optional: check formatting
    add_custom_target(check-format
        COMMAND find source/agent -name "*.cc" -o -name "*.h" | xargs ${CLANG_FORMAT_EXE} --dry-run --Werror -style=file
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking code formatting..."
        VERBATIM
    )
else()
    message(WARNING "clang-format not found! 'make format' will not be available.")
endif()