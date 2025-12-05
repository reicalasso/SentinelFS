# CMake configuration for the ML plugin
cmake_minimum_required(VERSION 3.10)

project(MLPlugin)

add_library(ml_plugin SHARED
    src/ml_plugin.cpp
    src/AnomalyDetector.cpp
)

target_include_directories(ml_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../../core/include
    ../../core/network/include
    ../../core/security/include
    ../../core/sync/include
    ../../core/utils/include
)

target_link_libraries(ml_plugin PRIVATE sentinel_core)