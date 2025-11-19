#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <csignal>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <map>
#include <fstream>

#include "PluginLoader.h"
#include "EventBus.h"
#include "IStorageAPI.h"
#include "INetworkAPI.h"
#include "IFileAPI.h"
#include "DeltaEngine.h"
#include "SessionCode.h"
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sstream>

using namespace SentinelFS;

std::atomic<bool> running{true};

// --- Serialization Helpers ---

std::vector<uint8_t> serializeSignature(const std::vector<BlockSignature>& sigs) {
    std::vector<uint8_t> buffer;
    uint32_t count = htonl(sigs.size());
    buffer.insert(buffer.end(), (uint8_t*)&count, (uint8_t*)&count + sizeof(count));

    for (const auto& sig : sigs) {
        uint32_t index = htonl(sig.index);
        uint32_t adler = htonl(sig.adler32);
        uint32_t shaLen = htonl(sig.sha256.length());
        
        buffer.insert(buffer.end(), (uint8_t*)&index, (uint8_t*)&index + sizeof(index));
        buffer.insert(buffer.end(), (uint8_t*)&adler, (uint8_t*)&adler + sizeof(adler));
        buffer.insert(buffer.end(), (uint8_t*)&shaLen, (uint8_t*)&shaLen + sizeof(shaLen));
        buffer.insert(buffer.end(), sig.sha256.begin(), sig.sha256.end());
    }
    return buffer;
}

std::vector<BlockSignature> deserializeSignature(const std::vector<uint8_t>& data) {
    std::vector<BlockSignature> sigs;
    size_t offset = 0;
    
    if (data.size() < 4) return sigs;
    
    uint32_t count;
    memcpy(&count, data.data() + offset, 4);
    count = ntohl(count);
    offset += 4;

    for (uint32_t i = 0; i < count; ++i) {
        if (offset + 12 > data.size()) break;
        
        BlockSignature sig;
        uint32_t val;
        
        memcpy(&val, data.data() + offset, 4);
        sig.index = ntohl(val);
        offset += 4;
        
        memcpy(&val, data.data() + offset, 4);
        sig.adler32 = ntohl(val);
        offset += 4;
        
        memcpy(&val, data.data() + offset, 4);
        uint32_t shaLen = ntohl(val);
        offset += 4;
        
        if (offset + shaLen > data.size()) break;
        sig.sha256 = std::string((char*)data.data() + offset, shaLen);
        offset += shaLen;
        
        sigs.push_back(sig);
    }
    return sigs;
}

std::vector<uint8_t> serializeDelta(const std::vector<DeltaInstruction>& deltas) {
    std::vector<uint8_t> buffer;
    uint32_t count = htonl(deltas.size());
    buffer.insert(buffer.end(), (uint8_t*)&count, (uint8_t*)&count + sizeof(count));

    for (const auto& delta : deltas) {
        uint8_t isLiteral = delta.isLiteral ? 1 : 0;
        buffer.push_back(isLiteral);
        
        if (delta.isLiteral) {
            uint32_t len = htonl(delta.literalData.size());
            buffer.insert(buffer.end(), (uint8_t*)&len, (uint8_t*)&len + sizeof(len));
            buffer.insert(buffer.end(), delta.literalData.begin(), delta.literalData.end());
        } else {
            uint32_t index = htonl(delta.blockIndex);
            buffer.insert(buffer.end(), (uint8_t*)&index, (uint8_t*)&index + sizeof(index));
        }
    }
    return buffer;
}

std::vector<DeltaInstruction> deserializeDelta(const std::vector<uint8_t>& data) {
    std::vector<DeltaInstruction> deltas;
    size_t offset = 0;
    
    if (data.size() < 4) return deltas;
    
    uint32_t count;
    memcpy(&count, data.data() + offset, 4);
    count = ntohl(count);
    offset += 4;

    for (uint32_t i = 0; i < count; ++i) {
        if (offset + 1 > data.size()) break;
        
        DeltaInstruction delta;
        delta.isLiteral = (data[offset++] == 1);
        
        if (delta.isLiteral) {
            if (offset + 4 > data.size()) break;
            uint32_t len;
            memcpy(&len, data.data() + offset, 4);
            len = ntohl(len);
            offset += 4;
            
            if (offset + len > data.size()) break;
            delta.literalData.assign(data.begin() + offset, data.begin() + offset + len);
            offset += len;
        } else {
            if (offset + 4 > data.size()) break;
            uint32_t index;
            memcpy(&index, data.data() + offset, 4);
            delta.blockIndex = ntohl(index);
            offset += 4;
        }
        deltas.push_back(delta);
    }
    return deltas;
}

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    running = false;
}

// IPC Command Handler
std::string handleIPCCommand(const std::string& command, 
                             IStorageAPI* storage, 
                             INetworkAPI* network,
                             std::atomic<bool>& syncEnabled,
                             int tcpPort, 
                             int discoveryPort,
                             const std::string& watchDir) {
    std::stringstream response;
    
    if (command == "STATUS") {
        response << "=== SentinelFS Daemon Status ===\n";
        response << "TCP Port: " << tcpPort << "\n";
        response << "Discovery Port: " << discoveryPort << "\n";
        response << "Watch Directory: " << watchDir << "\n";
        response << "Sync Status: " << (syncEnabled ? "ENABLED" : "PAUSED") << "\n";
        response << "Encryption: " << (network->isEncryptionEnabled() ? "ENABLED 🔒" : "Disabled") << "\n";
        
        std::string code = network->getSessionCode();
        if (!code.empty()) {
            response << "Session Code: " << SessionCode::format(code) << " ✓\n";
        } else {
            response << "Session Code: Not set ⚠️\n";
        }
        
        auto peers = storage->getAllPeers();
        response << "Connected Peers: " << peers.size() << "\n";
        
    } else if (command == "PEERS") {
        auto sortedPeers = storage->getPeersByLatency();
        response << "=== Discovered Peers ===\n";
        if (sortedPeers.empty()) {
            response << "No peers discovered yet.\n";
        } else {
            for (const auto& peer : sortedPeers) {
                response << peer.id << " @ " << peer.ip << ":" << peer.port;
                if (peer.latency >= 0) {
                    response << " [" << peer.latency << "ms]";
                }
                response << " (" << peer.status << ")\n";
            }
        }
        
    } else if (command.substr(0, 4) == "LOGS") {
        response << "Log functionality not yet implemented.\n";
        response << "(Would show last N daemon log entries)\n";
        
    } else if (command == "CONFIG") {
        response << "=== Configuration ===\n";
        response << "TCP Port: " << tcpPort << "\n";
        response << "Discovery Port: " << discoveryPort << "\n";
        response << "Watch Directory: " << watchDir << "\n";
        response << "Encryption: " << (network->isEncryptionEnabled() ? "Enabled 🔒" : "Disabled") << "\n";
        std::string code = network->getSessionCode();
        if (!code.empty()) {
            response << "Session Code: " << SessionCode::format(code) << "\n";
        } else {
            response << "Session Code: Not set (WARNING: Insecure!)\n";
        }
        
    } else if (command == "PAUSE") {
        syncEnabled = false;
        response << "Synchronization PAUSED.\n";
        
    } else if (command == "RESUME") {
        syncEnabled = true;
        response << "Synchronization RESUMED.\n";
        
    } else if (command == "STATS") {
        response << "=== Transfer Statistics ===\n";
        response << "Stats tracking not yet implemented.\n";
        response << "(Would show: files synced, bytes transferred, etc.)\n";
        
    } else if (command == "CONFLICTS") {
        // List all unresolved conflicts
        auto conflicts = storage->getUnresolvedConflicts();
        response << "=== File Conflicts ===\n";
        
        if (conflicts.empty()) {
            response << "No conflicts detected. ✓\n";
        } else {
            response << "Found " << conflicts.size() << " unresolved conflict(s):\n\n";
            for (const auto& c : conflicts) {
                response << "  ID: " << c.id << "\n";
                response << "  File: " << c.path << "\n";
                response << "  Remote Peer: " << c.remotePeerId << "\n";
                response << "  Local: " << c.localSize << " bytes @ " << c.localTimestamp << "\n";
                response << "  Remote: " << c.remoteSize << " bytes @ " << c.remoteTimestamp << "\n";
                response << "  Strategy: " << c.strategy << "\n";
                response << "  ---\n";
            }
        }
        
        auto [total, unresolved] = storage->getConflictStats();
        response << "\nTotal conflicts: " << total << " (Unresolved: " << unresolved << ")\n";
        
    } else if (command.find("RESOLVE ") == 0) {
        // Resolve a specific conflict by ID
        int conflictId = std::stoi(command.substr(8));
        if (storage->markConflictResolved(conflictId)) {
            response << "Conflict " << conflictId << " marked as resolved.\n";
        } else {
            response << "Failed to resolve conflict " << conflictId << ".\n";
        }
        
    } else {
        response << "Unknown command: " << command << "\n";
        response << "Available commands: STATUS, PEERS, CONFIG, PAUSE, RESUME, STATS, CONFLICTS, RESOLVE <id>\n";
    }
    
    return response.str();
}

// IPC Server Thread
void ipcServerThread(IStorageAPI* storage, 
                     INetworkAPI* network,
                     std::atomic<bool>& syncEnabled,
                     int tcpPort,
                     int discoveryPort,
                     const std::string& watchDir) {
    const char* SOCKET_PATH = "/tmp/sentinel_daemon.sock";
    
    // Remove old socket if exists
    unlink(SOCKET_PATH);
    
    int serverFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (serverFd < 0) {
        std::cerr << "IPC: Cannot create socket" << std::endl;
        return;
    }
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (bind(serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "IPC: Cannot bind socket" << std::endl;
        close(serverFd);
        return;
    }
    
    if (listen(serverFd, 5) < 0) {
        std::cerr << "IPC: Cannot listen" << std::endl;
        close(serverFd);
        return;
    }
    
    std::cout << "IPC Server listening on " << SOCKET_PATH << std::endl;
    
    while (running) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(serverFd, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(serverFd + 1, &readfds, NULL, NULL, &tv);
        if (activity <= 0) continue;
        
        int clientFd = accept(serverFd, NULL, NULL);
        if (clientFd < 0) continue;
        
        char buffer[1024];
        ssize_t n = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            std::string command(buffer);
            std::string response = handleIPCCommand(command, storage, network, 
                                                    syncEnabled, tcpPort, 
                                                    discoveryPort, watchDir);
            send(clientFd, response.c_str(), response.length(), 0);
        }
        
        close(clientFd);
    }
    
    close(serverFd);
    unlink(SOCKET_PATH);
}

int main(int argc, char* argv[]) {
    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "SentinelFS Daemon Starting..." << std::endl;

    // Default Configuration
    int tcpPort = 8080;
    int discoveryPort = 9999;
    std::string watchDir = "./watched_folder";
    std::string sessionCode;

    // Parse Arguments
    bool encryptionEnabled = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            tcpPort = std::stoi(argv[++i]);
        } else if (arg == "--discovery" && i + 1 < argc) {
            discoveryPort = std::stoi(argv[++i]);
        } else if (arg == "--dir" && i + 1 < argc) {
            watchDir = argv[++i];
        } else if (arg == "--session-code" && i + 1 < argc) {
            sessionCode = SessionCode::normalize(argv[++i]);
        } else if (arg == "--generate-code") {
            std::string code = SessionCode::generate();
            std::cout << "\nGenerated Session Code: " << SessionCode::format(code) << std::endl;
            std::cout << "Use this code with: --session-code " << code << std::endl;
            return 0;
        } else if (arg == "--encrypt") {
            encryptionEnabled = true;
        }
    }

    std::cout << "Configuration:" << std::endl;
    std::cout << "  TCP Port: " << tcpPort << std::endl;
    std::cout << "  Discovery Port: " << discoveryPort << std::endl;
    std::cout << "  Watch Directory: " << watchDir << std::endl;
    std::cout << "  Encryption: " << (encryptionEnabled ? "Enabled" : "Disabled") << std::endl;
    
    if (!sessionCode.empty()) {
        if (!SessionCode::isValid(sessionCode)) {
            std::cerr << "Error: Invalid session code format. Must be 6 alphanumeric characters." << std::endl;
            return 1;
        }
        std::cout << "  Session Code: " << SessionCode::format(sessionCode) << " ✓" << std::endl;
        
        if (encryptionEnabled) {
            std::cout << "  🔒 Encryption key will be derived from session code" << std::endl;
        }
    } else {
        std::cout << "  Session Code: Not set (any peer can connect)" << std::endl;
        std::cout << "  ⚠️  WARNING: For security, use --generate-code to create a session code!" << std::endl;
        
        if (encryptionEnabled) {
            std::cerr << "Error: Cannot enable encryption without a session code!" << std::endl;
            return 1;
        }
    }

    // Print CWD
    std::cout << "Current Working Directory: " << std::filesystem::current_path() << std::endl;

    EventBus eventBus;
    PluginLoader loader;

    // Load Plugins
    std::string pluginDir = "./plugins";
    std::cout << "Loading plugins from: " << pluginDir << std::endl;
    auto storagePlugin = loader.loadPlugin(pluginDir + "/storage/libstorage_plugin.so", &eventBus);
    auto networkPlugin = loader.loadPlugin(pluginDir + "/network/libnetwork_plugin.so", &eventBus);
    auto fsPlugin = loader.loadPlugin(pluginDir + "/filesystem/libfilesystem_plugin.so", &eventBus);
    auto mlPlugin = loader.loadPlugin(pluginDir + "/ml/libml_plugin.so", &eventBus);

    // Check if plugin files exist
    std::cout << "Checking plugin paths:" << std::endl;
    std::cout << "  Storage: " << (std::filesystem::exists(pluginDir + "/storage/libstorage_plugin.so") ? "Found" : "NOT FOUND") << " at " << std::filesystem::absolute(pluginDir + "/storage/libstorage_plugin.so") << std::endl;
    std::cout << "  Network: " << (std::filesystem::exists(pluginDir + "/network/libnetwork_plugin.so") ? "Found" : "NOT FOUND") << " at " << std::filesystem::absolute(pluginDir + "/network/libnetwork_plugin.so") << std::endl;
    std::cout << "  Filesystem: " << (std::filesystem::exists(pluginDir + "/filesystem/libfilesystem_plugin.so") ? "Found" : "NOT FOUND") << " at " << std::filesystem::absolute(pluginDir + "/filesystem/libfilesystem_plugin.so") << std::endl;
    std::cout << "  ML: " << (std::filesystem::exists(pluginDir + "/ml/libml_plugin.so") ? "Found" : "NOT FOUND") << " at " << std::filesystem::absolute(pluginDir + "/ml/libml_plugin.so") << std::endl;

    if (!storagePlugin || !networkPlugin || !fsPlugin) {
        std::cerr << "Failed to load one or more critical plugins. Exiting." << std::endl;
        return 1;
    }
    
    if (!mlPlugin) {
        std::cout << "Warning: ML Plugin not loaded. Continuing without anomaly detection." << std::endl;
    }

    // Cast to Interfaces
    auto storage = std::dynamic_pointer_cast<IStorageAPI>(storagePlugin);
    auto network = std::dynamic_pointer_cast<INetworkAPI>(networkPlugin);
    auto fs = std::dynamic_pointer_cast<IFileAPI>(fsPlugin);

    if (!storage || !network || !fs) {
        std::cerr << "Failed to cast plugins to interfaces. Exiting." << std::endl;
        return 1;
    }

    // Set session code if provided
    if (!sessionCode.empty()) {
        network->setSessionCode(sessionCode);
    }
    
    // Enable encryption if requested
    if (encryptionEnabled) {
        network->setEncryptionEnabled(true);
    }

    // --- State ---
    std::mutex ignoreMutex;
    std::map<std::string, std::chrono::steady_clock::time_point> ignoreList;
    std::atomic<bool> syncEnabled{true}; // Anomaly detection can disable this

    // --- Event Handlers ---

    // 1. Peer Discovery -> Storage & Connect
    eventBus.subscribe("PEER_DISCOVERED", [&](const std::any& data) {
        try {
            std::string msg = std::any_cast<std::string>(data);
            // Format: SENTINEL_DISCOVERY|ID|PORT
            // Simple parsing
            size_t firstPipe = msg.find('|');
            size_t secondPipe = msg.find('|', firstPipe + 1);
            
            if (secondPipe != std::string::npos) {
                std::string id = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
                std::string portStr = msg.substr(secondPipe + 1);
                int port = std::stoi(portStr);
                
                // TODO: Get IP from message or context (NetworkPlugin needs to provide this)
                // For now assuming localhost for demo or we need to extract from msg if we add IP there
                std::string ip = "127.0.0.1"; 

                PeerInfo peer;
                peer.id = id;
                peer.ip = ip;
                peer.port = port;
                peer.lastSeen = std::time(nullptr);
                peer.status = "active";
                peer.latency = -1; // Not measured yet

                storage->addPeer(peer);
                
                // Auto-connect
                network->connectToPeer(ip, port);
            }
        } catch (...) {
            std::cerr << "Error handling PEER_DISCOVERED" << std::endl;
        }
    });

    // 2. File Modified -> Calculate Delta -> Send
    eventBus.subscribe("FILE_MODIFIED", [&](const std::any& data) {
        try {
            std::string fullPath = std::any_cast<std::string>(data);
            std::string filename = std::filesystem::path(fullPath).filename().string();
            std::cout << "Daemon: File modified: " << filename << std::endl;

            {
                std::lock_guard<std::mutex> lock(ignoreMutex);
                if (ignoreList.count(filename)) {
                    auto now = std::chrono::steady_clock::now();
                    if (now - ignoreList[filename] < std::chrono::seconds(2)) {
                        std::cout << "Ignoring update for " << filename << " (recently patched)" << std::endl;
                        return;
                    }
                    ignoreList.erase(filename);
                }
            }

            // Check if sync is enabled (anomaly detection check)
            if (!syncEnabled) {
                std::cout << "⚠️  Sync disabled due to anomaly detection. Skipping update broadcast." << std::endl;
                return;
            }

            // Broadcast UPDATE_AVAILABLE
            auto peers = storage->getAllPeers();
            for (const auto& peer : peers) {
                std::string updateMsg = "UPDATE_AVAILABLE|" + filename;
                std::vector<uint8_t> payload(updateMsg.begin(), updateMsg.end());
                network->sendData(peer.id, payload);
            }

        } catch (...) {
            std::cerr << "Error handling FILE_MODIFIED" << std::endl;
        }
    });

    // 2b. Anomaly Detection -> Stop Sync
    eventBus.subscribe("ANOMALY_DETECTED", [&](const std::any& data) {
        try {
            std::string anomalyType = std::any_cast<std::string>(data);
            std::cerr << "\n🚨 CRITICAL ALERT: Anomaly detected - " << anomalyType << std::endl;
            std::cerr << "🛑 Sync operations PAUSED for safety!" << std::endl;
            std::cerr << "   Manual intervention required to resume." << std::endl;
            
            syncEnabled = false;
            
            // Could add: notify admin, create backup, etc.
            
        } catch (...) {
            std::cerr << "Error handling ANOMALY_DETECTED" << std::endl;
        }
    });

    // 3. Data Received -> Process
    eventBus.subscribe("DATA_RECEIVED", [&](const std::any& data) {
        try {
            // Expect pair<string, vector<uint8_t>>
            auto pair = std::any_cast<std::pair<std::string, std::vector<uint8_t>>>(data);
            std::string peerId = pair.first;
            std::vector<uint8_t> rawData = pair.second;
            
            std::string msg;
            if (rawData.size() > 256) {
                msg = std::string(rawData.begin(), rawData.begin() + 256);
            } else {
                msg = std::string(rawData.begin(), rawData.end());
            }

            if (msg.find("UPDATE_AVAILABLE|") == 0) {
                std::string fullMsg(rawData.begin(), rawData.end());
                std::string filename = fullMsg.substr(17);
                
                std::cout << "Peer " << peerId << " has update for: " << filename << std::endl;
                
                std::string localPath = watchDir + "/" + filename;
                
                std::vector<BlockSignature> sigs;
                if (std::filesystem::exists(localPath)) {
                    sigs = DeltaEngine::calculateSignature(localPath);
                }
                
                auto serializedSig = serializeSignature(sigs);
                
                std::string header = "REQUEST_DELTA|" + filename + "|";
                std::vector<uint8_t> payload(header.begin(), header.end());
                payload.insert(payload.end(), serializedSig.begin(), serializedSig.end());
                
                network->sendData(peerId, payload);
            }
            else if (msg.find("REQUEST_DELTA|") == 0) {
                size_t firstPipe = msg.find('|');
                size_t secondPipe = msg.find('|', firstPipe + 1);
                if (secondPipe != std::string::npos) {
                    std::string filename = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
                    
                    if (rawData.size() > secondPipe + 1) {
                        std::vector<uint8_t> sigData(rawData.begin() + secondPipe + 1, rawData.end());
                        auto sigs = deserializeSignature(sigData);
                        
                        std::cout << "Received delta request for: " << filename << " from " << peerId << std::endl;
                        
                        std::string localPath = watchDir + "/" + filename;
                        if (std::filesystem::exists(localPath)) {
                            auto deltas = DeltaEngine::calculateDelta(localPath, sigs);
                            auto serializedDelta = serializeDelta(deltas);
                            
                            std::string header = "DELTA_DATA|" + filename + "|";
                            std::vector<uint8_t> payload(header.begin(), header.end());
                            payload.insert(payload.end(), serializedDelta.begin(), serializedDelta.end());
                            
                            network->sendData(peerId, payload);
                            std::cout << "Sent delta with " << deltas.size() << " instructions" << std::endl;
                        }
                    }
                }
            }
            else if (msg.find("DELTA_DATA|") == 0) {
                size_t firstPipe = msg.find('|');
                size_t secondPipe = msg.find('|', firstPipe + 1);
                if (secondPipe != std::string::npos) {
                    std::string filename = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
                    
                    if (rawData.size() > secondPipe + 1) {
                        std::vector<uint8_t> deltaData(rawData.begin() + secondPipe + 1, rawData.end());
                        auto deltas = deserializeDelta(deltaData);
                        
                        std::cout << "Received delta data for: " << filename << " from " << peerId << std::endl;
                        
                        std::string localPath = watchDir + "/" + filename;
                        
                        if (!std::filesystem::exists(localPath)) {
                            std::ofstream outfile(localPath);
                            outfile.close();
                        }
                        
                        auto newData = DeltaEngine::applyDelta(localPath, deltas);
                        
                        // Write new data
                        {
                            std::lock_guard<std::mutex> lock(ignoreMutex);
                            ignoreList[filename] = std::chrono::steady_clock::now();
                        }
                        fs->writeFile(localPath, newData);
                        std::cout << "Patched file: " << localPath << std::endl;
                    }
                }
            }
        } catch (const std::bad_any_cast& e) {
             // Ignore bad cast
        } catch (...) {
            std::cerr << "Error handling DATA_RECEIVED" << std::endl;
        }
    });


    // --- Start Services ---
    network->startListening(tcpPort);
    network->startDiscovery(discoveryPort);
    
    // Start watching
    std::filesystem::create_directories(watchDir);
    fs->startWatching(watchDir);

    std::cout << "Daemon running. Press Ctrl+C to stop." << std::endl;

    // Start IPC Server Thread for CLI communication
    std::thread ipcThread([&]() {
        ipcServerThread(storage.get(), network.get(), syncEnabled, 
                       tcpPort, discoveryPort, watchDir);
    });

    // RTT Measurement Thread
    std::thread rttThread([&]() {
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(15));
            
            // Measure RTT to all connected peers
            auto peers = storage->getAllPeers();
            for (const auto& peer : peers) {
                if (network->isPeerConnected(peer.id)) {
                    int rtt = network->measureRTT(peer.id);
                    if (rtt >= 0) {
                        storage->updatePeerLatency(peer.id, rtt);
                        std::cout << "Updated latency for " << peer.id << ": " << rtt << "ms" << std::endl;
                    } else {
                        std::cout << "Failed to measure RTT for " << peer.id << std::endl;
                        // Mark peer as potentially offline
                        network->disconnectPeer(peer.id);
                    }
                } else {
                    std::cout << "Peer " << peer.id << " not connected, attempting reconnect..." << std::endl;
                    network->connectToPeer(peer.ip, peer.port);
                }
            }
        }
    });

    // Main Loop
    int loopCount = 0;
    while (running) {
        // Periodic tasks (e.g., broadcast presence)
        network->broadcastPresence(discoveryPort, tcpPort); // Broadcast TCP port
        
        // Show peer status every 30 seconds
        if (loopCount % 6 == 0) {
            auto sortedPeers = storage->getPeersByLatency();
            if (!sortedPeers.empty()) {
                std::cout << "\n=== Connected Peers (sorted by latency) ===" << std::endl;
                for (const auto& peer : sortedPeers) {
                    std::cout << "  " << peer.id << " (" << peer.ip << ":" << peer.port << ") - ";
                    if (peer.latency >= 0) {
                        std::cout << peer.latency << "ms";
                    } else {
                        std::cout << "N/A";
                    }
                    std::cout << " [" << peer.status << "]" << std::endl;
                }
                std::cout << "==========================================\n" << std::endl;
            }
        }
        
        loopCount++;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // Shutdown threads
    if (rttThread.joinable()) rttThread.join();
    if (ipcThread.joinable()) ipcThread.join();

    // Shutdown plugins
    fs->shutdown();
    network->shutdown();
    storage->shutdown();

    return 0;
}
