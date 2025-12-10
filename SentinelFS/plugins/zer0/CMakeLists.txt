cmake_minimum_required(VERSION 3.16)
project(zer0 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Source files
set(ZER0_SOURCES
    src/Zer0Plugin.cpp
    src/MagicBytes.cpp
    src/BehaviorAnalyzer.cpp
)

# Header files
set(ZER0_HEADERS
    include/Zer0Plugin.h
    include/MagicBytes.h
    include/BehaviorAnalyzer.h
)

# Create shared library
add_library(zer0 SHARED ${ZER0_SOURCES} ${ZER0_HEADERS})

target_include_directories(zer0 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/core/include
    ${OPENSSL_INCLUDE_DIR}
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(zer0 PRIVATE
    sentinel_core
    OpenSSL::Crypto
    SQLite::SQLite3
    pthread
)

# Set output directory
set_target_properties(zer0 PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/zer0
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/zer0
)

# Install
install(TARGETS zer0
    LIBRARY DESTINATION lib/sentinelfs/plugins/zer0
    RUNTIME DESTINATION lib/sentinelfs/plugins/zer0
)
