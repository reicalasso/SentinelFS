cmake_minimum_required(VERSION 3.16)
project(falconstore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SQLite3
find_package(SQLite3 REQUIRED)

# Source files
set(FALCON_SOURCES
    src/FalconStore.cpp
    src/MigrationManager.cpp
)

# Header files
set(FALCON_HEADERS
    include/FalconStore.h
    include/QueryBuilder.h
    include/MigrationManager.h
    include/Cache.h
)

# Create shared library
add_library(falconstore SHARED ${FALCON_SOURCES} ${FALCON_HEADERS})

target_include_directories(falconstore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/core/utils/include
    ${SQLite3_INCLUDE_DIRS}
)

target_link_libraries(falconstore PRIVATE
    sentinel_core
    SQLite::SQLite3
    pthread
)

# Set output directory
set_target_properties(falconstore PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/falconstore
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/falconstore
)

# Install
install(TARGETS falconstore
    LIBRARY DESTINATION lib/sentinelfs/plugins/falconstore
    RUNTIME DESTINATION lib/sentinelfs/plugins/falconstore
)

install(FILES ${FALCON_HEADERS}
    DESTINATION include/sentinelfs/plugins/falconstore
)
