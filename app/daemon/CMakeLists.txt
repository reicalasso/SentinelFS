project(SentinelDaemon)

add_executable(sentinel_daemon placeholder.cpp)

target_link_libraries(sentinel_daemon sentinel_core)
target_include_directories(sentinel_daemon PRIVATE ../../core/include)
