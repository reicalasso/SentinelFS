#include "RelayCommands.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include <sstream>
#include <sqlite3.h>

namespace SentinelFS {

std::string RelayCommands::handleRelayConnect(const std::string& args) {
    std::istringstream iss(args);
    std::string hostPort, sessionCode;
    iss >> hostPort >> sessionCode;
    
    if (hostPort.empty() || sessionCode.empty()) {
        return "Error: Usage: RELAY_CONNECT <host:port> <session_code>\n";
    }
    
    // Parse host:port
    std::string host;
    int port = 9000; // Default port
    
    size_t colonPos = hostPort.find(':');
    if (colonPos != std::string::npos) {
        host = hostPort.substr(0, colonPos);
        try {
            port = std::stoi(hostPort.substr(colonPos + 1));
        } catch (...) {
            return "Error: Invalid port number\n";
        }
    } else {
        host = hostPort;
    }
    
    if (!ctx_.network) {
        return "Error: Network subsystem not ready\n";
    }
    
    // Store relay config
    if (ctx_.storage) {
        sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
        sqlite3_stmt* stmt;
        
        const char* updateSql = "INSERT OR REPLACE INTO config (key, value) VALUES (?, ?)";
        if (sqlite3_prepare_v2(db, updateSql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, "relay_host", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, host.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
            
            sqlite3_bind_text(stmt, 1, "relay_port", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, std::to_string(port).c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_reset(stmt);
            
            sqlite3_bind_text(stmt, 1, "relay_session_code", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, sessionCode.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }
    
    // Attempt to connect via network API
    if (ctx_.network->connectToRelay(host, port, sessionCode)) {
        std::stringstream ss;
        ss << "{\"success\":true,\"host\":\"" << host << "\",\"port\":" << port;
        ss << ",\"session\":\"" << sessionCode.substr(0, 4) << "...\"}\n";
        return ss.str();
    } else {
        return "Error: Failed to connect to relay server at " + host + ":" + std::to_string(port) + "\n";
    }
}

std::string RelayCommands::handleRelayDisconnect() {
    if (!ctx_.network) {
        return "Error: Network subsystem not ready\n";
    }
    
    ctx_.network->disconnectFromRelay();
    
    // Clear stored config
    if (ctx_.storage) {
        sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
        const char* sql = "DELETE FROM config WHERE key IN ('relay_host', 'relay_port', 'relay_session_code')";
        sqlite3_exec(db, sql, nullptr, nullptr, nullptr);
    }
    
    return "{\"success\":true,\"message\":\"Disconnected from relay server\"}\n";
}

std::string RelayCommands::handleRelayStatus() {
    std::stringstream ss;
    ss << "{";
    
    if (!ctx_.network) {
        ss << "\"error\":\"Network subsystem not ready\"";
    } else {
        ss << "\"enabled\":" << (ctx_.network->isRelayEnabled() ? "true" : "false") << ",";
        ss << "\"connected\":" << (ctx_.network->isRelayConnected() ? "true" : "false");
        
        // Get stored relay config
        if (ctx_.storage) {
            sqlite3* db = static_cast<sqlite3*>(ctx_.storage->getDB());
            sqlite3_stmt* stmt;
            
            const char* sql = "SELECT key, value FROM config WHERE key IN ('relay_host', 'relay_port', 'relay_session_code')";
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                std::string host, portStr, session;
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    std::string key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    std::string value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    
                    if (key == "relay_host") host = value;
                    else if (key == "relay_port") portStr = value;
                    else if (key == "relay_session_code") session = value;
                }
                sqlite3_finalize(stmt);
                
                if (!host.empty()) {
                    ss << ",\"host\":\"" << host << "\"";
                    ss << ",\"port\":" << (portStr.empty() ? "9000" : portStr);
                    if (!session.empty()) {
                        ss << ",\"sessionCode\":\"" << session.substr(0, 4) << "...\"";
                    }
                }
            }
        }
    }
    
    ss << "}\n";
    return ss.str();
}

std::string RelayCommands::handleRelayPeers() {
    std::stringstream ss;
    ss << "{\"peers\":[";
    
    if (!ctx_.network) {
        ss << "],\"error\":\"Network subsystem not ready\"}\n";
        return ss.str();
    }
    
    // Get relay peers from network API
    auto relayPeers = ctx_.network->getRelayPeers();
    
    bool first = true;
    for (const auto& peer : relayPeers) {
        if (!first) ss << ",";
        first = false;
        
        ss << "{";
        ss << "\"peer_id\":\"" << peer.id << "\",";
        ss << "\"public_endpoint\":\"" << peer.ip << ":" << peer.port << "\",";
        ss << "\"nat_type\":\"" << (peer.natType.empty() ? "unknown" : peer.natType) << "\",";
        ss << "\"connected_at\":\"" << peer.connectedAt << "\"";
        ss << "}";
    }
    
    ss << "]}\n";
    return ss.str();
}

} // namespace SentinelFS
