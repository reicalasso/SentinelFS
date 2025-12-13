cmake_minimum_required(VERSION 3.17)

project(SentinelCore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Modern CMake: No global include_directories
# Use target_include_directories instead

find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LZ4 REQUIRED liblz4)

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
    security/src/keymanager/KeyStructs.cpp
    security/src/keymanager/FileKeyStore.cpp
    security/src/keymanager/KeyManagerCore.cpp
    security/src/keymanager/IdentityKeyManager.cpp
    security/src/keymanager/PeerKeyManager.cpp
    security/src/keymanager/SessionKeyManager.cpp
    security/src/tls/TLSContextCore.cpp
    security/src/tls/PinManager.cpp
    security/src/tls/PinRotation.cpp
    security/src/tls/Verification.cpp

    # Sync sources
    sync/src/ConflictResolver.cpp
    sync/src/ConflictHandler.cpp
    sync/src/EventHandlers.cpp
    sync/src/FileSyncHandler.cpp
    sync/src/DeltaSerialization.cpp
    sync/src/OfflineQueue.cpp
    sync/src/delta/DeltaSyncCore.cpp
    sync/src/delta/UpdateHandler.cpp
    sync/src/delta/DeltaRequestHandler.cpp
    sync/src/delta/DeltaDataHandler.cpp
    sync/src/delta/FileTransferHandler.cpp
    
    # Sync pipeline sources
    sync/src/pipeline/SyncPipelineCore.cpp
    sync/src/pipeline/HandshakeHandler.cpp
    sync/src/pipeline/MetaTransferHandler.cpp
    sync/src/pipeline/DeltaSyncHandler.cpp
    sync/src/pipeline/BlockStreamHandler.cpp
    sync/src/pipeline/FinalizeHandler.cpp
    
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
    utils/src/HealthEndpoint.cpp
    utils/src/Compression.cpp
    utils/src/PathUtils.cpp
    utils/src/DBHelper.cpp
    utils/src/BatchQueries.cpp
    src/ErrorCodes.cpp
    src/DatabaseManager.cpp
    src/MergeResolver.cpp
    src/DatabaseMigrations.cpp
)

add_library(sentinel_core SHARED ${CORE_SOURCES})

# Modern CMake: Use target_include_directories with PUBLIC/PRIVATE
target_include_directories(sentinel_core 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/network/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/security/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sync/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/storage/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/utils/include>
        $<BUILD_INTERFACE:${LZ4_INCLUDE_DIRS}>
        $<INSTALL_INTERFACE:include>
)

find_package(ZLIB REQUIRED)

# Modern CMake: Explicit PUBLIC/PRIVATE linkage
target_link_libraries(sentinel_core 
    PUBLIC
        OpenSSL::SSL 
        OpenSSL::Crypto 
        SQLite::SQLite3
        ZLIB::ZLIB
        ${LZ4_LIBRARIES}
    PRIVATE
        ${CMAKE_DL_LIBS}
)

# Export for parent CMakeLists
set(SENTINEL_CORE_LIB sentinel_core PARENT_SCOPE)
