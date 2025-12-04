cmake_minimum_required(VERSION 3.17)
project(SentinelDaemon)

set(CMAKE_CXX_STANDARD 17)

# Add Core
# add_subdirectory(../../core core_build)

find_library(SENTINEL_CORE_LIB sentinel_core PATHS ${CMAKE_BINARY_DIR}/core)

# IPC Command handlers (modular)
set(IPC_COMMAND_SOURCES
    ipc/commands/CommandHandler.cpp
    ipc/commands/StatusCommands.cpp
    ipc/commands/PeerCommands.cpp
    ipc/commands/ConfigCommands.cpp
    ipc/commands/FileCommands.cpp
    ipc/commands/TransferCommands.cpp
    ipc/commands/RelayCommands.cpp
)

# DaemonCore modular sources
set(DAEMON_CORE_SOURCES
    core/DaemonLifecycle.cpp
    core/PluginLoader.cpp
    core/WatchManager.cpp
)

add_executable(sentinel_daemon 
    sentinel_daemon.cpp
    IPCHandler.cpp
    MetricsServer.cpp
    ${IPC_COMMAND_SOURCES}
    ${DAEMON_CORE_SOURCES}
)

target_link_libraries(sentinel_daemon ${SENTINEL_CORE_LIB} dl pthread sqlite3)
target_include_directories(sentinel_daemon PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../core/include
    ../../core/network/include
    ../../core/security/include
    ../../core/sync/include
    ../../core/utils/include
    ipc/commands
)
