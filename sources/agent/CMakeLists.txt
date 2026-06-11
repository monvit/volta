cmake_minimum_required(VERSION 3.16)

project(volta_agent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.0")
    message(FATAL_ERROR "GCC 13+ required (found ${CMAKE_CXX_COMPILER_VERSION})")
endif()

# --- Dependencies ---
find_package(gRPC CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(Threads REQUIRED)

# --- Proto / gRPC code generation ---
set(PROTO_DIR ${CMAKE_SOURCE_DIR}/../proto)
set(PROTO_FILE ${PROTO_DIR}/volta.proto)
set(GENERATED_DIR ${CMAKE_SOURCE_DIR}/gen)

get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin LOCATION)

set(PROTO_SRC "${GENERATED_DIR}/volta.pb.cc")
set(PROTO_HDR "${GENERATED_DIR}/volta.pb.h")
set(GRPC_SRC  "${GENERATED_DIR}/volta.grpc.pb.cc")
set(GRPC_HDR  "${GENERATED_DIR}/volta.grpc.pb.h")

add_custom_command(
    OUTPUT  "${PROTO_SRC}" "${PROTO_HDR}" "${GRPC_SRC}" "${GRPC_HDR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
    COMMAND protobuf::protoc
    ARGS    --cpp_out="${GENERATED_DIR}"
            --grpc_out="${GENERATED_DIR}"
            --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}"
            -I "${PROTO_DIR}"
            "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}" gRPC::grpc_cpp_plugin
    COMMENT "Generating C++ code from volta.proto..."
)

add_library(volta_proto STATIC ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR})

target_include_directories(volta_proto PUBLIC ${GENERATED_DIR})

target_link_libraries(volta_proto PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
    gRPC::grpc++_reflection
)

# --- Agent executable ---
add_executable(volta_agent)

target_compile_options(volta_agent PRIVATE -std=c++20)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build - adding DEBUG macro")
    target_compile_definitions(volta_agent PRIVATE DEBUG)
endif()

file(GLOB_RECURSE AGENT_SOURCES CONFIGURE_DEPENDS "src/*.cc")
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
    ${CMAKE_DL_LIBS}
)

target_compile_definitions(volta_agent PRIVATE NVML_NO_UNVERSIONED_FUNC_DEFS)

# --- Config file ---
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/agent.conf)
    if(EXISTS ${CMAKE_SOURCE_DIR}/agent.example.conf)
        message(STATUS "agent.conf not found - copying from agent.example.conf")
        file(COPY ${CMAKE_SOURCE_DIR}/agent.example.conf
             DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
        file(RENAME ${CMAKE_CURRENT_BINARY_DIR}/agent.example.conf
                    ${CMAKE_CURRENT_BINARY_DIR}/agent.conf)
    else()
        message(WARNING "Neither agent.conf nor agent.example.conf found")
    endif()
else()
    add_custom_command(
        TARGET volta_agent POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/agent.conf
            ${CMAKE_CURRENT_BINARY_DIR}/agent.conf
        COMMENT "Copying agent.conf..."
    )
endif()

# --- Formatting ---
find_program(CLANG_FORMAT_EXE clang-format)

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND find src -name "*.cc" -o -name "*.h" | xargs ${CLANG_FORMAT_EXE} -i -style=Google
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-format on agent sources..."
        VERBATIM
    )
    add_custom_target(check-format
        COMMAND find src -name "*.cc" -o -name "*.h" | xargs ${CLANG_FORMAT_EXE} --dry-run --Werror -style=Google
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Checking agent source formatting..."
        VERBATIM
    )
else()
    message(WARNING "clang-format not found - 'make format' unavailable")
endif()

# TODO: find rocm_smi and oneapi level_zero
# TODO: find PMU (libpfm)

# --- Scripts ---
find_program(GIT_EXE git)
if(GIT_EXE)
    execute_process(
        COMMAND ${GIT_EXE} rev-parse --show-toplevel
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_ROOT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(EXISTS ${GIT_ROOT}/.git)
        configure_file(
            ${GIT_ROOT}/scripts/hooks/pre-commit
            ${GIT_ROOT}/.git/hooks/pre-commit
            COPYONLY
        )
        file(CHMOD ${GIT_ROOT}/.git/hooks/pre-commit
             PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
        message(STATUS "Git pre-commit hook installed")
    endif()
endif()
