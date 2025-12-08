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
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
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
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# ============================================
# UNIT TESTS - Add tests here
# ============================================

add_sentinel_test(test_config unit/test_config.cpp)
add_sentinel_test(test_crypto unit/test_crypto.cpp)
add_sentinel_test(test_threadpool unit/test_threadpool.cpp)
add_sentinel_test(test_logger unit/test_logger.cpp)
add_sentinel_test(test_eventbus unit/test_eventbus.cpp)
add_sentinel_test(test_sha256 unit/test_sha256.cpp)
add_sentinel_test(test_compression unit/test_compression.cpp)
add_sentinel_test(test_utils unit/test_utils.cpp)
add_sentinel_test(test_result unit/test_result.cpp)
add_sentinel_test(test_metrics unit/test_metrics.cpp)
add_sentinel_test(test_bandwidth_limiter unit/test_bandwidth_limiter.cpp)
add_sentinel_test(test_delta_engine unit/test_delta_engine.cpp)
add_sentinel_test(test_delta_serialization unit/test_delta_serialization.cpp)
add_sentinel_test(test_session_code unit/test_session_code.cpp)
add_integration_test(test_anomaly_detector unit/test_anomaly_detector.cpp ml_plugin)
add_integration_test(test_threat_detector unit/test_threat_detector.cpp ml_plugin)
add_sentinel_test(test_auto_remesh unit/test_auto_remesh.cpp)
add_sentinel_test(test_offline_queue unit/test_offline_queue.cpp)

# FileHasher is in plugins/filesystem, so we need to link against it or include its source
# Since plugins are usually shared libs, we might need to link sentinel_filesystem if it exists
# Or just include the source if it's simple.
# Let's check if sentinel_filesystem target exists.
# Assuming it does, we use add_integration_test which includes plugin headers.
# But we also need to link the library.
add_integration_test(test_file_hasher unit/test_file_hasher.cpp filesystem_plugin)
add_sentinel_test(test_key_manager unit/test_key_manager.cpp)
add_sentinel_test(test_conflict_resolver unit/test_conflict_resolver.cpp)
add_sentinel_test(test_plugin_manager unit/test_plugin_manager.cpp)
add_sentinel_test(test_storage unit/test_storage.cpp)
add_sentinel_test(test_network_manager unit/test_network_manager.cpp)

# New Tests
add_sentinel_test(test_delta_sync unit/test_delta_sync.cpp)
add_sentinel_test(test_tls_context unit/test_tls_context.cpp)
add_sentinel_test(test_delta_sync_protocol_handler unit/test_delta_sync_protocol_handler.cpp)
add_sentinel_test(test_event_handlers unit/test_event_handlers.cpp)
add_sentinel_test(test_file_sync_handler unit/test_file_sync_handler.cpp)
add_sentinel_test(test_health_endpoint unit/test_health_endpoint.cpp)
add_sentinel_test(test_path_utils unit/test_path_utils.cpp)
add_sentinel_test(test_plugin_loader unit/test_plugin_loader.cpp)

# TODO: Update these tests to use new plugin APIs (IronRoot, NetFalcon)
# add_integration_test(test_fs_watchers unit/test_fs_watchers.cpp ironroot)
# add_integration_test(test_handshake_protocol unit/test_handshake_protocol.cpp netfalcon)
# add_integration_test(test_tcp_handler unit/test_tcp_handler.cpp netfalcon)
# add_integration_test(test_udp_discovery unit/test_udp_discovery.cpp netfalcon)
add_integration_test(test_storage_managers unit/test_storage_managers.cpp storage_plugin sqlite3)

# ============================================
# INTEGRATION TESTS - Add tests here
# ============================================

add_integration_test(test_full_sync integration/test_full_sync.cpp)
add_integration_test(test_peer_lifecycle integration/test_peer_lifecycle.cpp)

# Remote Sync Test
add_integration_test(test_remote_sync integration/test_remote_sync.cpp sqlite3)

# Offline Sync Test
add_integration_test(test_offline_sync integration/test_offline_sync.cpp sqlite3)


