#include <iostream>
#include <csignal>
#include <string>
#include <filesystem>
#include <ctime>
#include <unordered_map>
#include <limits>
#include <cmath>
#include "DaemonCore.h"
#include "IPCHandler.h"
#include "EventHandlers.h"
#include "SessionCode.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include "AutoRemeshManager.h"
#include "Config.h"
#include "PathUtils.h"
#include <cstdlib>

using namespace SentinelFS;

int main(int argc, char* argv[]) {
    // Initialize logging
    auto& logger = Logger::instance();
    
    // Create logs directory if it doesn't exist
    std::filesystem::create_directories("./logs");
    
    logger.setLogFile("./logs/sentinel_daemon.log");
    logger.setLevel(LogLevel::DEBUG);
    logger.setMaxFileSize(100); // 100MB max log file size
    logger.setComponent("Daemon");
    
    logger.info("=== SentinelFS Daemon Starting ===", "Daemon");
    
    // --- Parse Command Line Arguments ---
    // Ensure production-friendly directories
    auto configDir = PathUtils::getConfigDir();
    auto dataDir = PathUtils::getDataDir();
    auto runtimeDir = PathUtils::getRuntimeDir();
    PathUtils::ensureDirectory(configDir);
    PathUtils::ensureDirectory(dataDir);
    PathUtils::ensureDirectory(runtimeDir);

    SentinelFS::Config fileConfig;
    auto configPath = configDir / "sentinel.conf";
    if (!fileConfig.loadFromFile(configPath.string())) {
        std::ofstream templateFile(configPath);
        if (templateFile.is_open()) {
            templateFile << "# SentinelFS configuration\n";
            templateFile << "tcp_port=8080\n";
            templateFile << "discovery_port=9999\n";
            templateFile << "watch_directory=~/sentinel_sync\n";
            templateFile << "encryption_enabled=false\n";
            templateFile << "upload_limit_kbps=0\n";
            templateFile << "download_limit_kbps=0\n";
            templateFile << "# session_code=ABC123\n";
        }
        fileConfig.loadFromFile(configPath.string());
    }

    DaemonConfig config;
    config.tcpPort = fileConfig.getInt("tcp_port", 8080);
    config.discoveryPort = fileConfig.getInt("discovery_port", 9999);
    config.watchDirectory = fileConfig.get("watch_directory", "./watched_folder");
    config.sessionCode = fileConfig.get("session_code", "");
    config.encryptionEnabled = fileConfig.getBool("encryption_enabled", false);

    auto uploadLimitKb = fileConfig.getSize("upload_limit_kbps", 0);
    auto downloadLimitKb = fileConfig.getSize("download_limit_kbps", 0);
    config.uploadLimit = uploadLimitKb > 0 ? uploadLimitKb * 1024 : 0;
    config.downloadLimit = downloadLimitKb > 0 ? downloadLimitKb * 1024 : 0;

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
    if (!config.sessionCode.empty() && !SessionCode::isValid(config.sessionCode)) {
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

    // Auto-remesh engine for peer selection
    AutoRemeshManager autoRemesh;

    // --- Setup Event Handlers ---
    EventHandlers eventHandlers(
        *daemon.getEventBus(),  // Dereference pointer to get reference
        daemon.getNetworkPlugin(),
        daemon.getStoragePlugin(),
        daemon.getFilesystemPlugin(),
        config.watchDirectory
    );
    
    eventHandlers.setupHandlers();

    // --- Setup IPC Handler ---
    auto socketPath = PathUtils::getSocketPath();
    auto dbPath = dataDir / "sentinel.db";
    setenv("SENTINEL_DB_PATH", dbPath.c_str(), 1);
    IPCHandler ipcHandler(
        socketPath.string(),
        daemon.getNetworkPlugin(),
        daemon.getStoragePlugin(),
        daemon.getFilesystemPlugin(),
        &daemon
    );
    
    // Connect sync enable/disable to event handlers
    ipcHandler.setSyncEnabledCallback([&](bool enabled) {
        eventHandlers.setSyncEnabled(enabled);
    });
    
    if (!ipcHandler.start()) {
        std::cerr << "Warning: Failed to start IPC server. CLI commands will not work." << std::endl;
    }

    // --- RTT Measurement + Auto-Remesh Thread ---
    std::thread rttThread([&]() {
        while (daemon.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(15));
            
            auto storage = daemon.getStoragePlugin();
            auto network = daemon.getNetworkPlugin();
            auto& metrics = MetricsCollector::instance();
            
            // Measure RTT to all peers and update health metrics
            auto peers = storage->getAllPeers();
            for (const auto& peer : peers) {
                if (network->isPeerConnected(peer.id)) {
                    int rtt = network->measureRTT(peer.id);
                    if (rtt >= 0) {
                        storage->updatePeerLatency(peer.id, rtt);
                        autoRemesh.updateMeasurement(peer.id, rtt, true);
                        metrics.recordSyncLatency(static_cast<uint64_t>(rtt));

                        PeerInfo updatedPeer = peer;
                        updatedPeer.lastSeen = static_cast<long long>(std::time(nullptr));
                        updatedPeer.status = "active";
                        updatedPeer.latency = rtt;
                        storage->addPeer(updatedPeer);
                        std::cout << "Updated latency for " << peer.id << ": " << rtt << "ms" << std::endl;
                    } else {
                        autoRemesh.updateMeasurement(peer.id, -1, false);
                        std::cout << "Failed to measure RTT for " << peer.id << std::endl;
                        PeerInfo updatedPeer = peer;
                        updatedPeer.status = "offline";
                        updatedPeer.latency = -1;
                        storage->addPeer(updatedPeer);
                        network->disconnectPeer(peer.id);
                    }
                } else {
                    // Peer currently disconnected; count as failed probe for health
                    autoRemesh.updateMeasurement(peer.id, -1, false);
                    PeerInfo updatedPeer = peer;
                    updatedPeer.status = "offline";
                    updatedPeer.latency = -1;
                    storage->addPeer(updatedPeer);
                    std::cout << "Peer " << peer.id << " not connected, attempting reconnect..." << std::endl;
                    network->connectToPeer(peer.ip, peer.port);
                }
            }

            // Compute auto-remesh decision based on current metrics
            std::vector<AutoRemeshManager::PeerInfoSnapshot> snapshots;
            snapshots.reserve(peers.size());
            for (const auto& peer : peers) {
                AutoRemeshManager::PeerInfoSnapshot s;
                s.peerId = peer.id;
                s.isConnected = network->isPeerConnected(peer.id);
                snapshots.push_back(std::move(s));
            }

            auto decision = autoRemesh.computeRemesh(snapshots);

            // Estimate RTT improvement from this remesh cycle using AutoRemeshManager metrics
            auto health = autoRemesh.snapshotMetrics();
            std::unordered_map<std::string, double> avgByPeer;
            avgByPeer.reserve(health.size());
            for (const auto& h : health) {
                if (h.avgRttMs >= 0.0 && std::isfinite(h.avgRttMs)) {
                    avgByPeer[h.peerId] = h.avgRttMs;
                }
            }

            std::unordered_map<std::string, bool> wasConnected;
            wasConnected.reserve(peers.size());
            for (const auto& peer : peers) {
                wasConnected[peer.id] = network->isPeerConnected(peer.id);
            }

            std::unordered_map<std::string, bool> finalConnected = wasConnected;
            for (const auto& id : decision.disconnectPeers) {
                finalConnected[id] = false;
            }
            for (const auto& id : decision.connectPeers) {
                finalConnected[id] = true;
            }

            auto computeAvgRtt = [&avgByPeer](const std::unordered_map<std::string, bool>& state) {
                double sum = 0.0;
                std::size_t count = 0;
                for (const auto& entry : state) {
                    if (!entry.second) {
                        continue;
                    }
                    auto it = avgByPeer.find(entry.first);
                    if (it == avgByPeer.end()) {
                        continue;
                    }
                    double v = it->second;
                    if (!std::isfinite(v) || v < 0.0) {
                        continue;
                    }
                    sum += v;
                    ++count;
                }
                if (count == 0) {
                    return std::numeric_limits<double>::quiet_NaN();
                }
                return sum / static_cast<double>(count);
            };

            double preAvg = computeAvgRtt(wasConnected);
            double postAvg = computeAvgRtt(finalConnected);
            if (std::isfinite(preAvg) && std::isfinite(postAvg) && preAvg > postAvg) {
                uint64_t improvement = static_cast<uint64_t>(preAvg - postAvg);
                metrics.recordRemeshRttImprovement(improvement);
            }

            std::size_t disconnectCount = 0;
            std::size_t connectCount = 0;

            // Apply disconnect decisions
            for (const auto& id : decision.disconnectPeers) {
                if (network->isPeerConnected(id)) {
                    network->disconnectPeer(id);
                    ++disconnectCount;
                    std::cout << "[AutoRemesh] Disconnected suboptimal peer: " << id << std::endl;
                }
            }

            // Helper to find PeerInfo by id
            auto findPeer = [&peers](const std::string& id) -> const PeerInfo* {
                for (const auto& p : peers) {
                    if (p.id == id) return &p;
                }
                return nullptr;
            };

            // Apply connect decisions
            for (const auto& id : decision.connectPeers) {
                const PeerInfo* p = findPeer(id);
                if (!p) {
                    continue;
                }
                if (!network->isPeerConnected(id)) {
                    if (network->connectToPeer(p->ip, p->port)) {
                        ++connectCount;
                        std::cout << "[AutoRemesh] Connected preferred peer: " << id
                                  << " (" << p->ip << ":" << p->port << ")" << std::endl;
                    }
                }
            }

            if (connectCount > 0 || disconnectCount > 0) {
                metrics.incrementRemeshCycles();
                std::cout << "[AutoRemesh] Remesh cycle: "
                          << "connected=" << connectCount
                          << ", disconnected=" << disconnectCount << std::endl;
            }
        }
    });

    // --- Status Display Thread ---
    std::thread statusThread([&]() {
        int loopCount = 0;
        
        while (daemon.isRunning()) {
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
    if (statusThread.joinable()) statusThread.join();
    if (rttThread.joinable()) rttThread.join();
    
    ipcHandler.stop();

    return 0;
}
