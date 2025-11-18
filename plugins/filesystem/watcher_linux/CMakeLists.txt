cmake_minimum_required(VERSION 3.15)
project(WatcherLinux VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build as shared library (plugin)
add_library(watcher_linux SHARED
    watcher_linux.cpp
    plugin.cpp
)

# Link against watcher common
target_link_libraries(watcher_linux
    PRIVATE
        watcher-common
)

# Include directories
target_include_directories(watcher_linux
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Set output name (without lib prefix)
set_target_properties(watcher_linux PROPERTIES
    PREFIX ""
    OUTPUT_NAME "watcher_linux"
    SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Install plugin
install(TARGETS watcher_linux
    LIBRARY DESTINATION plugins
)
