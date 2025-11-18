cmake_minimum_required(VERSION 3.15)
project(SentinelFS-App VERSION 0.1.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create test executable
add_executable(sentinelfs-test
    main.cpp
)

# Link against core library
target_link_libraries(sentinelfs-test
    PRIVATE
        sfs-core
)

# Include directories
target_include_directories(sentinelfs-test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Install executable
install(TARGETS sentinelfs-test
    RUNTIME DESTINATION bin
)
