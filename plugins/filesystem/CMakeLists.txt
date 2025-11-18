cmake_minimum_required(VERSION 3.15)

# Common watcher library
add_subdirectory(watcher_common)

# Platform-specific watchers
if(UNIX AND NOT APPLE)
    add_subdirectory(watcher_linux)
elseif(APPLE)
    # add_subdirectory(watcher_macos)
elseif(WIN32)
    # add_subdirectory(watcher_windows)
endif()
