#pragma once

#include "CommandHandler.h"
#include <string>

namespace SentinelFS {

/**
 * @brief Handles relay server related IPC commands
 * 
 * Commands: RELAY_CONNECT, RELAY_DISCONNECT, RELAY_STATUS, RELAY_PEERS
 */
class RelayCommands : public CommandHandler {
public:
    explicit RelayCommands(CommandContext& ctx) : CommandHandler(ctx) {}
    
    std::string handleRelayConnect(const std::string& args);
    std::string handleRelayDisconnect();
    std::string handleRelayStatus();
    std::string handleRelayPeers();
};

} // namespace SentinelFS
