add_library(peer_registry STATIC
    peer_registry.cpp
    peer_registry.h
)

target_include_directories(peer_registry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
