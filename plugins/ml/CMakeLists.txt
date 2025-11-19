# CMake configuration for the ML plugin
cmake_minimum_required(VERSION 3.10)

project(MLPlugin)

add_library(ml_plugin SHARED
    ml_plugin.cpp
)

target_include_directories(ml_plugin PUBLIC
    ../../core/include
)