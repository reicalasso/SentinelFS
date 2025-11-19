#include "EventHandlers.h"
#include "DeltaEngine.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <arpa/inet.h>

namespace SentinelFS {

// --- Serialization Helpers (moved from daemon) ---

static std::vector<uint8_t> serializeSignature(const std::vector<BlockSignature>& sigs) {
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

static std::vector<BlockSignature> deserializeSignature(const std::vector<uint8_t>& data) {
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

static std::vector<uint8_t> serializeDelta(const std::vector<DeltaInstruction>& deltas) {
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

static std::vector<DeltaInstruction> deserializeDelta(const std::vector<uint8_t>& data) {
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

// --- EventHandlers Implementation ---

EventHandlers::EventHandlers(EventBus& eventBus,
                             INetworkAPI* network,
                             IStorageAPI* storage,
                             IFileAPI* filesystem,
                             const std::string& watchDirectory)
    : eventBus_(eventBus)
    , network_(network)
    , storage_(storage)
    , filesystem_(filesystem)
    , watchDirectory_(watchDirectory)
{
}

EventHandlers::~EventHandlers() {
    // EventBus subscriptions are automatically cleaned up
}

void EventHandlers::setupHandlers() {
    // Subscribe to all relevant events
    
    eventBus_.subscribe("PEER_DISCOVERED", [this](const std::any& data) {
        handlePeerDiscovered(data);
    });
    
    eventBus_.subscribe("FILE_MODIFIED", [this](const std::any& data) {
        handleFileModified(data);
    });
    
    eventBus_.subscribe("DATA_RECEIVED", [this](const std::any& data) {
        handleDataReceived(data);
    });
    
    eventBus_.subscribe("ANOMALY_DETECTED", [this](const std::any& data) {
        handleAnomalyDetected(data);
    });
}

void EventHandlers::setSyncEnabled(bool enabled) {
    syncEnabled_ = enabled;
    if (enabled) {
        std::cout << "Synchronization ENABLED" << std::endl;
    } else {
        std::cout << "Synchronization DISABLED" << std::endl;
    }
}

void EventHandlers::handlePeerDiscovered(const std::any& data) {
    try {
        std::string msg = std::any_cast<std::string>(data);
        // Format: SENTINEL_DISCOVERY|ID|PORT
        
        size_t firstPipe = msg.find('|');
        size_t secondPipe = msg.find('|', firstPipe + 1);
        
        if (secondPipe != std::string::npos) {
            std::string id = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
            std::string portStr = msg.substr(secondPipe + 1);
            int port = std::stoi(portStr);
            
            // TODO: Extract IP from discovery message
            std::string ip = "127.0.0.1"; 

            PeerInfo peer;
            peer.id = id;
            peer.ip = ip;
            peer.port = port;
            peer.lastSeen = std::time(nullptr);
            peer.status = "active";
            peer.latency = -1;

            storage_->addPeer(peer);
            
            // Auto-connect
            network_->connectToPeer(ip, port);
            
            std::cout << "Discovered peer: " << id << " at " << ip << ":" << port << std::endl;
        }
    } catch (...) {
        std::cerr << "Error handling PEER_DISCOVERED" << std::endl;
    }
}

void EventHandlers::handleFileModified(const std::any& data) {
    try {
        std::string fullPath = std::any_cast<std::string>(data);
        std::string filename = std::filesystem::path(fullPath).filename().string();
        std::cout << "File modified: " << filename << std::endl;

        // Check ignore list (to prevent sync loops)
        {
            std::lock_guard<std::mutex> lock(ignoreMutex_);
            if (ignoreList_.count(filename)) {
                auto now = std::chrono::steady_clock::now();
                if (now - ignoreList_[filename] < std::chrono::seconds(2)) {
                    std::cout << "Ignoring update for " << filename << " (recently patched)" << std::endl;
                    return;
                }
                ignoreList_.erase(filename);
            }
        }

        // Check if sync is enabled
        if (!syncEnabled_) {
            std::cout << "⚠️  Sync disabled. Skipping update broadcast." << std::endl;
            return;
        }

        // Broadcast UPDATE_AVAILABLE to all peers
        auto peers = storage_->getAllPeers();
        for (const auto& peer : peers) {
            std::string updateMsg = "UPDATE_AVAILABLE|" + filename;
            std::vector<uint8_t> payload(updateMsg.begin(), updateMsg.end());
            network_->sendData(peer.id, payload);
        }

    } catch (...) {
        std::cerr << "Error handling FILE_MODIFIED" << std::endl;
    }
}

void EventHandlers::handleDataReceived(const std::any& data) {
    try {
        // Expect pair<string, vector<uint8_t>>
        auto pair = std::any_cast<std::pair<std::string, std::vector<uint8_t>>>(data);
        std::string peerId = pair.first;
        std::vector<uint8_t> rawData = pair.second;
        
        // Extract message header
        std::string msg;
        if (rawData.size() > 256) {
            msg = std::string(rawData.begin(), rawData.begin() + 256);
        } else {
            msg = std::string(rawData.begin(), rawData.end());
        }

        // Route based on message type
        if (msg.find("UPDATE_AVAILABLE|") == 0) {
            handleUpdateAvailable(peerId, rawData);
        }
        else if (msg.find("REQUEST_DELTA|") == 0) {
            handleDeltaRequest(peerId, rawData);
        }
        else if (msg.find("DELTA_DATA|") == 0) {
            handleDeltaData(peerId, rawData);
        }
    } catch (const std::bad_any_cast& e) {
        // Ignore bad cast
    } catch (...) {
        std::cerr << "Error handling DATA_RECEIVED" << std::endl;
    }
}

void EventHandlers::handleAnomalyDetected(const std::any& data) {
    try {
        std::string anomalyType = std::any_cast<std::string>(data);
        std::cerr << "\n🚨 CRITICAL ALERT: Anomaly detected - " << anomalyType << std::endl;
        std::cerr << "🛑 Sync operations PAUSED for safety!" << std::endl;
        std::cerr << "   Manual intervention required to resume." << std::endl;
        
        syncEnabled_ = false;
        
    } catch (...) {
        std::cerr << "Error handling ANOMALY_DETECTED" << std::endl;
    }
}

// --- Delta Sync Protocol Handlers ---

void EventHandlers::handleUpdateAvailable(const std::string& peerId, 
                                          const std::vector<uint8_t>& rawData) {
    std::string fullMsg(rawData.begin(), rawData.end());
    std::string filename = fullMsg.substr(17); // Skip "UPDATE_AVAILABLE|"
    
    std::cout << "Peer " << peerId << " has update for: " << filename << std::endl;
    
    std::string localPath = watchDirectory_ + "/" + filename;
    
    // Calculate local signature
    std::vector<BlockSignature> sigs;
    if (std::filesystem::exists(localPath)) {
        sigs = DeltaEngine::calculateSignature(localPath);
    }
    
    auto serializedSig = serializeSignature(sigs);
    
    // Send delta request
    std::string header = "REQUEST_DELTA|" + filename + "|";
    std::vector<uint8_t> payload(header.begin(), header.end());
    payload.insert(payload.end(), serializedSig.begin(), serializedSig.end());
    
    network_->sendData(peerId, payload);
}

void EventHandlers::handleDeltaRequest(const std::string& peerId,
                                       const std::vector<uint8_t>& rawData) {
    std::string msg(rawData.begin(), rawData.end());
    
    size_t firstPipe = msg.find('|');
    size_t secondPipe = msg.find('|', firstPipe + 1);
    
    if (secondPipe != std::string::npos) {
        std::string filename = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        
        if (rawData.size() > secondPipe + 1) {
            std::vector<uint8_t> sigData(rawData.begin() + secondPipe + 1, rawData.end());
            auto sigs = deserializeSignature(sigData);
            
            std::cout << "Received delta request for: " << filename << " from " << peerId << std::endl;
            
            std::string localPath = watchDirectory_ + "/" + filename;
            if (std::filesystem::exists(localPath)) {
                auto deltas = DeltaEngine::calculateDelta(localPath, sigs);
                auto serializedDelta = serializeDelta(deltas);
                
                std::string header = "DELTA_DATA|" + filename + "|";
                std::vector<uint8_t> payload(header.begin(), header.end());
                payload.insert(payload.end(), serializedDelta.begin(), serializedDelta.end());
                
                network_->sendData(peerId, payload);
                std::cout << "Sent delta with " << deltas.size() << " instructions" << std::endl;
            }
        }
    }
}

void EventHandlers::handleDeltaData(const std::string& peerId,
                                    const std::vector<uint8_t>& rawData) {
    std::string msg(rawData.begin(), rawData.end());
    
    size_t firstPipe = msg.find('|');
    size_t secondPipe = msg.find('|', firstPipe + 1);
    
    if (secondPipe != std::string::npos) {
        std::string filename = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
        
        if (rawData.size() > secondPipe + 1) {
            std::vector<uint8_t> deltaData(rawData.begin() + secondPipe + 1, rawData.end());
            auto deltas = deserializeDelta(deltaData);
            
            std::cout << "Received delta data for: " << filename << " from " << peerId << std::endl;
            
            std::string localPath = watchDirectory_ + "/" + filename;
            
            // Create file if doesn't exist
            if (!std::filesystem::exists(localPath)) {
                std::ofstream outfile(localPath);
                outfile.close();
            }
            
            // Apply delta
            auto newData = DeltaEngine::applyDelta(localPath, deltas);
            
            // Write new data (mark in ignore list to prevent sync loop)
            {
                std::lock_guard<std::mutex> lock(ignoreMutex_);
                ignoreList_[filename] = std::chrono::steady_clock::now();
            }
            
            filesystem_->writeFile(localPath, newData);
            std::cout << "Patched file: " << localPath << std::endl;
        }
    }
}

} // namespace SentinelFS
