# CMake configuration for the storage plugin
cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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

target_link_libraries(storage_plugin PRIVATE sentinel_core ${SQLite3_LIBRARIES})

# Check if we need to link stdc++fs (for older GCC/Clang but harmless to check)
# In some environments, filesystem is a separate library
find_library(STD_FS_LIB stdc++fs)
if(STD_FS_LIB)
    target_link_libraries(storage_plugin PRIVATE ${STD_FS_LIB})
endif()