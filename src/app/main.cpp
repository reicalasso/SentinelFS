#include <iostream>
#include <thread>
#include <memory>
#include <atomic>
#include <signal.h>

#include "app/cli.hpp"
#include "app/logger.hpp"
#include "fs/watcher.hpp"
#include "fs/delta_engine.hpp"
#include "fs/file_queue.hpp"
#include "fs/conflict_resolver.hpp"
#include "fs/file_locker.hpp"
#include "security/security_manager.hpp"
#include "net/discovery.hpp"
#include "net/remesh.hpp"
#include "net/transfer.hpp"
#include "net/nat_traversal.hpp"
#include "db/db.hpp"
#include "db/cache.hpp"
#include "ml/ml_analyzer.hpp"

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
        
        // Initialize security components first
        auto& securityManager = SecurityManager::getInstance();
        securityManager.initialize();  // Initialize with default keys
        
        // Initialize network components
        Discovery discovery(config.sessionCode);
        Remesh remesh;
        Transfer transfer;
        NATTraversal natTraversal;  // For NAT traversal functionality
        
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
        
        // Initialize file system components
        ConflictResolver conflictResolver(ConflictResolutionStrategy::BACKUP);
        FileLocker fileLocker;
        
        // Set up file watcher
        FileWatcher watcher(config.syncPath, [&](const FileEvent& event) {
            logger.info("File event: " + event.type + " - " + event.path);
            
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
        
        // Main loop
        logger.info("SentinelFS-Neo is running with session code: " + config.sessionCode);
        
        while (!shouldExit) {
            // Periodic network maintenance
            remesh.evaluateAndOptimize();
            
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