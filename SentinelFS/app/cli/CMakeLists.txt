project(SentinelCLI)

add_executable(sentinel_cli sentinel_cli.cpp)

target_link_libraries(sentinel_cli sentinel_core)
target_include_directories(sentinel_cli PRIVATE 
    ../../core/include
    ../../core/utils/include
)
