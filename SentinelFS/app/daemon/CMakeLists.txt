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
    ipc/commands/FalconStoreCommands.cpp
    ipc/commands/Zer0Commands.cpp
)

# IPC Handler
set(IPC_SOURCES
    ipc/IPCHandler.cpp
)

# DaemonCore modular sources
set(DAEMON_CORE_SOURCES
    core/DaemonLifecycle.cpp
    core/PluginLoader.cpp
    core/WatchManager.cpp
)

add_executable(sentinel_daemon 
    sentinel_daemon.cpp
    MetricsServer.cpp
    ${IPC_SOURCES}
    ${IPC_COMMAND_SOURCES}
    ${DAEMON_CORE_SOURCES}
)

target_link_libraries(sentinel_daemon ${SENTINEL_CORE_LIB} falconstore dl pthread sqlite3 jsoncpp)

target_include_directories(sentinel_daemon PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ipc
    ${CMAKE_SOURCE_DIR}/core/include
    ${CMAKE_SOURCE_DIR}/core/utils/include
    ${CMAKE_SOURCE_DIR}/core/network/include
    ${CMAKE_SOURCE_DIR}/core/security/include
    ${CMAKE_SOURCE_DIR}/core/storage/include
    ${CMAKE_SOURCE_DIR}/core/sync/include
    ${CMAKE_SOURCE_DIR}/app/daemon
    ${CMAKE_SOURCE_DIR}/app/daemon/core
    ${CMAKE_SOURCE_DIR}/app/daemon/ipc
    ${CMAKE_SOURCE_DIR}/app/daemon/ipc/commands
    ${CMAKE_SOURCE_DIR}/plugins/zer0/include
)
