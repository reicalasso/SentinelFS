# CMake configuration for the filesystem plugin
cmake_minimum_required(VERSION 3.10)

project(FilesystemPlugin)

add_library(filesystem_plugin SHARED
    # Add source files here
)

target_include_directories(filesystem_plugin PUBLIC
    # Add include directories here
)