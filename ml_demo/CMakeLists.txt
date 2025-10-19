cmake_minimum_required(VERSION 3.12)
project(MLDemo)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    ml_demo.cpp
    ../src/ml/ml_analyzer.cpp
    ../src/ml/ml_data_generator.cpp
)

# Create executable
add_executable(ml_demo ${SOURCES})

# Include directories
target_include_directories(ml_demo PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

# Link libraries
target_link_libraries(ml_demo)