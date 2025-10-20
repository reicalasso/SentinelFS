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
#include "ml/online_learning.hpp"
#include "ml/federated_learning.hpp"
#include "ml/advanced_forecasting.hpp"
#include "ml/neural_network.hpp"
#include "gui/main_window.hpp"

std::atomic<bool> running(true);

void signalHandler(int signum) {
    std::cout << "\nReceived signal " << signum << ". Shutting down gracefully..." << std::endl;
    running = false;
}

// Run in CLI mode (headless)
int runCLIMode(int argc, char* argv[]);

// Run in GUI mode
int runGUIMode(int argc, char* argv[]);

int main(int argc, char* argv[]) {
    // Check for --gui flag
    bool guiMode = false;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--gui") {
            guiMode = true;
            break;
        }
    }
    
    if (guiMode) {
        std::cout << "Starting in GUI mode..." << std::endl;
        return runGUIMode(argc, argv);
    } else {
        std::cout << "Starting in CLI mode (use --gui for graphical interface)..." << std::endl;
        return runCLIMode(argc, argv);
    }
}

int runGUIMode(int argc, char* argv[]) {
    // Create GUI window
    MainWindow window(argc, argv);
    
    // Initialize backend in separate thread
    std::thread backendThread([&window]() {
        // Initialize logger
        Logger logger;
        logger.setLevel(LogLevel::INFO);
        
        // Parse CLI args
        CLI cli;
        cli.parseArguments(0, nullptr);  // GUI mode doesn't need CLI args
        Config config = cli.getConfig();
        
        // Set defaults for GUI
        if (config.syncPath.empty()) {
            config.syncPath = std::string(getenv("HOME")) + "/SentinelFS";
            std::filesystem::create_directories(config.syncPath);
        }
        if (config.sessionCode.empty()) {
            config.sessionCode = "GUI_SESSION";
        }
        
        window.addLogMessage("Initializing SentinelFS-Neo backend...", "INFO");
        
        // Initialize all components (same as CLI mode)
        MetadataDB db(config.syncPath + "/metadata.db");
        db.initialize();
        
        SecurityManager securityManager;
        securityManager.initialize(config.sessionCode);
        
        Transfer transfer(config.port);
        transfer.enableSecurity(true);
        transfer.setSecurityManager(&securityManager);
        
        Discovery discovery(config.port);
        Remesh remesh;
        NATTraversal natTraversal;
        
        // Sync Manager
        SyncConfig syncConfig;
        syncConfig.enableSelectiveSync = true;
        syncConfig.enableBandwidthThrottling = true;
        syncConfig.enableResumeTransfers = true;
        syncConfig.enableVersionHistory = true;
        syncConfig.maxBandwidthUpload = 1024 * 1024 * 10;
        syncConfig.maxBandwidthDownload = 1024 * 1024 * 20;
        SyncManager syncManager(syncConfig);
        syncManager.start();
        
        // ML modules
        OnlineLearningConfig onlineConfig;
        OnlineLearner onlineLearner(onlineConfig);
        
        FederatedConfig fedConfig;
        FederatedLearning fedLearner(fedConfig);
        
        ForecastingConfig forecastConfig;
        AdvancedForecastingManager forecaster(forecastConfig);
        forecaster.initialize();
        
        NeuralNetwork neuralNet;
        neuralNet.addLayer(10, 20, "relu");
        neuralNet.addLayer(20, 10, "relu");
        neuralNet.addLayer(10, 1, "sigmoid");
        
        window.addLogMessage("✅ All backend components initialized!", "INFO");
        window.setStatus("Backend ready");
        
        // Update GUI periodically
        while (running) {
            // Get current statistics
            GUIStatistics stats;
            auto dbStats = db.getStatistics();
            stats.totalFiles = dbStats.totalFiles;
            stats.activePeers = dbStats.activePeers;
            stats.totalPeers = dbStats.totalPeers;
            stats.anomaliesDetected = dbStats.anomaliesLogged;
            
            auto syncStats = syncManager.getSyncStats();
            stats.syncedFiles = syncStats.filesSynced;
            stats.bytesTransferred = syncStats.bytesTransferred;
            stats.uploadRate = syncManager.getCurrentUploadRate();
            stats.downloadRate = syncManager.getCurrentDownloadRate();
            
            // Update GUI
            window.updateStatistics(stats);
            
            // Get files and peers
            auto files = db.getAllFiles();
            window.updateFileList(files);
            
            auto peers = db.getAllPeers();
            std::vector<PeerInfo> peerList;
            for (const auto& peer : peers) {
                peerList.push_back(peer);
            }
            window.updatePeerList(peerList);
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    
    // Set GUI callbacks
    window.setSyncButtonCallback([&window]() {
        window.addLogMessage("Manual sync triggered", "INFO");
        window.setStatus("Syncing...");
    });
    
    // Show window and run
    window.show();
    window.run();
    
    running = false;
    if (backendThread.joinable()) {
        backendThread.join();
    }
    
    return 0;
}

int runCLIMode(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // ... (original CLI code continues here)
        Logger logger;
        logger.setLevel(LogLevel::INFO);
        
        CLI cli;
        cli.parseArguments(argc, argv);
        Config config = cli.getConfig();
        
        logger.info("Starting SentinelFS-Neo with session code: " + config.sessionCode);
        
        // [Rest of the original main.cpp code...]
        // (Keep all the existing initialization code)
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
