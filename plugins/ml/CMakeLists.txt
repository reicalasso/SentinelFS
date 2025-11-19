# CMake configuration for the ML plugin
cmake_minimum_required(VERSION 3.10)

project(MLPlugin)

add_library(ml_plugin SHARED
    # Add source files here
)

target_include_directories(ml_plugin PUBLIC
    # Add include directories here
)