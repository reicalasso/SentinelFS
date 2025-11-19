#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <csignal>
#include <atomic>
#include <filesystem>

#include "PluginLoader.h"
#include "EventBus.h"
#include "IStorageAPI.h"
#include "INetworkAPI.h"
#include "IFileAPI.h"
#include "DeltaEngine.h"

using namespace SentinelFS;

std::atomic<bool> running{true};

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    running = false;
}

int main(int argc, char* argv[]) {
    // Register signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "SentinelFS Daemon Starting..." << std::endl;

    EventBus eventBus;
    PluginLoader loader;

    // Load Plugins
    // Note: Paths might need adjustment based on installation
    std::string storagePath = "./plugins/storage/libstorage_plugin.so";
    std::string networkPath = "./plugins/network/libnetwork_plugin.so";
    std::string fsPath = "./plugins/filesystem/libfilesystem_plugin.so";

    auto storagePlugin = loader.loadPlugin(storagePath, &eventBus);
    auto networkPlugin = loader.loadPlugin(networkPath, &eventBus);
    auto fsPlugin = loader.loadPlugin(fsPath, &eventBus);

    if (!storagePlugin || !networkPlugin || !fsPlugin) {
        std::cerr << "Failed to load one or more plugins. Exiting." << std::endl;
        return 1;
    }

    // Cast to Interfaces
    auto storage = std::dynamic_pointer_cast<IStorageAPI>(storagePlugin);
    auto network = std::dynamic_pointer_cast<INetworkAPI>(networkPlugin);
    auto fs = std::dynamic_pointer_cast<IFileAPI>(fsPlugin);

    if (!storage || !network || !fs) {
        std::cerr << "Failed to cast plugins to interfaces. Exiting." << std::endl;
        return 1;
    }

    // Initialize Plugins
    if (!storage->initialize(&eventBus) || !network->initialize(&eventBus) || !fs->initialize(&eventBus)) {
        std::cerr << "Failed to initialize plugins. Exiting." << std::endl;
        return 1;
    }

    // --- Event Handlers ---

    // 1. Peer Discovery -> Storage & Connect
    eventBus.subscribe("PEER_DISCOVERED", [&](const std::any& data) {
        try {
            std::string msg = std::any_cast<std::string>(data);
            // Format: SENTINEL_DISCOVERY|ID|PORT
            // Simple parsing
            size_t firstPipe = msg.find('|');
            size_t secondPipe = msg.find('|', firstPipe + 1);
            
            if (secondPipe != std::string::npos) {
                std::string id = msg.substr(firstPipe + 1, secondPipe - firstPipe - 1);
                std::string portStr = msg.substr(secondPipe + 1);
                int port = std::stoi(portStr);
                
                // TODO: Get IP from message or context (NetworkPlugin needs to provide this)
                // For now assuming localhost for demo or we need to extract from msg if we add IP there
                std::string ip = "127.0.0.1"; 

                PeerInfo peer;
                peer.id = id;
                peer.ip = ip;
                peer.port = port;
                peer.lastSeen = std::time(nullptr);
                peer.status = "active";

                storage->addPeer(peer);
                
                // Auto-connect
                network->connectToPeer(ip, port);
            }
        } catch (...) {
            std::cerr << "Error handling PEER_DISCOVERED" << std::endl;
        }
    });

    // 2. File Modified -> Calculate Delta -> Send
    eventBus.subscribe("FILE_MODIFIED", [&](const std::any& data) {
        try {
            std::string path = std::any_cast<std::string>(data);
            std::cout << "Daemon: File modified: " << path << std::endl;

            // 1. Get previous metadata
            auto meta = storage->getFile(path);
            if (!meta) {
                // New file, just add metadata and maybe send full file (not implemented yet)
                // For now, just update storage
                // In real sync, we would generate signature
                return;
            }

            // 2. Calculate Delta (Simplified for demo)
            // In a real scenario, we need the 'old' version or its signature.
            // Here we assume we are sending to a peer who has an older version.
            // This logic is complex; for now, let's just broadcast "I have an update"
            
            auto peers = storage->getAllPeers();
            for (const auto& peer : peers) {
                std::string updateMsg = "UPDATE_AVAILABLE|" + path;
                std::vector<uint8_t> payload(updateMsg.begin(), updateMsg.end());
                network->sendData(peer.id, payload);
            }

        } catch (...) {
            std::cerr << "Error handling FILE_MODIFIED" << std::endl;
        }
    });

    // 3. Data Received -> Process
    eventBus.subscribe("DATA_RECEIVED", [&](const std::any& data) {
        try {
            std::string msg = std::any_cast<std::string>(data);
            std::cout << "Daemon: Received data: " << msg << std::endl;
            
            if (msg.find("UPDATE_AVAILABLE|") == 0) {
                std::string path = msg.substr(17);
                std::cout << "Peer has update for: " << path << std::endl;
                // Trigger request for delta
            }
        } catch (...) {
            std::cerr << "Error handling DATA_RECEIVED" << std::endl;
        }
    });


    // --- Start Services ---
    int discoveryPort = 9999;
    int tcpPort = 8080; // Should be configurable

    network->startListening(tcpPort);
    network->startDiscovery(discoveryPort);
    
    // Start watching a test directory
    std::string watchDir = "./watched_folder";
    std::filesystem::create_directories(watchDir);
    fs->startWatching(watchDir);

    std::cout << "Daemon running. Press Ctrl+C to stop." << std::endl;

    // Main Loop
    while (running) {
        // Periodic tasks (e.g., broadcast presence)
        network->broadcastPresence(tcpPort); // Broadcast TCP port
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // Shutdown
    fs->shutdown();
    network->shutdown();
    storage->shutdown();

    return 0;
}
