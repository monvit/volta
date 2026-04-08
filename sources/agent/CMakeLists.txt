cmake_minimum_required(VERSION 3.16)

project(volta_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Dependencies ---
find_package(protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(Threads REQUIRED)

# --- Proto / gRPC code generation ---
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/../proto)
set(PROTO_FILE ${PROTO_DIR}/volta.proto)

get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin LOCATION)

set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/volta.pb.cc")
set(PROTO_HDR "${CMAKE_CURRENT_BINARY_DIR}/volta.pb.h")
set(GRPC_SRC  "${CMAKE_CURRENT_BINARY_DIR}/volta.grpc.pb.cc")
set(GRPC_HDR  "${CMAKE_CURRENT_BINARY_DIR}/volta.grpc.pb.h")

add_custom_command(
    OUTPUT  "${PROTO_SRC}" "${PROTO_HDR}" "${GRPC_SRC}" "${GRPC_HDR}"
    COMMAND protobuf::protoc
    ARGS    --cpp_out="${CMAKE_CURRENT_BINARY_DIR}"
            --grpc_out="${CMAKE_CURRENT_BINARY_DIR}"
            --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}"
            -I "${PROTO_DIR}"
            "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}" gRPC::grpc_cpp_plugin
    COMMENT "Generating C++ code from volta.proto..."
)

add_library(volta_proto STATIC ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR})

target_include_directories(volta_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(volta_proto PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection
)

# --- Agent executable ---
add_executable(volta_agent)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build - adding DEBUG macro")
    target_compile_definitions(volta_agent PRIVATE DEBUG)
endif()

file(GLOB_RECURSE AGENT_SOURCES "src/*.cc")
target_sources(volta_agent PRIVATE ${AGENT_SOURCES})

target_include_directories(volta_agent PRIVATE
    src
    ${CMAKE_SOURCE_DIR}/libs/include/nvidia
)

target_link_libraries(volta_agent PRIVATE
    volta_proto
    Threads::Threads
    fmt::fmt
    tomlplusplus::tomlplusplus
)

# --- NVML ---
find_path(NVML_INCLUDE_DIR nvml.h
    PATHS
        ${CMAKE_SOURCE_DIR}/libs/include/nvidia
        /usr/include
        /usr/local/cuda/include
)

find_library(NVML_LIBRARY NAMES nvidia-ml nvml
    PATHS /usr/lib /usr/lib64 /usr/local/lib
)

if(NVML_INCLUDE_DIR AND NVML_LIBRARY)
    message(STATUS "Found NVML: ${NVML_LIBRARY}")
    target_include_directories(volta_agent PRIVATE ${NVML_INCLUDE_DIR})
    target_link_libraries(volta_agent PRIVATE ${NVML_LIBRARY})
else()
    message(STATUS "NVML not found - using stub")
    add_library(nvml_stub STATIC ${CMAKE_SOURCE_DIR}/libs/stubs/nvml_stub.cc)
    target_include_directories(nvml_stub PRIVATE ${CMAKE_SOURCE_DIR}/libs/include/nvidia)
    target_link_libraries(volta_agent PRIVATE nvml_stub)
    target_include_directories(volta_agent PRIVATE ${CMAKE_SOURCE_DIR}/libs/include/nvidia)
endif()

target_compile_definitions(volta_agent PRIVATE HAVE_NVML)

# --- Config file ---
configure_file(
    ${CMAKE_SOURCE_DIR}/agent.conf
    ${CMAKE_CURRENT_BINARY_DIR}/agent.conf
    COPYONLY
)

# --- Formatting ---
find_program(CLANG_FORMAT_EXE clang-format)

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND find source/agent -name "*.cc" -o -name "*.h" | xargs ${CLANG_FORMAT_EXE} -i -style=Google
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on agent sources..."
        VERBATIM
    )
    add_custom_target(check-format
        COMMAND find source/agent -name "*.cc" -o -name "*.h" | xargs ${CLANG_FORMAT_EXE} --dry-run --Werror -style=Google
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking agent source formatting..."
        VERBATIM
    )
else()
    message(WARNING "clang-format not found - 'make format' unavailable")
endif()

# TODO: find rocm_smi and oneapi level_zero
# TODO: find PMU (libpfm)