# CMake configuration for the filesystem plugin
cmake_minimum_required(VERSION 3.10)

project(FilesystemPlugin)

find_package(OpenSSL REQUIRED)

add_library(filesystem_plugin SHARED
    src/filesystem_plugin.cpp
    src/InotifyWatcher.cpp
    src/Win32Watcher.cpp
    src/FSEventsWatcher.cpp
    src/FileHasher.cpp
)

target_include_directories(filesystem_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../../core/include
    ../../core/network/include
    ../../core/security/include
    ../../core/sync/include
    ../../core/utils/include
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(filesystem_plugin
    sentinel_core
    OpenSSL::SSL
    OpenSSL::Crypto
)