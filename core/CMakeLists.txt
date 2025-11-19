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

# Collect sources from all subdirectories
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "network/src/*.cpp"
    "security/src/*.cpp"
    "sync/src/*.cpp"
    "storage/src/*.cpp"
    "utils/src/*.cpp"
)

add_library(sentinel_core SHARED ${SOURCES})

target_include_directories(sentinel_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/network/include
    ${CMAKE_CURRENT_SOURCE_DIR}/security/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sync/include
    ${CMAKE_CURRENT_SOURCE_DIR}/storage/include
    ${CMAKE_CURRENT_SOURCE_DIR}/utils/include
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(sentinel_core ${CMAKE_DL_LIBS} OpenSSL::SSL OpenSSL::Crypto)

# Export for parent CMakeLists
set(SENTINEL_CORE_LIB sentinel_core PARENT_SCOPE)

