#include <iostream>
#include <csignal>
#include <string>
#include "DaemonCore.h"
#include "IPCHandler.h"
#include "EventHandlers.h"
#include "../../core/include/SessionCode.h"

using namespace SentinelFS;

std::atomic<bool> running{true};

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    running = false;
}

int main(int argc, char* argv[]) {
    // Register signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // --- Parse Command Line Arguments ---
    DaemonConfig config;
    config.tcpPort = 8080;
    config.discoveryPort = 9999;
    config.watchDirectory = "./watched_folder";
    config.encryptionEnabled = false;
    config.uploadLimit = 0;   // 0 = unlimited
    config.downloadLimit = 0; // 0 = unlimited
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--port" && i + 1 < argc) {
            config.tcpPort = std::stoi(argv[++i]);
        } 
        else if (arg == "--discovery" && i + 1 < argc) {
            config.discoveryPort = std::stoi(argv[++i]);
        } 
        else if (arg == "--dir" && i + 1 < argc) {
            config.watchDirectory = argv[++i];
        } 
        else if (arg == "--session-code" && i + 1 < argc) {
            config.sessionCode = SessionCode::normalize(argv[++i]);
        } 
        else if (arg == "--generate-code") {
            std::string code = SessionCode::generate();
            std::cout << "\nGenerated Session Code: " << SessionCode::format(code) << std::endl;
            std::cout << "Use this code with: --session-code " << code << std::endl;
            return 0;
        } 
        else if (arg == "--encrypt") {
            config.encryptionEnabled = true;
        } 
        else if (arg == "--upload-limit" && i + 1 < argc) {
            config.uploadLimit = std::stoull(argv[++i]) * 1024; // Convert KB/s to B/s
        } 
        else if (arg == "--download-limit" && i + 1 < argc) {
            config.downloadLimit = std::stoull(argv[++i]) * 1024; // Convert KB/s to B/s
        }
        else if (arg == "--help") {
            std::cout << "SentinelFS Daemon - P2P File Synchronization" << std::endl;
            std::cout << "\nUsage: " << argv[0] << " [OPTIONS]" << std::endl;
            std::cout << "\nOptions:" << std::endl;
            std::cout << "  --port <PORT>              TCP port for data transfer (default: 8080)" << std::endl;
            std::cout << "  --discovery <PORT>         UDP port for peer discovery (default: 9999)" << std::endl;
            std::cout << "  --dir <PATH>               Directory to watch (default: ./watched_folder)" << std::endl;
            std::cout << "  --session-code <CODE>      6-character session code for peer authentication" << std::endl;
            std::cout << "  --generate-code            Generate a new session code and exit" << std::endl;
            std::cout << "  --encrypt                  Enable AES-256-CBC encryption (requires session code)" << std::endl;
            std::cout << "  --upload-limit <KB/s>      Limit upload bandwidth (0 = unlimited)" << std::endl;
            std::cout << "  --download-limit <KB/s>    Limit download bandwidth (0 = unlimited)" << std::endl;
            std::cout << "  --help                     Show this help message" << std::endl;
            return 0;
        }
    }

    // Validate configuration
    if (!config.sessionCode.empty() && !SessionCode::validate(config.sessionCode)) {
        std::cerr << "Error: Invalid session code format. Must be 6 alphanumeric characters." << std::endl;
        return 1;
    }
    
    if (config.encryptionEnabled && config.sessionCode.empty()) {
        std::cerr << "Error: Cannot enable encryption without a session code!" << std::endl;
        return 1;
    }

    // --- Initialize Daemon Core ---
    DaemonCore daemon(config);
    
    if (!daemon.initialize()) {
        std::cerr << "Failed to initialize daemon" << std::endl;
        return 1;
    }

    // --- Setup Event Handlers ---
    EventHandlers eventHandlers(
        daemon.getEventBus(),
        daemon.getNetworkPlugin(),
        daemon.getStoragePlugin(),
        daemon.getFilesystemPlugin(),
        config.watchDirectory
    );
    
    eventHandlers.setupHandlers();

    // --- Setup IPC Handler ---
    IPCHandler ipcHandler(
        "/tmp/sentinel_daemon.sock",
        daemon.getNetworkPlugin(),
        daemon.getStoragePlugin(),
        daemon.getFilesystemPlugin()
    );
    
    // Connect sync enable/disable to event handlers
    ipcHandler.setSyncEnabledCallback([&](bool enabled) {
        eventHandlers.setSyncEnabled(enabled);
    });
    
    if (!ipcHandler.start()) {
        std::cerr << "Warning: Failed to start IPC server. CLI commands will not work." << std::endl;
    }

    // --- RTT Measurement Thread ---
    std::thread rttThread([&]() {
        while (running && daemon.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(15));
            
            auto storage = daemon.getStoragePlugin();
            auto network = daemon.getNetworkPlugin();
            
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
                        network->disconnectPeer(peer.id);
                    }
                } else {
                    std::cout << "Peer " << peer.id << " not connected, attempting reconnect..." << std::endl;
                    network->connectToPeer(peer.ip, peer.port);
                }
            }
        }
    });

    // --- Status Display Thread ---
    std::thread statusThread([&]() {
        int loopCount = 0;
        
        while (running && daemon.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            auto storage = daemon.getStoragePlugin();
            auto network = daemon.getNetworkPlugin();
            
            // Broadcast presence every 5 seconds
            network->broadcastPresence(config.discoveryPort, config.tcpPort);
            
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
        }
    });

    // --- Run Daemon ---
    daemon.run();

    // --- Cleanup ---
    running = false;
    
    if (statusThread.joinable()) statusThread.join();
    if (rttThread.joinable()) rttThread.join();
    
    ipcHandler.stop();

    return 0;
}
