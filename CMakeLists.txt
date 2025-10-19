cmake_minimum_required(VERSION 3.12)
project(sentinelfs-neo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

# Find required packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Source directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(APP_DIR ${SRC_DIR}/app)
set(FS_DIR ${SRC_DIR}/fs)
set(NET_DIR ${SRC_DIR}/net)
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
    ${NET_DIR}/discovery.cpp
    ${NET_DIR}/remesh.cpp
    ${NET_DIR}/transfer.cpp
    ${DB_DIR}/db.cpp
    ${DB_DIR}/cache.cpp
    ${ML_DIR}/ml_analyzer.cpp
)

# Create executable
add_executable(sentinelfs-neo ${SOURCES})

# Link libraries - using system libraries directly
target_link_libraries(sentinelfs-neo 
    ${CMAKE_THREAD_LIBS_INIT}
    sqlite3
    ${OPENSSL_LIBRARIES}
)

# Include directories
target_include_directories(sentinelfs-neo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OPENSSL_INCLUDE_DIR}
)