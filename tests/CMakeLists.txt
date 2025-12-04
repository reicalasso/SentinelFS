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
        ../core/storage/include
    )
endfunction()

# Function for integration tests (includes plugin headers)
function(add_integration_test TEST_NAME SOURCE_FILE)
    add_executable(${TEST_NAME} ${SOURCE_FILE})
    target_link_libraries(${TEST_NAME} sentinel_core ${ARGN})
    target_include_directories(${TEST_NAME} PRIVATE 
        ../core/include
        ../core/network/include
        ../core/security/include
        ../core/sync/include
        ../core/utils/include
        ../core/storage/include
        ../plugins/filesystem/include
        ../plugins/network/include
        ../plugins/storage/include
        ../plugins/ml/include
    )
endfunction()

# ============================================
# UNIT TESTS - Add tests here
# ============================================

add_sentinel_test(test_config unit/test_config.cpp)
add_sentinel_test(test_crypto unit/test_crypto.cpp)
add_sentinel_test(test_threadpool unit/test_threadpool.cpp)
add_sentinel_test(test_logger unit/test_logger.cpp)
add_sentinel_test(test_eventbus unit/test_eventbus.cpp)

# ============================================
# INTEGRATION TESTS - Add tests here
# ============================================

# Example:
# add_integration_test(test_example integration/test_example.cpp)

