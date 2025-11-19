# CMake configuration for the storage plugin
cmake_minimum_required(VERSION 3.10)

project(StoragePlugin)

add_library(storage_plugin SHARED
    # Add source files here
)

target_include_directories(storage_plugin PUBLIC
    # Add include directories here
)