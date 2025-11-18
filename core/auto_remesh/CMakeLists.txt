cmake_minimum_required(VERSION 3.15)

# Auto-remesh library
add_library(auto_remesh STATIC
    network_metrics.cpp
    peer_scorer.cpp
    auto_remesh.cpp
)

target_include_directories(auto_remesh
    PUBLIC
        ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(auto_remesh
    PUBLIC
        peer_registry
)

# Install library
install(TARGETS auto_remesh
    ARCHIVE DESTINATION lib
)
