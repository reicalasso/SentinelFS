cmake_minimum_required(VERSION 3.12)
project(sentinelfs-neo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

# Find required packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(ZLIB REQUIRED zlib)
endif()

# Source directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(APP_DIR ${SRC_DIR}/app)
set(FS_DIR ${SRC_DIR}/fs)
set(SECURITY_DIR ${SRC_DIR}/security)
set(NET_DIR ${SRC_DIR}/net)
set(SYNC_DIR ${SRC_DIR}/sync)
set(DB_DIR ${SRC_DIR}/db)
set(ML_DIR ${SRC_DIR}/ml)

# Collect source files
set(SOURCES
    ${APP_DIR}/main.cpp
    ${APP_DIR}/cli.cpp
    ${APP_DIR}/logger.cpp
    ${FS_DIR}/watcher.cpp
    ${FS_DIR}/delta_engine.cpp
    ${FS_DIR}/file_queue.cpp
    ${FS_DIR}/conflict_resolver.cpp
    ${FS_DIR}/compressor.cpp
    ${FS_DIR}/file_locker.cpp
    ${SECURITY_DIR}/security_manager.cpp
    ${NET_DIR}/discovery.cpp
    ${NET_DIR}/remesh.cpp
    ${NET_DIR}/transfer.cpp
    ${NET_DIR}/secure_transfer.cpp
    ${NET_DIR}/nat_traversal.cpp
    ${SYNC_DIR}/selective_sync.cpp
    ${SYNC_DIR}/bandwidth_throttling.cpp
    ${SYNC_DIR}/resume_transfers.cpp
    ${SYNC_DIR}/version_history.cpp
    ${SYNC_DIR}/sync_manager.cpp
    ${DB_DIR}/db.cpp
    ${DB_DIR}/cache.cpp
    ${ML_DIR}/ml_analyzer.cpp
    ${ML_DIR}/ml_data_generator.cpp
    ${ML_DIR}/neural_network.cpp
    ${ML_DIR}/federated_learning.cpp
    ${ML_DIR}/online_learning.cpp
    ${ML_DIR}/advanced_forecasting.cpp
)

# Create executable
add_executable(sentinelfs-neo ${SOURCES})

# Link libraries - using system libraries directly
target_link_libraries(sentinelfs-neo 
    ${CMAKE_THREAD_LIBS_INIT}
    sqlite3
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

# Include directories
target_include_directories(sentinelfs-neo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPENSSL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)