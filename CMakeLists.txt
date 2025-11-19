cmake_minimum_required(VERSION 3.17)

project(SentinelFS)

add_subdirectory(core)
add_subdirectory(plugins/filesystem)
add_subdirectory(plugins/network)
add_subdirectory(plugins/storage)
add_subdirectory(plugins/ml)
add_subdirectory(app/cli)
add_subdirectory(app/daemon)
add_subdirectory(tests)
