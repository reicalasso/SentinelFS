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

# Add all test executables
add_sentinel_test(sentinel_tests placeholder_test.cpp)
add_sentinel_test(storage_test storage_test.cpp)
add_sentinel_test(filesystem_test filesystem_test.cpp)
add_sentinel_test(delta_test delta_test.cpp)
add_sentinel_test(discovery_test discovery_test.cpp dl)
add_sentinel_test(transfer_test transfer_test.cpp dl)
add_sentinel_test(autoremsh_test autoremsh_test.cpp)
add_sentinel_test(autoremsh_integ_test autoremsh_integ_test.cpp)

