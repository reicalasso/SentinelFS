# CMake configuration for the network plugin
cmake_minimum_required(VERSION 3.10)

project(NetworkPlugin)

add_library(network_plugin SHARED
    network_plugin.cpp
    HandshakeProtocol.cpp
    TCPHandler.cpp
    UDPDiscovery.cpp
)

target_include_directories(network_plugin PUBLIC
    ../../core/include
)
