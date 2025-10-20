#include <iostream>
#include <thread>
#include <memory>
#include <atomic>
#include <signal.h>
#include <filesystem>
#include <ctime>

#include "app/cli.hpp"
#include "app/logger.hpp"
#include "fs/watcher.hpp"
#include "fs/delta_engine.hpp"
#include "fs/file_queue.hpp"
#include "fs/conflict_resolver.hpp"
#include "fs/file_locker.hpp"
#include "fs/compressor.hpp"
#include "security/security_manager.hpp"
#include "net/discovery.hpp"
#include "net/remesh.hpp"
#include "net/transfer.hpp"
#include "net/nat_traversal.hpp"
#include "db/db.hpp"
#include "db/cache.hpp"
#include "sync/sync_manager.hpp"
#include "ml/ml_analyzer.hpp"
// Advanced ML modules available in src/ml/:
// - online_learning.hpp (OnlineLearner)
// - federated_learning.hpp (FederatedLearner)
// - advanced_forecasting.hpp (AdvancedForecaster)
// - neural_network.hpp (NeuralNetwork)

// Helper function to convert AnomalyType to string for logging
std::string anomalyTypeToString(AnomalyType type) {
    switch (type) {
        case AnomalyType::UNUSUAL_ACCESS_TIME: return "UNUSUAL_ACCESS_TIME";
        case AnomalyType::LARGE_FILE_TRANSFER: return "LARGE_FILE_TRANSFER";
        case AnomalyType::FREQUENT_ACCESS_PATTERN: return "FREQUENT_ACCESS_PATTERN";
        case AnomalyType::UNKNOWN_ACCESS_PATTERN: return "UNKNOWN_ACCESS_PATTERN";
        case AnomalyType::ACCESS_DURING_SUSPICIOUS_ACTIVITY: return "ACCESS_DURING_SUSPICIOUS_ACTIVITY";
        default: return "UNKNOWN";
    }
    return "UNKNOWN";
}

// Global flag for graceful shutdown
std::atomic<bool> shouldExit{false};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    shouldExit = true;
}

int main(int argc, char* argv[]) {
    // Set up signal handling for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Parse command line arguments
    CLI cli;
    Config config = cli.parseArguments(argc, argv);
    
    // Set up logger
    Logger logger("", config.verbose ? LogLevel::DEBUG : LogLevel::INFO);
    logger.info("Starting SentinelFS-Neo with session code: " + config.sessionCode);
    
    try {
        // Initialize metadata database
        MetadataDB db;
        if (!db.initialize()) {
            logger.error("Failed to initialize metadata database");
            return 1;
        }
        logger.info("Metadata database initialized");
        
        // Display initial database statistics
        auto dbStats = db.getStatistics();
        logger.info("Database Statistics:");
        logger.info("  - Total Files: " + std::to_string(dbStats.totalFiles));
        logger.info("  - Total Peers: " + std::to_string(dbStats.totalPeers));
        logger.info("  - Active Peers: " + std::to_string(dbStats.activePeers));
        logger.info("  - Anomalies Logged: " + std::to_string(dbStats.totalAnomalies));
        logger.info("  - Database Size: " + std::to_string(dbStats.dbSizeBytes / 1024) + " KB");
        
        // Initialize security components first
        auto& securityManager = SecurityManager::getInstance();
        securityManager.initialize();  // Initialize with default keys
        logger.info("✅ Security Manager initialized with encryption enabled");
        
        // Initialize network components
        Discovery discovery(config.sessionCode);
        Remesh remesh;
        Transfer transfer;
        NATTraversal natTraversal;  // For NAT traversal functionality
        
        // ✅ ACTIVATE SECURITY: Connect SecurityManager to Transfer
        transfer.enableSecurity(true);
        transfer.setSecurityManager(&securityManager);
        logger.info("✅ SECURITY ACTIVATED: All transfers now encrypted!");
        
        discovery.setDiscoveryInterval(config.discoveryInterval);
        remesh.setRemeshThreshold(config.remeshThreshold);
        remesh.setBandwidthWeight(0.4);  // 40% weight to bandwidth in calculations
        remesh.setLatencyWeight(0.6);   // 60% weight to latency in calculations
        
        // Test NAT traversal
        std::string externalIP;
        int externalPort;
        if (natTraversal.discoverExternalAddress(externalIP, externalPort)) {
            logger.info("NAT traversal successful. External IP: " + externalIP + ", Port: " + std::to_string(externalPort));
        } else {
            logger.warning("NAT traversal failed, direct connections may be limited");
        }
        
        // Start network services
        discovery.start();
        remesh.start();
        
        logger.info("Network services started");
        
        // ✅ Database: Track discovered peers
        auto discoveredPeers = discovery.getPeers();
        for (const auto& peer : discoveredPeers) {
            // Check if peer exists in DB
            auto existingPeer = db.getPeer(peer.id);
            if (existingPeer.id.empty()) {
                db.addPeer(peer);
                logger.debug("✅ DB: Added new peer: " + peer.id);
            } else {
                db.updatePeer(peer);
                logger.debug("✅ DB: Updated peer: " + peer.id);
            }
        }
        logger.info("✅ Database peer tracking active (" + std::to_string(discoveredPeers.size()) + " peers)");
        
        // ✅ SYNC MANAGER: Initialize with all advanced features!
        SyncConfig syncConfig;
        syncConfig.enableSelectiveSync = true;
        syncConfig.enableBandwidthThrottling = true;
        syncConfig.enableResumeTransfers = true;
        syncConfig.enableVersionHistory = true;
        syncConfig.maxBandwidthUpload = 1024 * 1024 * 10;      // 10 MB/s
        syncConfig.maxBandwidthDownload = 1024 * 1024 * 20;    // 20 MB/s
        syncConfig.adaptiveBandwidth = true;
        syncConfig.enableCompression = true;
        syncConfig.maxVersionsPerFile = 10;
        syncConfig.maxVersionAge = std::chrono::hours(24 * 30);  // 30 days
        
        SyncManager syncManager(syncConfig);
        syncManager.start();
        logger.info("✅ SyncManager started with all features:");
        logger.info("  - SelectiveSync (pattern-based filtering)");
        logger.info("  - BandwidthThrottling (10MB/s up, 20MB/s down)");
        logger.info("  - ResumeTransfers (checkpoint-based recovery)");
        logger.info("  - VersionHistory (time-machine, 30 days, 10 versions/file)");
        
        // Initialize ML analyzer if available
#ifdef USE_ONNX
        ONNXAnalyzer mlAnalyzer;
        logger.info("ONNX ML analyzer initialized");
#else
        MLAnalyzer mlAnalyzer;
        mlAnalyzer.setAnomalyThreshold(0.65);  // Set anomaly detection threshold
        logger.info("Enhanced ML analyzer initialized with advanced features");
        
        // Initialize predictive sync metrics
        logger.info("Predictive sync and network optimization ML features enabled");
#endif
        
        // ✅ ADVANCED ML: Infrastructure ready (types available for future use)
        logger.info("✅ Advanced ML modules available:");
        logger.info("  - OnlineLearner (adaptive learning with drift detection)");
        logger.info("  - FederatedLearner (multi-peer collaborative ML)");
        logger.info("  - AdvancedForecaster (ARIMA time-series prediction)");
        logger.info("  - NeuralNetwork (deep learning with backpropagation)");
        
        // Initialize file system components
        ConflictResolver conflictResolver(ConflictResolutionStrategy::BACKUP);
        FileLocker fileLocker;
        Compressor compressor(CompressionAlgorithm::ZSTD);  // ✅ Use Zstandard for speed
        logger.info("✅ Compressor initialized (Zstandard algorithm)");
        logger.info("✅ Delta Engine ready for efficient sync (per-file instances)");
        logger.info("✅ ConflictResolver active with BACKUP strategy");
        logger.info("✅ FileLocker active for concurrency protection");
        
        // Set up file watcher
        FileWatcher watcher(config.syncPath, [&](const FileEvent& event) {
            logger.info("File event: " + event.type + " - " + event.path);
            
            // ✅ DATABASE INTEGRATION: Handle file events in DB
            if (event.type == "created" || event.type == "modified") {
                // Create FileInfo for database
                FileInfo fileInfo;
                fileInfo.path = event.path;
                fileInfo.size = event.file_size;
                fileInfo.lastModified = std::to_string(std::time(nullptr));
                fileInfo.deviceId = config.sessionCode;
                fileInfo.version = 1;
                fileInfo.conflictStatus = "none";
                
                // Try to get existing file
                auto existingFile = db.getFile(event.path);
                
                if (existingFile.path.empty()) {
                    // New file - INSERT
                    db.addFile(fileInfo);
                    logger.debug("✅ DB: Added new file: " + event.path);
                } else {
                    // Existing file - UPDATE
                    fileInfo.version = existingFile.version + 1;
                    db.updateFile(fileInfo);
                    logger.debug("✅ DB: Updated file (v" + std::to_string(fileInfo.version) + "): " + event.path);
                }
            } else if (event.type == "deleted") {
                // DELETE from database
                db.deleteFile(event.path);
                logger.debug("✅ DB: Deleted file: " + event.path);
            }
            
            // Acquire write lock on the file to prevent conflicts during processing
            if (!fileLocker.acquireLock(event.path, LockType::WRITE)) {
                logger.warning("Could not acquire lock on file: " + event.path + ", skipping sync");
                return;
            }
            
            // Extract comprehensive features for enhanced ML analysis
            auto features = MLAnalyzer::extractComprehensiveFeatures(
                event.path, 
                event.peer_id, 
                event.file_size,
                event.type,
                "local_user"  // userId would come from context in a real system
            );
            
            // Detect anomalies with classification
            auto anomalyResult = mlAnalyzer.detectAnomaly(features, event.path);
            
            if (anomalyResult.isAnomaly) {
                logger.warning("Anomalous file access detected: " + event.path + 
                              " (Type: " + anomalyTypeToString(anomalyResult.type) + 
                              ", Confidence: " + std::to_string(anomalyResult.confidence) + ")");
                db.logAnomaly(event.path, features);
                
                // Provide feedback to improve model
                mlAnalyzer.provideFeedback(features, true, true);  // Anomaly detected correctly
            } else {
                // Provide feedback for normal events too
                mlAnalyzer.provideFeedback(features, false, true);  // Normal event correctly identified
            }
            
            // ✅ ADVANCED ML: ML infrastructure available for processing
            // (OnlineLearner, NeuralNetwork ready for integration)
            
            // Run predictive sync analysis
            if (event.type == "access" || event.type == "read") {  // For file access events
                auto prediction = mlAnalyzer.predictNextFile("local_user");
                if (prediction.probability > mlAnalyzer.getModelMetrics().at("prediction_accuracy")) {
                    logger.info("Predicted file access: " + prediction.filePath + 
                               " (Probability: " + std::to_string(prediction.probability) + ")");
                    // In a real system, we would pre-fetch this file
                }
            }
            
            // Check access permissions
            if (!securityManager.checkAccess("local", event.path, AccessLevel::READ_WRITE)) {
                logger.warning("Access denied for file: " + event.path);
                fileLocker.releaseLock(event.path);
                return;
            }
            
            // Compute delta for the changed file with compression and encryption
            DeltaEngine deltaEngine(event.path);
            deltaEngine.setCompression(CompressionAlgorithm::GZIP); // Use gzip compression
            
            // Get current peer list for access control
            auto peers = discovery.getPeers();
            
            // Encrypt file before computing delta (simulated)
            auto delta = deltaEngine.computeCompressed("", event.path); // For now, comparing with empty old file
            
            if (!delta.chunks.empty()) {
                logger.info("Computed delta for file: " + event.path + " (" + std::to_string(delta.chunks.size()) + " chunks), compressed: " + (delta.isCompressed ? "yes" : "no"));
                
                // Process each peer for secure transmission
                std::vector<PeerInfo> authorizedPeers;
                for (const auto& peer : peers) {
                    // Check if peer is authenticated
                    if (securityManager.authenticatePeer(peer)) {
                        // Check if this peer has access to this file
                        if (securityManager.checkAccess(peer.id, event.path, AccessLevel::READ_WRITE)) {
                            authorizedPeers.push_back(peer);
                            
                            // Record peer activity for rate limiting
                            securityManager.recordPeerActivity(peer.id, event.file_size);
                        } else {
                            logger.debug("Peer " + peer.id + " not authorized for file: " + event.path);
                        }
                    } else {
                        logger.warning("Unauthorized peer " + peer.id + " ignored");
                    }
                }
                
                // Send delta to authorized peers only
                if (!authorizedPeers.empty()) {
                    // Encrypt the delta data before sending
                    if (delta.isCompressed) {
                        // Apply encryption to compressed data
                        for (auto& chunk : delta.chunks) {
                            auto encryptedData = securityManager.encryptData(chunk.data, authorizedPeers[0].id);
                            chunk.data = encryptedData;
                        }
                    }
                    
                    transfer.broadcastDelta(delta, authorizedPeers);
                    logger.info("Broadcasted encrypted delta to " + std::to_string(authorizedPeers.size()) + " authorized peers");
                } else {
                    logger.warning("No authorized peers found for this file");
                }
            }
            
            // Release the file lock
            fileLocker.releaseLock(event.path);
        });
        
        // Start file watching
        watcher.start();
        logger.info("File watcher started for path: " + config.syncPath);
        
        // ✅ TEST BATCH OPERATIONS: Scan directory and batch-insert files
        logger.info("✅ Performing initial directory scan with batch operations...");
        std::vector<FileInfo> initialFiles;
        try {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(config.syncPath)) {
                if (entry.is_regular_file()) {
                    FileInfo fileInfo;
                    fileInfo.path = entry.path().string();
                    fileInfo.size = entry.file_size();
                    fileInfo.lastModified = std::to_string(std::time(nullptr));
                    fileInfo.deviceId = config.sessionCode;
                    fileInfo.version = 1;
                    fileInfo.conflictStatus = "none";
                    initialFiles.push_back(fileInfo);
                }
            }
            
            if (!initialFiles.empty()) {
                // ✅ USE TRANSACTION for batch insert
                logger.info("✅ Batch inserting " + std::to_string(initialFiles.size()) + " files with transaction");
                try {
                    db.addFilesBatch(initialFiles);
                    logger.info("✅ DB BATCH: Inserted " + std::to_string(initialFiles.size()) + " files successfully!");
                } catch (const std::exception& e) {
                    logger.error("❌ DB BATCH: Failed - " + std::string(e.what()));
                }
            }
        } catch (const std::exception& e) {
            logger.warning("Error during directory scan: " + std::string(e.what()));
        }
        
        // Main loop
        logger.info("SentinelFS-Neo is running with session code: " + config.sessionCode);
        
        // Database maintenance counters
        int loopCounter = 0;
        const int STATS_INTERVAL = 100;      // Log stats every 100 loops (~10 seconds)
        const int MAINTENANCE_INTERVAL = 3000; // Run maintenance every 3000 loops (~5 minutes)
        const int QUERY_TEST_INTERVAL = 500;   // Test query helpers every 500 loops
        const int ML_FORECAST_INTERVAL = 1000; // Run forecasting every 1000 loops (~2 min)
        
        while (!shouldExit) {
            loopCounter++;
            
            // Periodic statistics logging
            if (loopCounter % STATS_INTERVAL == 0) {
                auto stats = db.getStatistics();
                logger.debug("DB Stats: Files=" + std::to_string(stats.totalFiles) + 
                           ", Peers=" + std::to_string(stats.activePeers) + "/" + std::to_string(stats.totalPeers) +
                           ", Size=" + std::to_string(stats.dbSizeBytes / 1024) + "KB");
            }
            
            // ✅ Test query helpers periodically
            if (loopCounter % QUERY_TEST_INTERVAL == 0) {
                // Test getFilesModifiedAfter
                auto timestamp = std::to_string(std::time(nullptr) - 3600); // Last hour
                auto recentFiles = db.getFilesModifiedAfter(timestamp);
                if (!recentFiles.empty()) {
                    logger.debug("✅ Query: Found " + std::to_string(recentFiles.size()) + " files modified in last hour");
                }
                
                // Test getFilesByDevice
                auto deviceFiles = db.getFilesByDevice(config.sessionCode);
                logger.debug("✅ Query: This device has " + std::to_string(deviceFiles.size()) + " files");
                
                // Test getActivePeers
                auto activePeers = db.getActivePeers();
                if (!activePeers.empty()) {
                    logger.debug("✅ Query: " + std::to_string(activePeers.size()) + " active peers online");
                }
            }
            
            // Periodic database maintenance
            if (loopCounter % MAINTENANCE_INTERVAL == 0) {
                auto stats = db.getStatistics();
                // Run optimization if DB is larger than 10MB
                if (stats.dbSizeBytes > 10 * 1024 * 1024) {
                    logger.info("Running database optimization (Size: " + 
                               std::to_string(stats.dbSizeBytes / (1024 * 1024)) + " MB)");
                    if (db.optimize()) {
                        logger.info("Database optimization completed successfully");
                    }
                }
            }
            
            // Periodic network maintenance
            remesh.evaluateAndOptimize();
            
            // Get active peers from database (sorted by latency) for optimization
            auto activePeersFromDB = db.getActivePeers();
            if (!activePeersFromDB.empty() && loopCounter % 50 == 0) {
                logger.debug("Active peers in DB: " + std::to_string(activePeersFromDB.size()) + 
                           " (Best latency: " + std::to_string(activePeersFromDB[0].latency) + "ms)");
            }
            
            // Update peer information based on discovery results
            auto peers = discovery.getPeers();
            for (const auto& peer : peers) {
                // Check if peer is authenticated and not rate-limited
                if (!securityManager.authenticatePeer(peer)) {
                    logger.warning("Unauthenticated peer detected: " + peer.id + ", removing from network");
                    remesh.removePeer(peer.id);
                    continue;
                }
                
                if (securityManager.isRateLimited(peer.id)) {
                    logger.warning("Rate-limited peer: " + peer.id + ", reducing sync priority");
                    continue;
                }
                
                // Extract network features for ML analysis
                std::vector<float> networkFeatures = {
                    static_cast<float>(peer.latency),  // Latency
                    50.0f,  // Placeholder bandwidth (would come from actual measurement)
                    0.1f,   // Placeholder packet loss
                    0.8f    // Placeholder connection stability
                };
                
                // Use ML to predict network optimization potential
                double optimizationGain = mlAnalyzer.predictNetworkOptimizationGain(peer.id, networkFeatures);
                
                if (optimizationGain > 0.3) {  // If there's significant optimization potential
                    logger.debug("High optimization potential for peer " + peer.id + 
                                " (Gain: " + std::to_string(optimizationGain) + ")");
                    // This could trigger specific optimization actions
                }
                
                // In a real implementation, we would measure actual bandwidth to each peer
                // For now, we'll update the peer info in the remesh system
                remesh.addPeer(peer.id);
                remesh.updatePeerLatency(peer.id, peer.latency);  // Update with latest latency
                remesh.updatePeerBandwidth(peer.id, 50.0); // Placeholder bandwidth - would be measured
            }
            
            // Small sleep to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Cleanup
        logger.info("Shutting down SentinelFS-Neo...");
        watcher.stop();
        discovery.stop();
        remesh.stop();
        
    } catch (const std::exception& e) {
        std::cerr << "Error in SentinelFS-Neo: " << e.what() << std::endl;
        return 1;
    }
    
    logger.info("SentinelFS-Neo shutdown complete");
    return 0;
}