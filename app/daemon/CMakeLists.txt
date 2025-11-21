cmake_minimum_required(VERSION 3.17)
project(SentinelDaemon)

set(CMAKE_CXX_STANDARD 17)

# Add Core
# add_subdirectory(../../core core_build)

find_library(SENTINEL_CORE_LIB sentinel_core PATHS ${CMAKE_BINARY_DIR}/core)

add_executable(sentinel_daemon 
    sentinel_daemon.cpp
    DaemonCore.cpp
    IPCHandler.cpp
    MetricsServer.cpp
)

target_link_libraries(sentinel_daemon ${SENTINEL_CORE_LIB} dl pthread)
target_include_directories(sentinel_daemon PRIVATE 
    ../../core/include
    ../../core/network/include
    ../../core/security/include
    ../../core/sync/include
    ../../core/utils/include
)
