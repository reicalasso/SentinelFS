cmake_minimum_required(VERSION 3.15)
project(SentinelFS-App VERSION 0.1.0 LANGUAGES CXX)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sprint 1 test executable
add_executable(sentinelfs-test
    main.cpp
)

target_link_libraries(sentinelfs-test
    PRIVATE
        sfs-core
)

target_include_directories(sentinelfs-test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Sprint 2 test executable
add_executable(sentinelfs-sprint2
    sprint2_test.cpp
)

target_link_libraries(sentinelfs-sprint2
    PRIVATE
        sfs-core
)

target_include_directories(sentinelfs-sprint2
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Install executables
install(TARGETS sentinelfs-test sentinelfs-sprint2
    RUNTIME DESTINATION bin
)
