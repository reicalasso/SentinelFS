cmake_minimum_required(VERSION 3.17)
project(SentinelDaemon)

set(CMAKE_CXX_STANDARD 17)

# Add Core
add_subdirectory(../../core core_build)

add_executable(sentinel_daemon sentinel_daemon.cpp)

target_link_libraries(sentinel_daemon sentinel_core dl pthread)
target_include_directories(sentinel_daemon PRIVATE ../../core/include)
