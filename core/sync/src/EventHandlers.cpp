#include "EventHandlers.h"
#include "DeltaSyncProtocolHandler.h"
#include "FileSyncHandler.h"
#include "DeltaSerialization.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <ctime>
#include <sqlite3.h>

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
    
    eventBus_.subscribe("PEER_CONNECTED", [this](const std::any& data) {
        handlePeerConnected(data);
    });
    
    eventBus_.subscribe("FILE_CREATED", [this](const std::any& data) {
        handleFileCreated(data);
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
    
    eventBus_.subscribe("PEER_DISCONNECTED", [this](const std::any& data) {
        handlePeerDisconnected(data);
    });

    eventBus_.subscribe("WATCH_ADDED", [this](const std::any& data) {
        try {
            std::string path = std::any_cast<std::string>(data);
            auto& logger = Logger::instance();
            logger.info("Received WATCH_ADDED event for: " + path, "EventHandlers");
            fileSyncHandler_->scanDirectory(path);
        } catch (const std::exception& e) {
            Logger::instance().error("Error handling WATCH_ADDED: " + std::string(e.what()), "EventHandlers");
        }
    });

    // Trigger initial scan of the watched directory
    fileSyncHandler_->scanDirectory();

    // Scan other watched folders from DB
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        const char* sql = "SELECT path FROM watched_folders WHERE status = 'active'";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (path) {
                    std::string pathStr = path;
                    // Avoid rescanning default dir if it's in DB
                    if (pathStr != watchDirectory_) {
                        fileSyncHandler_->scanDirectory(pathStr);
                    }
                }
            }
            sqlite3_finalize(stmt);
        }
    }
}

void EventHandlers::setSyncEnabled(bool enabled) {
    syncEnabled_ = enabled;
    fileSyncHandler_->setSyncEnabled(enabled);
    
    auto& logger = Logger::instance();
    if (enabled) {
        logger.info("Synchronization ENABLED", "EventHandlers");
    } else {
        logger.warn("Synchronization DISABLED", "EventHandlers");
    }
}

void EventHandlers::handlePeerDiscovered(const std::any& data) {
    try {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        
        std::string msg = std::any_cast<std::string>(data);
        
        // Format: SENTINEL_DISCOVERY|PEER_ID|TCP_PORT|SENDER_IP
        size_t firstPipe = msg.find('|');
        size_t secondPipe = msg.find('|', firstPipe + 1);
        size_t thirdPipe = msg.find('|', secondPipe + 1);
        
        if (secondPipe != std::string::npos) {
            std::string id = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
            
            std::string portStr;
            std::string ip = "127.0.0.1";  // Default fallback
            
            if (thirdPipe != std::string::npos) {
                // New format with IP
                portStr = msg.substr(secondPipe + 1, thirdPipe - secondPipe - 1);
                ip = msg.substr(thirdPipe + 1);
            } else {
                // Old format without IP
                portStr = msg.substr(secondPipe + 1);
            }
            
            int port = std::stoi(portStr);

            logger.info("Discovered peer: " + id + " at " + ip + ":" + std::to_string(port), "EventHandlers");
            metrics.incrementPeersDiscovered();

            if (!network_) {
                logger.warn("Network plugin unavailable, cannot connect to peer " + id, "EventHandlers");
                return;
            }

            // Store peer info temporarily for connection
            PeerInfo peer;
            peer.id = id;
            peer.ip = ip;
            peer.port = port;
            peer.lastSeen = std::time(nullptr);
            peer.status = "connecting";
            peer.latency = -1;
            
            storage_->addPeer(peer);
            
            // Try to connect - PEER_CONNECTED event will be triggered on success
            if (!network_->connectToPeer(ip, port)) {
                logger.warn("Failed to initiate connection to peer " + id, "EventHandlers");
                storage_->removePeer(id);
            }
        }
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Error handling PEER_DISCOVERED: ") + e.what(), "EventHandlers");
    }
}

void EventHandlers::handlePeerConnected(const std::any& data) {
    try {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        
        std::string peerId = std::any_cast<std::string>(data);
        
        logger.info("Peer connected: " + peerId + ", updating status to active", "EventHandlers");
        
        // Update peer status to active
        auto peerOpt = storage_->getPeer(peerId);
        if (peerOpt.has_value()) {
            PeerInfo peer = peerOpt.value();
            peer.status = "active";
            peer.lastSeen = std::time(nullptr);
            storage_->addPeer(peer);
            metrics.incrementPeersConnected();
            logger.info("Peer " + peerId + " is now active and ready for sync", "EventHandlers");
        } else {
            logger.warn("PEER_CONNECTED event for unknown peer: " + peerId, "EventHandlers");
        }
        
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Error handling PEER_CONNECTED: ") + e.what(), "EventHandlers");
    }
}

void EventHandlers::handleFileCreated(const std::any& data) {
    try {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        
        std::string fullPath = std::any_cast<std::string>(data);
        
        // Check ignore list
        std::string filename = std::filesystem::path(fullPath).filename().string();
        {
            std::lock_guard<std::mutex> lock(ignoreMutex_);
            if (ignoreList_.count(filename)) {
                auto now = std::chrono::steady_clock::now();
                if (now - ignoreList_[filename] < std::chrono::seconds(2)) {
                    logger.debug("Ignoring creation for " + filename + " (recently patched)", "EventHandlers");
                    return;
                }
                ignoreList_.erase(filename);
            }
        }
        
        metrics.incrementFilesModified();
        fileSyncHandler_->handleFileModified(fullPath);
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Error handling FILE_CREATED: ") + e.what(), "EventHandlers");
    }
}

void EventHandlers::handleFileModified(const std::any& data) {
    try {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        
        std::string fullPath = std::any_cast<std::string>(data);
        
        // Check ignore list
        std::string filename = std::filesystem::path(fullPath).filename().string();
        {
            std::lock_guard<std::mutex> lock(ignoreMutex_);
            if (ignoreList_.count(filename)) {
                auto now = std::chrono::steady_clock::now();
                if (now - ignoreList_[filename] < std::chrono::seconds(2)) {
                    logger.debug("Ignoring update for " + filename + " (recently patched)", "EventHandlers");
                    return;
                }
                ignoreList_.erase(filename);
            }
        }
        
        metrics.incrementFilesModified();
        fileSyncHandler_->handleFileModified(fullPath);
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Error handling FILE_MODIFIED: ") + e.what(), "EventHandlers");
    }
}

void EventHandlers::handleDataReceived(const std::any& data) {
    try {
        auto& logger = Logger::instance();
        auto pair = std::any_cast<std::pair<std::string, std::vector<uint8_t>>>(data);
        std::string peerId = pair.first;
        std::vector<uint8_t> rawData = pair.second;
        
        std::string msg;
        if (rawData.size() > 256) {
            msg = std::string(rawData.begin(), rawData.begin() + 256);
        } else {
            msg = std::string(rawData.begin(), rawData.end());
        }

        logger.debug("Received " + std::to_string(rawData.size()) + " bytes from peer " + peerId, "EventHandlers");

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
        else if (msg.find("REQUEST_FILE|") == 0) {
            deltaProtocolHandler_->handleFileRequest(peerId, rawData);
        }
        else if (msg.find("FILE_DATA|") == 0) {
            deltaProtocolHandler_->handleFileData(peerId, rawData);
        }
        else if (msg.find("DELETE_FILE|") == 0) {
            deltaProtocolHandler_->handleDeleteFile(peerId, rawData);
        }
    } catch (const std::bad_any_cast&) {
        // Ignore
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Error handling DATA_RECEIVED: ") + e.what(), "EventHandlers");
    }
}

void EventHandlers::handleAnomalyDetected(const std::any& data) {
    try {
        auto& logger = Logger::instance();
        auto& metrics = MetricsCollector::instance();
        
        std::string anomalyType = std::any_cast<std::string>(data);
        
        logger.critical("🚨 ANOMALY DETECTED: " + anomalyType, "EventHandlers");
        logger.critical("🛑 Sync operations PAUSED for safety!", "EventHandlers");
        logger.warn("Manual intervention required to resume sync.", "EventHandlers");
        
        metrics.incrementAnomalies();
        metrics.incrementSyncPaused();
        
        setSyncEnabled(false);
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Error handling ANOMALY_DETECTED: ") + e.what(), "EventHandlers");
    }
}

void EventHandlers::handlePeerDisconnected(const std::any& data) {
    try {
        auto& logger = Logger::instance();
        
        std::string peerId = std::any_cast<std::string>(data);
        
        logger.info("Peer disconnected: " + peerId + ", removing from storage", "EventHandlers");
        
        // Remove peer from storage
        if (!storage_->removePeer(peerId)) {
            logger.warn("Peer " + peerId + " was not present in storage during disconnect cleanup", "EventHandlers");
        }
        
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Error handling PEER_DISCONNECTED: ") + e.what(), "EventHandlers");
    }
}

} // namespace SentinelFS
