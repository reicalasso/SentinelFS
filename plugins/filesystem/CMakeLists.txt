# CMake configuration for the filesystem plugin
cmake_minimum_required(VERSION 3.10)

project(FilesystemPlugin)

find_package(OpenSSL REQUIRED)

add_library(filesystem_plugin SHARED
    filesystem_plugin.cpp
    InotifyWatcher.cpp
    FileHasher.cpp
)

target_include_directories(filesystem_plugin PUBLIC
    ../../core/include
    ../../core/network/include
    ../../core/security/include
    ../../core/sync/include
    ../../core/utils/include
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(filesystem_plugin
    OpenSSL::SSL
    OpenSSL::Crypto
)