#include "EventHandlers.h"
#include "DeltaSyncProtocolHandler.h"
#include "FileSyncHandler.h"
#include "SyncPipeline.h"
#include "DeltaSerialization.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <ctime>
#include <sqlite3.h>
#include <thread>
#include <chrono>

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
    
    // Create 7-stage sync pipeline
    syncPipeline_ = std::make_unique<Sync::SyncPipeline>(network, storage, filesystem, watchDirectory);
    
    // Connect handlers - mark files as patched to prevent sync loops
    auto markAsPatchedCallback = [this](const std::string& filename) {
        std::lock_guard<std::mutex> lock(ignoreMutex_);
        ignoreList_[filename] = std::chrono::steady_clock::now();
    };
    
    deltaProtocolHandler_->setMarkAsPatchedCallback(markAsPatchedCallback);
    syncPipeline_->setMarkAsPatchedCallback(markAsPatchedCallback);
    
    // Setup progress callback for new pipeline
    syncPipeline_->setCompleteCallback([](const std::string& transferId, bool success, const std::string& error) {
        auto& logger = Logger::instance();
        if (success) {
            logger.info("✅ Transfer " + transferId + " completed successfully", "SyncPipeline");
        } else {
            logger.error("❌ Transfer " + transferId + " failed: " + error, "SyncPipeline");
        }
    });
    
    // Check environment variable to enable new pipeline
    const char* usePipeline = std::getenv("SENTINEL_USE_NEW_PIPELINE");
    useNewPipeline_ = (usePipeline != nullptr && std::string(usePipeline) == "1");
    
    if (useNewPipeline_) {
        Logger::instance().info("🚀 Using new 7-stage sync pipeline", "EventHandlers");
    }
    
    // Setup offline queue for operations when peers are unavailable
    setupOfflineQueue();
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

    eventBus_.subscribe("FILE_DELETED", [this](const std::any& data) {
        handleFileDeleted(data);
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
    
    // Subscribe to transport-specific disconnect events
    eventBus_.subscribe("QUIC_PEER_DISCONNECTED", [this](const std::any& data) {
        handlePeerDisconnected(data);
    });
    
    eventBus_.subscribe("WEBRTC_PEER_DISCONNECTED", [this](const std::any& data) {
        handlePeerDisconnected(data);
    });
    
    eventBus_.subscribe("RELAY_PEER_DISCONNECTED", [this](const std::any& data) {
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
        const char* sql = "SELECT path FROM watched_folders WHERE status_id = 1";
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
        // Process any pending changes that accumulated while paused
        processPendingChanges();
    } else {
        logger.warn("Synchronization DISABLED - changes will be queued as pending", "EventHandlers");
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
        
        // Event data format: "peerId|ip|port"
        std::string eventData = std::any_cast<std::string>(data);
        
        // Parse event data
        std::string peerId, peerIp;
        int peerPort = 0;
        
        size_t pos1 = eventData.find('|');
        if (pos1 != std::string::npos) {
            peerId = eventData.substr(0, pos1);
            size_t pos2 = eventData.find('|', pos1 + 1);
            if (pos2 != std::string::npos) {
                peerIp = eventData.substr(pos1 + 1, pos2 - pos1 - 1);
                try {
                    peerPort = std::stoi(eventData.substr(pos2 + 1));
                } catch (...) {
                    peerPort = 0;
                }
            }
        } else {
            // Fallback for old format (just peerId)
            peerId = eventData;
            peerIp = "0.0.0.0";
            peerPort = 0;
        }
        
        logger.info("Peer connected: " + peerId + " at " + peerIp + ":" + std::to_string(peerPort), "EventHandlers");
        
        // Update peer status to active
        auto peerOpt = storage_->getPeer(peerId);
        if (peerOpt.has_value()) {
            PeerInfo peer = peerOpt.value();
            peer.status = "active";
            peer.lastSeen = std::time(nullptr);
            // Update IP/port if we have valid values
            if (!peerIp.empty() && peerIp != "0.0.0.0") {
                peer.ip = peerIp;
            }
            if (peerPort > 0) {
                peer.port = peerPort;
            }
            storage_->addPeer(peer);
            metrics.incrementPeersConnected();
            logger.info("Peer " + peerId + " is now active and ready for sync", "EventHandlers");
        } else {
            // Peer not in database (incoming connection) - add with connection info
            logger.info("Adding new peer from incoming connection: " + peerId, "EventHandlers");
            PeerInfo newPeer;
            newPeer.id = peerId;
            newPeer.ip = peerIp;
            newPeer.port = peerPort;
            newPeer.lastSeen = std::time(nullptr);
            newPeer.status = "active";
            newPeer.latency = -1;
            storage_->addPeer(newPeer);
            metrics.incrementPeersConnected();
            logger.info("Peer " + peerId + " added at " + peerIp + ":" + std::to_string(peerPort), "EventHandlers");
        }
        
        // Trigger file sync after peer connection is established
        if (syncEnabled_) {
            if (useNewPipeline_) {
                // New pipeline: initiate handshake first, then sync
                logger.info("Initiating handshake with newly connected peer: " + peerId, "EventHandlers");
                std::thread([this, peerId]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    if (syncPipeline_->initiateHandshake(peerId)) {
                        // After handshake, broadcast files
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        fileSyncHandler_->broadcastAllFilesToPeer(peerId);
                    }
                }).detach();
            } else {
                // Legacy: broadcast all files directly
                logger.info("Triggering file sync to newly connected peer: " + peerId, "EventHandlers");
                std::thread([this, peerId]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    fileSyncHandler_->broadcastAllFilesToPeer(peerId);
                }).detach();
            }
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
        
        // Queue for resume if paused
        if (!syncEnabled_) {
            std::lock_guard<std::mutex> lock(pendingMutex_);
            pendingChanges_.push_back(fullPath);
            logger.info("⏸️  Sync paused - queued file creation: " + filename + " (" + std::to_string(pendingChanges_.size()) + " pending)", "EventHandlers");
        }
        
        // ALWAYS process file (updates DB even when paused, broadcasts only when enabled)
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
        
        // Queue for resume if paused
        if (!syncEnabled_) {
            std::lock_guard<std::mutex> lock(pendingMutex_);
            pendingChanges_.push_back(fullPath);
            logger.info("⏸️  Sync paused - queued file modification: " + filename + " (" + std::to_string(pendingChanges_.size()) + " pending)", "EventHandlers");
        }
        
        // ALWAYS process file (updates DB even when paused, broadcasts only when enabled)
        metrics.incrementFilesModified();
        fileSyncHandler_->handleFileModified(fullPath);
        
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Error handling FILE_MODIFIED: ") + e.what(), "EventHandlers");
    }
}

void EventHandlers::handleFileDeleted(const std::any& data) {
    try {
        auto& logger = Logger::instance();
        
        std::string fullPath = std::any_cast<std::string>(data);
        std::string filename = std::filesystem::path(fullPath).filename().string();
        
        // Check ignore list (reuse same mutex/list for simplicity, though usually for patches)
        {
            std::lock_guard<std::mutex> lock(ignoreMutex_);
            if (ignoreList_.count(filename)) {
                auto now = std::chrono::steady_clock::now();
                if (now - ignoreList_[filename] < std::chrono::seconds(2)) {
                    logger.debug("Ignoring deletion for " + filename + " (recently processed)", "EventHandlers");
                    return;
                }
                ignoreList_.erase(filename);
            }
        }

        // ALWAYS process file deletion (updates DB even when paused, broadcasts only when enabled)
        fileSyncHandler_->handleFileDeleted(fullPath);
        
    } catch (const std::exception& e) {
        Logger::instance().error(std::string("Error handling FILE_DELETED: ") + e.what(), "EventHandlers");
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

        // Check for new wire protocol (binary messages start with magic bytes)
        constexpr uint32_t PROTOCOL_MAGIC = 0x53454E54;  // "SENT"
        if (rawData.size() >= 4) {
            uint32_t magic = *reinterpret_cast<const uint32_t*>(rawData.data());
            if (magic == PROTOCOL_MAGIC) {
                // New 7-stage pipeline message
                logger.debug("Routing to SyncPipeline (wire protocol)", "EventHandlers");
                syncPipeline_->handleMessage(peerId, rawData);
                return;
            }
        }

        // Legacy text-based protocol - route to appropriate handler
        if (msg.find("UPDATE_AVAILABLE|") == 0) {
            if (useNewPipeline_) {
                // Convert to new pipeline format
                syncPipeline_->handleMessage(peerId, rawData);
            } else {
                deltaProtocolHandler_->handleUpdateAvailable(peerId, rawData);
            }
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

void EventHandlers::processPendingChanges() {
    auto& logger = Logger::instance();
    
    std::vector<std::string> changesToProcess;
    
    // Get pending changes from memory (files added during this session while paused)
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        changesToProcess = std::move(pendingChanges_);
        pendingChanges_.clear();
    }
    
    // Also get pending changes from database (synced=0)
    // This handles files that were pending when app was closed
    if (storage_) {
        sqlite3* db = static_cast<sqlite3*>(storage_->getDB());
        const char* sql = "SELECT path FROM files WHERE synced = 0";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (path) {
                    changesToProcess.push_back(path);
                }
            }
            sqlite3_finalize(stmt);
        }
    }
    
    if (changesToProcess.empty()) {
        logger.debug("No pending changes to broadcast", "EventHandlers");
        return;
    }
    
    logger.info("▶️  Resume: Broadcasting " + std::to_string(changesToProcess.size()) + " pending file change(s)", "EventHandlers");
    
    // Remove duplicates - keep only the last occurrence of each path
    std::map<std::string, size_t> pathIndices;
    for (size_t i = 0; i < changesToProcess.size(); ++i) {
        pathIndices[changesToProcess[i]] = i;
    }
    
    std::vector<std::string> uniqueChanges;
    for (const auto& pair : pathIndices) {
        uniqueChanges.push_back(pair.first);
    }
    
    if (uniqueChanges.size() != changesToProcess.size()) {
        logger.info("📋 After deduplication: " + std::to_string(uniqueChanges.size()) + " unique file(s) to broadcast", "EventHandlers");
    }
    
    // Broadcast each unique file change
    // Note: Files were already added to database when paused, now we just broadcast
    for (const auto& fullPath : uniqueChanges) {
        try {
            // Verify file still exists before broadcasting
            if (!std::filesystem::exists(fullPath)) {
                std::string filename = std::filesystem::path(fullPath).filename().string();
                logger.warn("Skipping pending broadcast for " + filename + " (file no longer exists)", "EventHandlers");
                continue;
            }
            
            std::string filename = std::filesystem::path(fullPath).filename().string();
            long long fileSize = std::filesystem::file_size(fullPath);
            logger.info("📡 Broadcasting pending file: " + filename + " (" + std::to_string(fileSize) + " bytes)", "EventHandlers");
            
            // Broadcast only (database was already updated when file was modified)
            fileSyncHandler_->broadcastUpdate(fullPath);
            
        } catch (const std::exception& e) {
            logger.error("Error broadcasting pending change for " + fullPath + ": " + std::string(e.what()), "EventHandlers");
        }
    }
    
    logger.info("✅ Finished broadcasting pending changes", "EventHandlers");
}

void EventHandlers::setupOfflineQueue() {
    auto& logger = Logger::instance();
    
    offlineQueue_ = std::make_unique<sfs::sync::OfflineQueue>();
    
    // Set processor callback
    offlineQueue_->setProcessor([this](const sfs::sync::QueuedOperation& op) {
        auto& logger = Logger::instance();
        
        // Check if we have peers to sync with
        auto peers = storage_->getAllPeers();
        if (peers.empty()) {
            logger.debug("No peers available, keeping operation in queue", "OfflineQueue");
            return false; // Retry later
        }
        
        std::string filename = std::filesystem::path(op.filePath).filename().string();
        
        switch (op.type) {
            case sfs::sync::OperationType::Create:
            case sfs::sync::OperationType::Update:
                logger.info("Processing queued update: " + filename, "OfflineQueue");
                if (fileSyncHandler_->updateFileInDatabase(op.filePath)) {
                    fileSyncHandler_->broadcastUpdate(op.filePath);
                    return true;
                }
                break;
                
            case sfs::sync::OperationType::Delete:
                logger.info("Processing queued delete: " + filename, "OfflineQueue");
                // Remove from database and broadcast delete to peers
                if (storage_->removeFile(op.filePath)) {
                    fileSyncHandler_->broadcastDelete(op.filePath);
                    return true;
                }
                break;
                
            case sfs::sync::OperationType::Rename:
                logger.info("Processing queued rename: " + filename + " -> " + op.targetPath, "OfflineQueue");
                // Rename is handled as: delete old path, then create/update new path
                // First, remove old path from database and broadcast delete
                if (storage_->removeFile(op.filePath)) {
                    fileSyncHandler_->broadcastDelete(op.filePath);
                }
                // Then, update new path in database and broadcast update
                if (fileSyncHandler_->updateFileInDatabase(op.targetPath)) {
                    fileSyncHandler_->broadcastUpdate(op.targetPath);
                    return true;
                }
                break;
        }
        
        return false;
    });
    
    // Start the queue processor
    offlineQueue_->start();
    logger.info("Offline queue initialized and started", "EventHandlers");
}

} // namespace SentinelFS
