# CMake configuration for the storage plugin
cmake_minimum_required(VERSION 3.10)

project(StoragePlugin)

find_package(SQLite3 REQUIRED)

add_library(storage_plugin SHARED
    storage_plugin.cpp
)

target_include_directories(storage_plugin PUBLIC
    ../../core/include
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(storage_plugin PRIVATE ${SQLite3_LIBRARIES})