set(AGENT_NAME volta_agent)
add_executable(${AGENT_NAME})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build - adding DEBUG macro")
    target_compile_definitions(${AGENT_NAME} PRIVATE DEBUG)
endif()

file(GLOB_RECURSE AGENT_SOURCES "src/*.cc")
target_sources(${AGENT_NAME} PRIVATE ${AGENT_SOURCES})

target_include_directories(${AGENT_NAME} PRIVATE src)

target_link_libraries(${AGENT_NAME} PRIVATE volta_proto)

find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

target_link_libraries(${AGENT_NAME} PRIVATE Threads::Threads)
target_link_libraries(${AGENT_NAME} PRIVATE gRPC::grpc++ protobuf::libprotobuf)

find_path(NVML_INCLUDE_DIR nvml.h
    PATHS ${CMAKE_SOURCE_DIR}/libs/include/nvidia
    /usr/include
    /usr/local/cuda/include
)

find_library(NVML_LIBRARY NAMES nvidia-ml nvml
    PATHS /usr/lib /usr/lib64 /usr/local/lib
)

if(NVML_INCLUDE_DIR AND NVML_LIBRARY)
    message(STATUS "Found Real NVML: ${NVML_LIBRARY}")
    target_include_directories(${AGENT_NAME} PRIVATE ${NVML_INCLUDE_DIR})
    target_link_libraries(${AGENT_NAME} PRIVATE ${NVML_LIBRARY})
    target_compile_definitions(${AGENT_NAME} PRIVATE HAVE_NVML)
else()
    message(STATUS "NVML library not found. Using STUB for compilation.")
    add_library(nvml_stub STATIC ${CMAKE_SOURCE_DIR}/libs/stubs/nvml_stub.cc)
    target_include_directories(nvml_stub PRIVATE ${CMAKE_SOURCE_DIR}/libs/include/nvidia)
    target_link_libraries(${AGENT_NAME} PRIVATE nvml_stub)
    target_compile_definitions(${AGENT_NAME} PRIVATE HAVE_NVML)
    target_include_directories(${AGENT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/libs/include/nvidia)
endif()

find_path(TOML_INCLUDE_DIR toml++/toml.hpp
    PATHS ${CMAKE_SOURCE_DIR}/libs/include
          /usr/include
          /usr/local/include
)

if(TOML_INCLUDE_DIR)
    message(STATUS "Found toml++ headers: ${TOML_INCLUDE_DIR}")
    target_include_directories(${AGENT_NAME} PRIVATE ${TOML_INCLUDE_DIR})
else()
    message(FATAL_ERROR "toml++ headers not found! Please clone them into libs/include/toml++")
endif()

install(TARGETS ${AGENT_NAME} RUNTIME DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/agent.conf DESTINATION /etc/volta)

add_custom_command(
    TARGET ${AGENT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:${AGENT_NAME}>/etc/volta
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/agent.conf
        $<TARGET_FILE_DIR:${AGENT_NAME}>/etc/volta/agent.conf
)

# 5. TODO: find rocm_smi and oneapi level_zero
# 6. TODO: find PMU (libpfm)
