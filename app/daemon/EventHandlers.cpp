#include "EventHandlers.h"
#include "DeltaSyncProtocolHandler.h"
#include "FileSyncHandler.h"
#include "DeltaSerialization.h"
#include <iostream>
#include <ctime>

namespace SentinelFS {

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
    // Create specialized handlers
    fileSyncHandler_ = std::make_unique<FileSyncHandler>(network, storage, watchDirectory);
    deltaProtocolHandler_ = std::make_unique<DeltaSyncProtocolHandler>(network, storage, filesystem, watchDirectory);
    
    // Connect handlers
    deltaProtocolHandler_->setMarkAsPatchedCallback([this](const std::string& filename) {
        std::lock_guard<std::mutex> lock(ignoreMutex_);
        ignoreList_[filename] = std::chrono::steady_clock::now();
    });
}

EventHandlers::~EventHandlers() = default;

void EventHandlers::setupHandlers() {
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
    fileSyncHandler_->setSyncEnabled(enabled);
    
    if (enabled) {
        std::cout << "Synchronization ENABLED" << std::endl;
    } else {
        std::cout << "Synchronization DISABLED" << std::endl;
    }
}

void EventHandlers::handlePeerDiscovered(const std::any& data) {
    try {
        std::string msg = std::any_cast<std::string>(data);
        
        size_t firstPipe = msg.find('|');
        size_t secondPipe = msg.find('|', firstPipe + 1);
        
        if (secondPipe != std::string::npos) {
            std::string id = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
            std::string portStr = msg.substr(secondPipe + 1);
            int port = std::stoi(portStr);
            
            std::string ip = "127.0.0.1";

            PeerInfo peer;
            peer.id = id;
            peer.ip = ip;
            peer.port = port;
            peer.lastSeen = std::time(nullptr);
            peer.status = "active";
            peer.latency = -1;

            storage_->addPeer(peer);
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
        
        // Check ignore list
        std::string filename = std::filesystem::path(fullPath).filename().string();
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
        
        fileSyncHandler_->handleFileModified(fullPath);
    } catch (...) {
        std::cerr << "Error handling FILE_MODIFIED" << std::endl;
    }
}

void EventHandlers::handleDataReceived(const std::any& data) {
    try {
        auto pair = std::any_cast<std::pair<std::string, std::vector<uint8_t>>>(data);
        std::string peerId = pair.first;
        std::vector<uint8_t> rawData = pair.second;
        
        std::string msg;
        if (rawData.size() > 256) {
            msg = std::string(rawData.begin(), rawData.begin() + 256);
        } else {
            msg = std::string(rawData.begin(), rawData.end());
        }

        // Route to appropriate handler
        if (msg.find("UPDATE_AVAILABLE|") == 0) {
            deltaProtocolHandler_->handleUpdateAvailable(peerId, rawData);
        }
        else if (msg.find("REQUEST_DELTA|") == 0) {
            deltaProtocolHandler_->handleDeltaRequest(peerId, rawData);
        }
        else if (msg.find("DELTA_DATA|") == 0) {
            deltaProtocolHandler_->handleDeltaData(peerId, rawData);
        }
    } catch (const std::bad_any_cast&) {
        // Ignore
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
        
        setSyncEnabled(false);
    } catch (...) {
        std::cerr << "Error handling ANOMALY_DETECTED" << std::endl;
    }
}

} // namespace SentinelFS
