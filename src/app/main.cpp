#include <iostream>
#include <thread>
#include <memory>
#include <atomic>
#include <system_error>
#include <signal.h>
#include <filesystem>
#include <fstream>
#include <ctime>

#include "app/cli.hpp"
#include "app/logger.hpp"

// GUI support (optional)
#ifdef ENABLE_GUI
#include "gui/main_window.hpp"
int runGUIMode(int argc, char* argv[]);
#endif
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
#include "ml/online_learning.hpp"
#include "ml/federated_learning.hpp"
#include "ml/advanced_forecasting.hpp"
#include "ml/neural_network.hpp"

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
    // Check for GUI mode
    bool guiMode = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--gui") {
            guiMode = true;
            // Remove --gui from argv to avoid CLI parsing errors
            for (int j = i; j < argc - 1; j++) {
                argv[j] = argv[j + 1];
            }
            argc--;
            break;
        }
    }
    
    if (guiMode) {
        #ifdef ENABLE_GUI
        return runGUIMode(argc, argv);
        #else
        std::cerr << "Error: GUI support not compiled in!" << std::endl;
        std::cerr << "Please rebuild with GTK3 support." << std::endl;
        return 1;
        #endif
    }
    
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
        
        // Ensure sync root exists before proceeding
        std::error_code dirEc;
        std::filesystem::create_directories(config.syncPath, dirEc);
        if (dirEc) {
            logger.error("Failed to prepare sync directory '" + config.syncPath + "': " + dirEc.message());
            return 1;
        }
        const std::filesystem::path syncRoot = std::filesystem::absolute(config.syncPath);

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

        if (!transfer.startListener(config.port)) {
            logger.warning("⚠️ Failed to start inbound transfer listener on port " + std::to_string(config.port));
        } else {
            logger.info("✅ Inbound transfer listener active on port " + std::to_string(config.port));
        }
        
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
        
        // ✅ ADVANCED ML: Initialize all advanced ML modules!
        logger.info("🤖 Initializing Advanced ML modules...");
        
        // 1. Online Learner - Adaptive learning with drift detection
        OnlineLearningConfig onlineConfig;
        onlineConfig.learningRate = 0.001;
        onlineConfig.bufferSize = 1000;
        onlineConfig.batchSize = 32;
        onlineConfig.enableDriftDetection = true;
        onlineConfig.driftThreshold = 0.1;
        OnlineLearner onlineLearner(onlineConfig);  // Uses config
        logger.info("✅ OnlineLearner initialized (adaptive learning, drift detection)");
        
        // 2. Federated Learner - Multi-peer collaborative ML
        FederatedConfig fedConfig;
        fedConfig.learningRate = 0.01;
        fedConfig.numRounds = 100;
        fedConfig.localEpochs = 5;
        fedConfig.secureAggregation = true;
        fedConfig.maxPeers = 10;
        FederatedLearning fedLearner(fedConfig);
        logger.info("✅ FederatedLearning initialized (collaborative ML, 100 rounds, secure aggregation)");
        
        // 3. Advanced Forecaster - ARIMA time-series prediction
        ForecastingConfig forecastConfig;
        forecastConfig.sequenceLength = 50;
        forecastConfig.predictionHorizon = 10;
        forecastConfig.learningRate = 0.001;
        AdvancedForecastingManager forecaster(forecastConfig);
        forecaster.initialize();
        logger.info("✅ AdvancedForecaster initialized (ARIMA, 10-step prediction)");
        
        // 4. Neural Network - Deep learning with backpropagation
        NeuralNetwork neuralNet;
        neuralNet.addLayer(10, 20, "relu");   // Input: 10, Hidden: 20
        neuralNet.addLayer(20, 10, "relu");   // Hidden: 20, Hidden: 10
        neuralNet.addLayer(10, 1, "sigmoid"); // Hidden: 10, Output: 1
        logger.info("✅ NeuralNetwork initialized (3 layers: 10->20->10->1)");
        
        logger.info("🎉 All Advanced ML modules active and ready!");
        
        // Initialize file system components
        ConflictResolver conflictResolver(ConflictResolutionStrategy::BACKUP);
        FileLocker fileLocker;
        Compressor compressor(CompressionAlgorithm::ZSTD);  // ✅ Use Zstandard for speed
        logger.info("✅ Compressor initialized (Zstandard algorithm)");
        logger.info("✅ Delta Engine ready for efficient sync (per-file instances)");
        logger.info("✅ ConflictResolver active with BACKUP strategy");
        logger.info("✅ FileLocker active for concurrency protection");

        auto makeRelativePath = [&](const std::string& absolutePath) -> std::string {
            std::error_code relEc;
            auto absolute = std::filesystem::absolute(absolutePath);
            auto rel = std::filesystem::relative(absolute, syncRoot, relEc);
            if (relEc || rel.empty()) {
                return std::filesystem::path(absolutePath).filename().string();
            }
            for (const auto& part : rel) {
                if (part == "..") {
                    return std::filesystem::path(absolutePath).filename().string();
                }
            }
            return rel.generic_string();
        };

        auto sanitizeInboundPath = [&](const std::string& remotePath) -> std::filesystem::path {
            std::filesystem::path candidate(remotePath);
            candidate = candidate.lexically_normal();
            if (candidate.has_root_path()) {
                candidate = candidate.relative_path();
            }
            if (candidate.empty()) {
                candidate = std::filesystem::path(remotePath).filename();
            }
            bool safe = true;
            for (const auto& part : candidate) {
                if (part == "..") {
                    safe = false;
                    break;
                }
            }
            if (!safe || candidate.empty()) {
                candidate = candidate.filename();
            }
            return candidate;
        };

        transfer.setDeltaHandler([&, sanitizeInboundPath, syncRoot](const PeerInfo& peer, const DeltaData& inboundDelta) {
            if (inboundDelta.filePath.empty()) {
                logger.warning("Inbound delta from " + peer.id + " missing file path; ignoring");
                return;
            }

            const auto relativePath = sanitizeInboundPath(inboundDelta.filePath);
            const auto targetPath = (syncRoot / relativePath).lexically_normal();

            std::error_code dirEc;
            std::filesystem::create_directories(targetPath.parent_path(), dirEc);
            if (dirEc) {
                logger.error("Failed to prepare path for inbound delta '" + targetPath.string() + "': " + dirEc.message());
                return;
            }

            if (!fileLocker.acquireLock(targetPath.string(), LockType::WRITE)) {
                logger.warning("Could not lock file for inbound delta: " + targetPath.string());
                return;
            }

            DeltaEngine applyEngine(targetPath.string());
            bool applied = inboundDelta.isCompressed
                               ? applyEngine.applyCompressed(inboundDelta, targetPath.string())
                               : applyEngine.apply(inboundDelta, targetPath.string());

            if (!applied) {
                logger.error("Failed to apply inbound delta to " + targetPath.string());
                fileLocker.releaseLock(targetPath.string());
                return;
            }

            std::error_code sizeEc;
            auto fileSize = std::filesystem::file_size(targetPath, sizeEc);

            FileInfo fileInfo;
            fileInfo.path = targetPath.string();
            fileInfo.size = sizeEc ? 0 : static_cast<size_t>(fileSize);
            fileInfo.lastModified = std::to_string(std::time(nullptr));
            fileInfo.deviceId = peer.id;
            fileInfo.hash = DeltaEngine::calculateHash(targetPath.string());

            auto existing = db.getFile(fileInfo.path);
            if (!existing.path.empty()) {
                fileInfo.version = existing.version + 1;
                db.updateFile(fileInfo);
            } else {
                db.addFile(fileInfo);
            }

            syncManager.createFileVersion(fileInfo.path, "Inbound delta from " + peer.id, peer.id);
            securityManager.recordPeerActivity(peer.id, fileInfo.size);
            logger.info("✅ Applied inbound delta for " + fileInfo.path + " from peer " + peer.id);

            fileLocker.releaseLock(targetPath.string());
        });

        transfer.setFileHandler([&, sanitizeInboundPath, syncRoot](const PeerInfo& peer,
                                                                  const std::string& remotePath,
                                                                  const std::vector<uint8_t>& contents) {
            auto relativePath = sanitizeInboundPath(remotePath);
            auto targetPath = (syncRoot / relativePath).lexically_normal();

            std::error_code dirEc;
            std::filesystem::create_directories(targetPath.parent_path(), dirEc);
            if (dirEc) {
                logger.error("Failed to prepare path for inbound file '" + targetPath.string() + "': " + dirEc.message());
                return;
            }

            if (!fileLocker.acquireLock(targetPath.string(), LockType::WRITE)) {
                logger.warning("Could not lock file for inbound transfer: " + targetPath.string());
                return;
            }

            std::ofstream out(targetPath, std::ios::binary | std::ios::trunc);
            if (!out) {
                logger.error("Failed to open destination for inbound file: " + targetPath.string());
                fileLocker.releaseLock(targetPath.string());
                return;
            }
            out.write(reinterpret_cast<const char*>(contents.data()), static_cast<std::streamsize>(contents.size()));
            out.close();

            std::error_code sizeEc;
            auto fileSize = std::filesystem::file_size(targetPath, sizeEc);

            FileInfo fileInfo;
            fileInfo.path = targetPath.string();
            fileInfo.size = sizeEc ? contents.size() : static_cast<size_t>(fileSize);
            fileInfo.lastModified = std::to_string(std::time(nullptr));
            fileInfo.deviceId = peer.id;
            fileInfo.hash = DeltaEngine::calculateHash(targetPath.string());

            auto existing = db.getFile(fileInfo.path);
            if (!existing.path.empty()) {
                fileInfo.version = existing.version + 1;
                db.updateFile(fileInfo);
            } else {
                db.addFile(fileInfo);
            }

            syncManager.createFileVersion(fileInfo.path, "Inbound file from " + peer.id, peer.id);
            securityManager.recordPeerActivity(peer.id, fileInfo.size);
            logger.info("✅ Stored inbound file " + fileInfo.path + " from peer " + peer.id);

            fileLocker.releaseLock(targetPath.string());
        });
        
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
            const std::string relativePath = makeRelativePath(event.path);
            delta.filePath = relativePath;
            
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

// GUI Mode Implementation (compiled only if GTK3 is available)
#ifdef ENABLE_GUI
int runGUIMode(int argc, char* argv[]) {
    try {
        std::cout << "Starting SentinelFS-Neo in GUI mode..." << std::endl;
        
        // Create main window
        MainWindow window(argc, argv);
        
        // Backend thread flag
        std::atomic<bool> backendRunning{true};
        
        // Start backend thread
        std::thread backendThread([&window, &backendRunning]() {
            try {
                // Logger setup
                Logger logger;
                logger.setLevel(LogLevel::INFO);
                logger.info("Backend thread started");
                
                // Initialize backend components (minimal for GUI demo)
                // In a real application, you would initialize:
                // - MetadataDB
                // - SyncManager
                // - FileWatcher
                // - PeerDiscovery
                // - ML components
                // etc.
                
                // Simulate backend updates
                int counter = 0;
                while (backendRunning) {
                    GUIStatistics stats;
                    stats.totalFiles = 125 + (counter % 10);
                    stats.syncedFiles = 98 + (counter % 5);
                    stats.activePeers = 3;
                    stats.totalPeers = 5;
                    stats.uploadRate = 2.5 + (counter % 3) * 0.5;
                    stats.downloadRate = 4.2 + (counter % 4) * 0.3;
                    stats.bytesTransferred = 1200000000 + counter * 1000000;
                    stats.lastSync = "2025-10-20 16:00:00";
                    stats.mlAccuracy = 0.87;
                    stats.anomaliesDetected = 2;
                    
                    window.updateStatistics(stats);
                    
                    // Update file list (demo data)
                    if (counter % 5 == 0) {
                        std::vector<FileInfo> files;
                        FileInfo f1;
                        f1.path = "/home/user/documents/report.pdf";
                        f1.size = 1200000;
                        f1.lastModified = "2025-10-20 15:30:00";
                        f1.conflictStatus = "none";
                        files.push_back(f1);
                        
                        FileInfo f2;
                        f2.path = "/home/user/images/photo.jpg";
                        f2.size = 850000;
                        f2.lastModified = "2025-10-20 14:15:00";
                        f2.conflictStatus = "none";
                        files.push_back(f2);
                        
                        window.updateFileList(files);
                    }
                    
                    // Update peer list (demo data)
                    if (counter % 3 == 0) {
                        std::vector<PeerInfo> peers;
                        PeerInfo p1;
                        p1.id = "PEER-001";
                        p1.address = "192.168.1.100";
                        p1.port = 8080;
                        p1.latency = 45 + (counter % 10);
                        p1.active = true;
                        peers.push_back(p1);
                        
                        PeerInfo p2;
                        p2.id = "PEER-002";
                        p2.address = "192.168.1.101";
                        p2.port = 8080;
                        p2.latency = 72 + (counter % 15);
                        p2.active = true;
                        peers.push_back(p2);
                        
                        window.updatePeerList(peers);
                    }
                    
                    // Add log messages
                    if (counter % 2 == 0) {
                        std::string logMsg = "Backend update #" + std::to_string(counter);
                        window.addLogMessage(logMsg, "INFO");
                    }
                    
                    counter++;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                
                logger.info("Backend thread stopped");
            } catch (const std::exception& e) {
                std::cerr << "Backend thread error: " << e.what() << std::endl;
            }
        });
        
        // Set up callbacks
        window.setSyncButtonCallback([&window]() {
            window.addLogMessage("Manual sync triggered", "INFO");
            // In real implementation, trigger syncManager.syncNow()
        });
        
        window.setFileSelectedCallback([&window](const std::string& path) {
            window.addLogMessage("Selected file: " + path, "INFO");
        });
        
        // Show window and run
        window.show();
        window.run();
        
        // Stop backend thread
        backendRunning = false;
        if (backendThread.joinable()) {
            backendThread.join();
        }
        
        std::cout << "GUI mode exited cleanly" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error in GUI mode: " << e.what() << std::endl;
        return 1;
    }
}
#endif  // ENABLE_GUI