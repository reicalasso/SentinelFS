#include "../core/plugin_loader/plugin_loader.h"
#include "../core/peer_registry/peer_registry.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>

// External functions from plugins
extern "C" {
    sfs::PeerRegistry* get_peer_registry();
    int connect_to_peer(const char*, uint16_t);
    int send_data(int, const char*, size_t);
}

int main() {
    std::cout << "========================================\n";
    std::cout << "   SentinelFS Sprint 5 - Network Layer\n";
    std::cout << "========================================\n\n";

    sfs::core::PluginLoader loader;
    
    // Load network plugins
    std::cout << "[Test] Loading network plugins...\n";
    
    if (loader.load_plugin("lib/discovery_udp.so") != 0) {
        std::cerr << "[Test] Failed to load UDP discovery plugin\n";
        return 1;
    }
    
    if (loader.load_plugin("lib/transfer_tcp.so") != 0) {
        std::cerr << "[Test] Failed to load TCP transfer plugin\n";
        return 1;
    }
    
    std::cout << "\n[Test] Loaded plugins:\n";
    for (const auto& name : loader.get_loaded_plugins()) {
        auto* info = loader.get_plugin_info(name);
        if (info) {
            std::cout << "  - " << info->name << " v" << info->version << "\n";
        }
    }
    
    std::cout << "\n[Test] Discovery and TCP server running...\n";
    std::cout << "[Test] Waiting 10 seconds for peer discovery...\n\n";
    
    // Wait and check for discovered peers
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "  " << (10 - i) << " seconds remaining...\n";
    }
    
    // Get peer registry from discovery plugin
    std::cout << "\n[Test] Checking discovered peers...\n";
    
    // Call the get_peer_registry function directly
    sfs::PeerRegistry* registry = get_peer_registry();
    if (registry) {
        std::cout << "[Test] Total peers discovered: " << registry->peer_count() << "\n";
        
        auto peers = registry->get_all_peers();
        for (const auto& peer : peers) {
            std::cout << "  - Peer: " << peer.peer_id 
                     << " @ " << peer.address << ":" << peer.port;
            if (peer.is_connected) {
                std::cout << " [CONNECTED]";
            }
            std::cout << "\n";
        }
        
        // Test TCP connection to first peer if available
        if (!peers.empty()) {
            std::cout << "\n[Test] Testing TCP connection to first peer...\n";
            
            const auto& peer = peers[0];
            
            int sockfd = connect_to_peer(peer.address.c_str(), peer.port);
            if (sockfd >= 0) {
                std::cout << "[Test] Connected to peer! Socket: " << sockfd << "\n";
                
                // Send a ping
                const char* ping = "PING";
                if (send_data(sockfd, ping, strlen(ping)) > 0) {
                    std::cout << "[Test] Sent PING to peer\n";
                    
                    // Read response
                    char buffer[256];
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
                    if (n > 0) {
                        buffer[n] = '\0';
                        std::cout << "[Test] Received response: " << buffer << "\n";
                    }
                }
                
                close(sockfd);
            } else {
                std::cout << "[Test] Failed to connect to peer\n";
            }
        }
    } else {
        std::cout << "[Test] Could not access peer registry\n";
    }
    
    std::cout << "\n[Test] Unloading plugins...\n";
    loader.unload_all();
    
    std::cout << "\n========================================\n";
    std::cout << "   Sprint 5 Test Complete!\n";
    std::cout << "========================================\n";
    
    return 0;
}
