cmake_minimum_required(VERSION 3.17)

project(SentinelFS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/core/include/Version.h.in
               ${CMAKE_CURRENT_SOURCE_DIR}/core/include/Version.h
               @ONLY)

add_subdirectory(core)
add_subdirectory(plugins/filesystem)
add_subdirectory(plugins/network)
add_subdirectory(plugins/storage)
add_subdirectory(plugins/ml)
add_subdirectory(app/cli)
add_subdirectory(app/daemon)
add_subdirectory(tests)

set(SENTINEL_BINARIES sentinel_daemon sentinel_cli)
set(SENTINEL_LIBS sentinel_core)

install(TARGETS ${SENTINEL_BINARIES}
        RUNTIME DESTINATION bin)

install(TARGETS ${SENTINEL_LIBS}
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)

install(DIRECTORY core/include/ DESTINATION include)
