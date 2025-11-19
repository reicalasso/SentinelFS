# CMake configuration for the network plugin
cmake_minimum_required(VERSION 3.10)

project(NetworkPlugin)

add_library(network_plugin SHARED
    # Add source files here
)

target_include_directories(network_plugin PUBLIC
    # Add include directories here
)

# Network plugins
add_subdirectory(discovery_udp)
add_subdirectory(transfer_tcp)
