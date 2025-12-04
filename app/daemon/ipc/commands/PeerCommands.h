#pragma once

#include "CommandHandler.h"
#include <string>

namespace SentinelFS {

/**
 * @brief Handles peer management IPC commands
 * 
 * Commands: PEERS, PEERS_JSON, ADD_PEER, CONNECT, BLOCK_PEER, UNBLOCK_PEER, CLEAR_PEERS
 */
class PeerCommands : public CommandHandler {
public:
    explicit PeerCommands(CommandContext& ctx) : CommandHandler(ctx) {}
    
    // CLI commands
    std::string handleList();
    std::string handleConnect(const std::string& args);
    std::string handleAddPeer(const std::string& args);
    std::string handleBlockPeer(const std::string& args);
    std::string handleUnblockPeer(const std::string& args);
    std::string handleClearPeers();
    
    // JSON commands for GUI
    std::string handlePeersJson();
};

} // namespace SentinelFS
