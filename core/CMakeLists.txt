cmake_minimum_required(VERSION 3.17)

project(SentinelCore)

set(CMAKE_CXX_STANDARD 17)

# Include all subdirectories
include_directories(
    include
    network/include
    security/include
    sync/include
    storage/include
    utils/include
)

find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

# Explicitly list all source files (better than GLOB for CMake)
set(CORE_SOURCES
    # Network sources
    network/src/DeltaEngine.cpp
    network/src/BandwidthLimiter.cpp
    network/src/DeltaSync.cpp
    network/src/NetworkManager.cpp
    network/src/AutoRemeshManager.cpp

    # Security sources
    security/src/Crypto.cpp

    # Sync sources
    sync/src/ConflictResolver.cpp
    sync/src/EventHandlers.cpp
    sync/src/FileSyncHandler.cpp
    sync/src/DeltaSyncProtocolHandler.cpp
    sync/src/DeltaSerialization.cpp
    sync/src/OfflineQueue.cpp

    # Storage sources
    storage/src/FileVersionManager.cpp

    # Utils sources
    utils/src/Logger.cpp
    utils/src/Config.cpp
    utils/src/EventBus.cpp
    utils/src/PluginLoader.cpp
    utils/src/PluginManager.cpp
    utils/src/MetricsCollector.cpp
    utils/src/ThreadPool.cpp
    # utils/src/HealthEndpoint.cpp  # Deprecated: Use MetricsServer instead
    utils/src/Compression.cpp
    src/PathUtils.cpp
)

add_library(sentinel_core SHARED ${CORE_SOURCES})

target_include_directories(sentinel_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/network/include
    ${CMAKE_CURRENT_SOURCE_DIR}/security/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/storage/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/include
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

find_package(ZLIB REQUIRED)

target_link_libraries(sentinel_core ${CMAKE_DL_LIBS} OpenSSL::SSL OpenSSL::Crypto ${SQLite3_LIBRARIES} ZLIB::ZLIB)

# Export for parent CMakeLists
set(SENTINEL_CORE_LIB sentinel_core PARENT_SCOPE)

