cmake_minimum_required(VERSION 3.16)
project(IronRootPlugin)

add_library(ironroot SHARED
    src/IronRootPlugin.cpp
    src/FanotifyWatcher.cpp
    src/InotifyWatcher.cpp
    src/Debouncer.cpp
    src/FileOps.cpp
)

target_include_directories(ironroot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../../core/include
    ../../core/utils/include
)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

target_link_libraries(ironroot PRIVATE 
    sentinel_core
    OpenSSL::Crypto
)

# Check for fanotify support
include(CheckIncludeFile)
check_include_file("sys/fanotify.h" HAVE_FANOTIFY)
if(HAVE_FANOTIFY)
    target_compile_definitions(ironroot PRIVATE HAVE_FANOTIFY)
    message(STATUS "fanotify support enabled")
else()
    message(STATUS "fanotify not available - using inotify only")
endif()

# Check for xattr support
check_include_file("sys/xattr.h" HAVE_XATTR)
if(HAVE_XATTR)
    target_compile_definitions(ironroot PRIVATE HAVE_XATTR)
    message(STATUS "xattr support enabled")
endif()

set_target_properties(ironroot PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Install
install(TARGETS ironroot
    LIBRARY DESTINATION lib/sentinelfs/plugins
)
