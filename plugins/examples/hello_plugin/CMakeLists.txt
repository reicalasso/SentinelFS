cmake_minimum_required(VERSION 3.15)
project(HelloPlugin VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build as shared library (plugin)
add_library(hello_plugin SHARED
    hello_plugin.cpp
)

# Include core headers
target_include_directories(hello_plugin
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Set output name (without lib prefix on Unix)
set_target_properties(hello_plugin PROPERTIES
    PREFIX ""
    OUTPUT_NAME "hello_plugin"
)

# Platform-specific settings
if(WIN32)
    set_target_properties(hello_plugin PROPERTIES
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(hello_plugin PROPERTIES
        SUFFIX ".dylib"
    )
else()
    set_target_properties(hello_plugin PROPERTIES
        SUFFIX ".so"
    )
endif()

# Install plugin to plugins directory
install(TARGETS hello_plugin
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins
)
