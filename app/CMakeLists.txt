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

# Sprint 3 test executable (full)
add_executable(sentinelfs-sprint3
    sprint3_test.cpp
)

target_link_libraries(sentinelfs-sprint3
    PRIVATE
        sfs-core
        watcher-common
)

target_include_directories(sentinelfs-sprint3
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Sprint 3 simple test (plugin loading only)
add_executable(sentinelfs-sprint3-simple
    sprint3_simple_test.cpp
)

target_link_libraries(sentinelfs-sprint3-simple
    PRIVATE
        sfs-core
)

target_include_directories(sentinelfs-sprint3-simple
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Sprint 4 test executable (Delta Engine)
add_executable(sentinelfs-sprint4
    sprint4_test.cpp
)

target_link_libraries(sentinelfs-sprint4
    PRIVATE
        sfs-core
)

target_include_directories(sentinelfs-sprint4
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Sprint 5 test executable (Network Layer)
add_executable(sentinelfs-sprint5
    sprint5_test.cpp
)

target_link_libraries(sentinelfs-sprint5
    PRIVATE
        sfs-core
        peer_registry
        discovery_udp
        transfer_tcp
)

target_include_directories(sentinelfs-sprint5
    PRIVATE
        ${CMAKE_SOURCE_DIR}
)

# Install executables
install(TARGETS sentinelfs-test sentinelfs-sprint2 sentinelfs-sprint3 sentinelfs-sprint3-simple sentinelfs-sprint4 sentinelfs-sprint5
    RUNTIME DESTINATION bin
)
