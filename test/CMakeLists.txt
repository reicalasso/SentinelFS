cmake_minimum_required(VERSION 3.12)
project(sentinelFS_test)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pthread")

# Find required packages
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZLIB REQUIRED zlib)

# Source directories
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# Include directories
include_directories(${SRC_DIR})

# Test executable
add_executable(test_advanced_features
    ${SRC_DIR}/ml/test_advanced_features.cpp
    ${SRC_DIR}/ml/advanced_forecasting.cpp
    ${SRC_DIR}/ml/online_learning.cpp
    ${SRC_DIR}/ml/neural_network.cpp
    ${SRC_DIR}/ml/federated_learning.cpp
    ${SRC_DIR}/fs/conflict_resolver.cpp
    ${SRC_DIR}/fs/compressor.cpp
    ${SRC_DIR}/fs/file_locker.cpp
    ${SRC_DIR}/fs/delta_engine.cpp
)

# Link libraries
target_link_libraries(test_advanced_features 
    ${CMAKE_THREAD_LIBS_INIT}
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    crypto
)

# Include directories
target_include_directories(test_advanced_features PRIVATE 
    ${SRC_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${ZLIB_INCLUDE_DIRS}
)