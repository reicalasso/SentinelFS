project(SentinelTests)

add_executable(sentinel_tests placeholder_test.cpp)

target_link_libraries(sentinel_tests sentinel_core)
target_include_directories(sentinel_tests PRIVATE ../core/include)
