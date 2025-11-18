cmake_minimum_required(VERSION 3.15)
project(SentinelFS-Core VERSION 0.1.0 LANGUAGES CXX)

# C++17 standard required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Core library sources
set(CORE_SOURCES
    event_bus/event_bus.cpp
    plugin_loader/plugin_loader.cpp
    logger/logger.cpp
    config/config.cpp
    file_api/file_api_impl.cpp
    file_api/snapshot_engine.cpp
)

set(CORE_HEADERS
    plugin_api.h
    event_bus/event_bus.h
    plugin_loader/plugin_loader.h
    logger/logger.h
    config/config.h
    file_api/file_api.h
    file_api/file_api_impl.h
    file_api/snapshot_engine.h
)

# Create core library
add_library(sfs-core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

# Include directories
target_include_directories(sfs-core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Find OpenSSL (required for SHA-256)
find_package(OpenSSL REQUIRED)
target_link_libraries(sfs-core PUBLIC OpenSSL::SSL OpenSSL::Crypto)

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(sfs-core PUBLIC dl pthread)
elseif(APPLE)
    target_link_libraries(sfs-core PUBLIC dl pthread)
elseif(WIN32)
    # Windows doesn't need explicit dl library
endif()

# Install targets
install(TARGETS sfs-core
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${CORE_HEADERS}
    DESTINATION include/sfs/core
)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
