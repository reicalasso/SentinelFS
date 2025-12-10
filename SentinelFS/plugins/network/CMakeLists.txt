# CMake configuration for the network plugin
cmake_minimum_required(VERSION 3.10)

project(NetworkPlugin)

add_library(network_plugin SHARED
    src/network_plugin.cpp
    src/HandshakeProtocol.cpp
    src/TCPHandler.cpp
    src/UDPDiscovery.cpp
    src/TCPRelay.cpp
)

target_include_directories(network_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../../core/include
    ../../core/network/include
    ../../core/security/include
    ../../core/sync/include
    ../../core/utils/include
)

target_link_libraries(network_plugin PRIVATE sentinel_core)
