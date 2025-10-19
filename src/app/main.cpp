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
#include "net/discovery.hpp"
#include "net/remesh.hpp"
#include "net/transfer.hpp"
#include "net/nat_traversal.hpp"
#include "db/db.hpp"
#include "db/cache.hpp"
#include "ml/ml_analyzer.hpp"

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
        logger.info("Basic ML analyzer initialized");
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
            
            // Check for anomalies
            auto features = MLAnalyzer::extractFeatures(event.path, event.peer_id, event.file_size);
            bool isAnomaly = mlAnalyzer.detectAnomaly(features);
            
            if (isAnomaly) {
                logger.warning("Anomalous file access detected: " + event.path);
                db.logAnomaly(event.path, features);
            }
            
            // Compute delta for the changed file with compression
            DeltaEngine deltaEngine(event.path);
            deltaEngine.setCompression(CompressionAlgorithm::GZIP); // Use gzip compression
            auto delta = deltaEngine.computeCompressed("", event.path); // For now, comparing with empty old file
            
            if (!delta.chunks.empty()) {
                logger.info("Computed delta for file: " + event.path + " (" + std::to_string(delta.chunks.size()) + " chunks), compressed: " + (delta.isCompressed ? "yes" : "no"));
                
                // Get current peer list
                auto peers = discovery.getPeers();
                
                // Send delta to all peers
                if (!peers.empty()) {
                    transfer.broadcastDelta(delta, peers);
                    logger.info("Broadcasted delta to " + std::to_string(peers.size()) + " peers");
                } else {
                    logger.warning("No peers found to sync with");
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
                // In a real implementation, we would measure actual bandwidth to each peer
                // For now, we'll update the peer info in the remesh system
                remesh.addPeer(peer.id);
                remesh.updatePeerLatency(peer.id, peer.latency);  // Update with latest latency
                // remesh.updatePeerBandwidth(peer.id, measured_bandwidth); // Would update with actual measurements
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