cmake_minimum_required(VERSION 3.17)

project(SentinelCore)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_library(sentinel_core SHARED ${SOURCES})

target_link_libraries(sentinel_core ${CMAKE_DL_LIBS})
