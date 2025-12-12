#include "PeerCommands.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include <sstream>
#include <sqlite3.h>
#include <algorithm>

namespace SentinelFS {

std::string PeerCommands::handleList() {
    std::stringstream ss;
    
    if (!ctx_.storage) {
        return "Error: Storage not initialized\n";
    }
    
    auto sortedPeers = ctx_.storage->getPeersByLatency();
    ss << "=== Discovered Peers ===\n";
    
    if (sortedPeers.empty()) {
        ss << "No peers discovered yet.\n";
    } else {
        for (const auto& peer : sortedPeers) {
            ss << peer.id << " @ " << peer.ip << ":" << peer.port;
            if (peer.latency >= 0) {
                ss << " [" << peer.latency << "ms]";
            }
            ss << " (" << peer.status << ")\n";
        }
    }
    
    return ss.str();
}

std::string PeerCommands::handleConnect(const std::string& args) {
    if (!ctx_.network) {
        return "Error: Network subsystem not initialized\n";
    }
    
    // Parse IP:PORT
    size_t colonPos = args.find(':');
    if (colonPos == std::string::npos || colonPos == 0 || colonPos == args.length() - 1) {
        return "Error: Invalid format. Use: CONNECT <ip>:<port>\n";
    }
    
    std::string ip = args.substr(0, colonPos);
    std::string portStr = args.substr(colonPos + 1);
    
    // Validate IP (basic check)
    if (ip.empty() || ip.length() > 45) {  // IPv6 max length
        return "Error: Invalid IP address\n";
    }
    
    int port;
    try {
        port = std::stoi(portStr);
        if (port < 1 || port > 65535) {
            return "Error: Port number must be between 1 and 65535\n";
        }
    } catch (const std::invalid_argument&) {
        return "Error: Invalid port number\n";
    } catch (const std::out_of_range&) {
        return "Error: Port number out of range\n";
    }
    
    if (ctx_.network->connectToPeer(ip, port)) {
        return "Success: Connecting to " + ip + ":" + std::to_string(port) + "...\n";
    } else {
        return "Error: Failed to initiate connection to " + ip + ":" + std::to_string(port) + "\n";
    }
}

std::string PeerCommands::handleAddPeer(const std::string& args) {
    if (!ctx_.network) {
        return "Error: Network subsystem not initialized\n";
    }
    
    // Parse IP:PORT
    size_t colonPos = args.find(':');
    if (colonPos == std::string::npos || colonPos == 0 || colonPos == args.length() - 1) {
        return "Error: Invalid format. Use: ADD_PEER <ip>:<port>\n";
    }
    
    std::string ip = args.substr(0, colonPos);
    std::string portStr = args.substr(colonPos + 1);
    
    // Validate IP (basic check)
    if (ip.empty() || ip.length() > 45) {  // IPv6 max length
        return "Error: Invalid IP address\n";
    }
    
    int port;
    try {
        port = std::stoi(portStr);
        if (port < 1 || port > 65535) {
            return "Error: Port number must be between 1 and 65535\n";
        }
    } catch (const std::invalid_argument&) {
        return "Error: Invalid port number\n";
    } catch (const std::out_of_range&) {
        return "Error: Port number out of range\n";
    }
    
    if (ctx_.network->connectToPeer(ip, port)) {
        return "Success: Connecting to peer " + ip + ":" + std::to_string(port) + "...\n";
    } else {
        return "Error: Failed to initiate connection to " + ip + ":" + std::to_string(port) + "\n";
    }
}

std::string PeerCommands::handleBlockPeer(const std::string& args) {
    if (args.empty()) {
        return "Error: No peer ID provided. Usage: BLOCK_PEER <peer_id>\n";
    }
    
    if (!ctx_.storage) {
        return "Error: Storage not initialized\n";
    }
    
    // Use API for proper statistics tracking
    if (ctx_.storage->blockPeer(args)) {
        return "Success: Peer blocked: " + args + "\n";
    }
    
    return "Error: Failed to block peer\n";
}

std::string PeerCommands::handleUnblockPeer(const std::string& args) {
    if (args.empty()) {
        return "Error: No peer ID provided. Usage: UNBLOCK_PEER <peer_id>\n";
    }
    
    if (!ctx_.storage) {
        return "Error: Storage not initialized\n";
    }
    
    // Use API for proper statistics tracking
    if (ctx_.storage->unblockPeer(args)) {
        return "Success: Peer unblocked: " + args + "\n";
    }
    
    return "Error: Failed to unblock peer\n";
}

std::string PeerCommands::handleClearPeers() {
    if (ctx_.storage) {
        ctx_.storage->removeAllPeers();
        return "Success: All peers cleared from database\n";
    }
    return "Error: Storage not initialized\n";
}

std::string PeerCommands::handlePeersJson() {
    if (!ctx_.storage) {
        return "{\"peers\": [], \"error\": \"Storage not initialized\"}\n";
    }
    
    std::stringstream ss;
    auto sortedPeers = ctx_.storage->getPeersByLatency();
    
    // Get local peer info to filter out ourselves
    std::string localPeerId;
    int localPort = 0;
    std::vector<std::string> connectedPeerIds;
    if (ctx_.network) {
        localPeerId = ctx_.network->getLocalPeerId();
        localPort = ctx_.network->getLocalPort();
        connectedPeerIds = ctx_.network->getConnectedPeerIds();
    }
    
    ss << "{\"peers\": [";
    bool first = true;
    for (size_t i = 0; i < sortedPeers.size(); ++i) {
        const auto& p = sortedPeers[i];
        
        // Skip our own peer ID
        if (p.id == localPeerId) {
            continue;
        }
        
        // Skip peers on our own port (different sessions on same machine)
        if (p.port == localPort && (p.ip == "127.0.0.1" || p.ip == "localhost")) {
            continue;
        }
        
        // Check if peer is actually connected
        bool isConnected = std::find(connectedPeerIds.begin(), connectedPeerIds.end(), p.id) != connectedPeerIds.end();
        std::string actualStatus = isConnected ? "active" : "disconnected";
        
        if (!first) ss << ",";
        first = false;
        
        ss << "{";
        ss << "\"id\": \"" << p.id << "\",";
        ss << "\"ip\": \"" << p.ip << "\",";
        ss << "\"port\": " << p.port << ",";
        ss << "\"latency\": " << p.latency << ",";
        ss << "\"status\": \"" << actualStatus << "\"";
        ss << "}";
    }
    ss << "]}\n";
    return ss.str();
}

} // namespace SentinelFS
