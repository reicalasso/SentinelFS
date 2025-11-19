cmake_minimum_required(VERSION 3.17)

project(SentinelCore)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

find_package(OpenSSL REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_library(sentinel_core SHARED ${SOURCES})

target_include_directories(sentinel_core PUBLIC ${OPENSSL_INCLUDE_DIR})
target_link_libraries(sentinel_core ${CMAKE_DL_LIBS} OpenSSL::SSL OpenSSL::Crypto)

# Export for parent CMakeLists
set(SENTINEL_CORE_LIB sentinel_core PARENT_SCOPE)

