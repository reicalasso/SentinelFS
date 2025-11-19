project(SentinelTests)

add_executable(sentinel_tests placeholder_test.cpp)
target_link_libraries(sentinel_tests sentinel_core)
target_include_directories(sentinel_tests PRIVATE ../core/include)

add_executable(storage_test storage_test.cpp)
target_link_libraries(storage_test sentinel_core)
target_include_directories(storage_test PRIVATE ../core/include)

add_executable(filesystem_test filesystem_test.cpp)
target_link_libraries(filesystem_test sentinel_core)
target_include_directories(filesystem_test PRIVATE ../core/include)

add_executable(delta_test delta_test.cpp)
target_link_libraries(delta_test sentinel_core)
target_include_directories(delta_test PRIVATE ../core/include)

