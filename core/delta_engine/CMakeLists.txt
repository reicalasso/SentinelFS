cmake_minimum_required(VERSION 3.15)

# Delta Engine Library
add_library(sfs-delta-engine
    delta_types.cpp
    delta_engine.cpp
)

target_include_directories(sfs-delta-engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(sfs-delta-engine
    PUBLIC
        OpenSSL::Crypto
)

# Install headers
install(FILES
    delta_types.h
    delta_engine.h
    DESTINATION include/sfs/core/delta_engine
)
