project(SentinelDaemon)

add_executable(sentinel_daemon sentinel_daemon.cpp)

target_link_libraries(sentinel_daemon sentinel_core dl)
target_include_directories(sentinel_daemon PRIVATE ../../core/include)
