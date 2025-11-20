# CMake configuration for the storage plugin
cmake_minimum_required(VERSION 3.10)

project(StoragePlugin)

find_package(SQLite3 REQUIRED)

add_library(storage_plugin SHARED
    src/storage_plugin.cpp
    src/SQLiteHandler.cpp
    src/FileMetadataManager.cpp
    src/PeerManager.cpp
    src/ConflictManager.cpp
    src/DeviceManager.cpp
    src/SessionManager.cpp
    src/SyncQueueManager.cpp
    src/FileAccessLogManager.cpp
)

target_include_directories(storage_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../../core/include
    ../../core/network/include
    ../../core/security/include
    ../../core/sync/include
    ../../core/utils/include
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(storage_plugin PRIVATE ${SQLite3_LIBRARIES})