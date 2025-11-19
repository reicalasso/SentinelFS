project(SentinelTests)

add_executable(sentinel_tests placeholder_test.cpp)
target_link_libraries(sentinel_tests sentinel_core)
target_include_directories(sentinel_tests PRIVATE ../core/include)

add_executable(storage_test storage_test.cpp)
target_link_libraries(storage_test sentinel_core)
target_include_directories(storage_test PRIVATE ../core/include)

