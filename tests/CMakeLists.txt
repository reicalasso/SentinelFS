project(SentinelTests)

# Function to create test executable with standard includes
function(add_sentinel_test TEST_NAME SOURCE_FILE)
    add_executable(${TEST_NAME} ${SOURCE_FILE})
    target_link_libraries(${TEST_NAME} sentinel_core ${ARGN})
    target_include_directories(${TEST_NAME} PRIVATE 
        ../core/include
        ../core/network/include
        ../core/security/include
        ../core/sync/include
        ../core/utils/include
    )
endfunction()

# Unit tests
add_sentinel_test(sentinel_tests unit/placeholder_test.cpp)
add_sentinel_test(storage_test unit/storage_test.cpp)
add_sentinel_test(filesystem_test unit/filesystem_test.cpp)
add_sentinel_test(delta_test unit/delta_test.cpp)
add_sentinel_test(discovery_test unit/discovery_test.cpp dl)
add_sentinel_test(transfer_test unit/transfer_test.cpp dl)
add_sentinel_test(autoremsh_test unit/autoremsh_test.cpp)
add_sentinel_test(autoremsh_integ_test unit/autoremsh_integ_test.cpp)
add_sentinel_test(watcher_test unit/watcher_test.cpp dl)
add_sentinel_test(bandwidth_limiter_test unit/bandwidth_limiter_test.cpp)

