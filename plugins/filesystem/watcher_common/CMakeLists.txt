cmake_minimum_required(VERSION 3.15)
project(WatcherCommon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Watcher common library (interface definitions)
add_library(watcher-common STATIC
    iwatcher.cpp
)

target_include_directories(watcher-common
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

# Install
install(TARGETS watcher-common
    ARCHIVE DESTINATION lib
)

install(FILES iwatcher.h
    DESTINATION include/sfs/plugins/filesystem
)
