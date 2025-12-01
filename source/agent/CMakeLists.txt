set(AGENT_NAME volta_agent)
add_executable(${AGENT_NAME})

file(GLOB_RECURSE AGENT_SOURCES "src/*.cc")
target_sources(${AGENT_NAME} PRIVATE ${AGENT_SOURCES})

target_include_directories(${AGENT_NAME} PRIVATE src)

target_link_libraries(${AGENT_NAME} PRIVATE volta_proto)

find_package(Threads REQUIRED)
find_package(protobuf REQUIRED)
find_package(gRPC REQUIRED)

target_link_libraries(${AGENT_NAME} PRIVATE Threads::Threads)
target_link_libraries(${AGENT_NAME} PRIVATE gRPC::grpc++ protobuf::libprotobuf)

find_path(NVML_INCLUDE_DIR nvml.h
    PATHS ${CMAKE_SOURCE_DIR}/libs/include/nvidia
    
    /usr/include
    /usr/local/include
    /usr/include/nvidia
    /usr/local/cuda/include
    PATH_SUFFIXES nvidia/current gdk
)

find_library(NVML_LIBRARY NAMES nvidia-ml nvml
    PATHS /usr/lib /usr/lib64 /usr/local/lib /usr/lib/x86_64-linux-gnu
    PATH_SUFFIXES nvidia/current
)

if(NVML_INCLUDE_DIR AND NVML_LIBRARY)
    message(STATUS "Found NVML: ${NVML_LIBRARY}")
    target_include_directories(${AGENT_NAME} PRIVATE ${NVML_INCLUDE_DIR})
    target_link_libraries(${AGENT_NAME} PRIVATE ${NVML_LIBRARY})
    target_compile_definitions(${AGENT_NAME} PRIVATE HAVE_NVML)
else()
    message(WARNING "NVML not found - the agent will be built without NVIDIA support.")
endif()

# 5. TODO: find rocm_smi and oneapi level_zero
# 6. TODO: find PMU (libpfm)
