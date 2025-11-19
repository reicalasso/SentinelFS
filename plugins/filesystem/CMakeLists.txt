# CMake configuration for the filesystem plugin
cmake_minimum_required(VERSION 3.10)

project(FilesystemPlugin)

add_library(filesystem_plugin SHARED
    filesystem_plugin.cpp
)

target_include_directories(filesystem_plugin PUBLIC
    ../../core/include
)