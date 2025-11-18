#include "../../core/plugin_api.h"
#include "../../core/peer_registry/peer_registry.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <ctime>

static std::atomic<bool> g_running{false};
static std::thread g_discovery_thread;
static std::thread g_beacon_thread;
static sfs::PeerRegistry* g_peer_registry = nullptr;
static std::string g_self_peer_id;
static uint16_t g_discovery_port = 47777;

// Generate a unique peer ID
static std::string generate_peer_id() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    
    std::srand(std::time(nullptr));
    int random_suffix = std::rand() % 10000;
    
    return std::string(hostname) + "-" + std::to_string(random_suffix);
}

// Discovery listener thread
static void discovery_listener() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "[UDP Discovery] Failed to create socket\n";
        return;
    }
    
    // Allow address reuse
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(g_discovery_port);
    
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[UDP Discovery] Failed to bind to port " << g_discovery_port << "\n";
        close(sockfd);
        return;
    }
    
    std::cout << "[UDP Discovery] Listening on port " << g_discovery_port << "\n";
    
    char buffer[512];
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_len = sizeof(peer_addr);
    
    while (g_running) {
        std::memset(buffer, 0, sizeof(buffer));
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                            (struct sockaddr*)&peer_addr, &peer_addr_len);
        
        if (n > 0) {
            buffer[n] = '\0';
            std::string message(buffer);
            
            // Message format: "SENTINEL_PEER:<peer_id>:<port>"
            if (message.find("SENTINEL_PEER:") == 0) {
                size_t first_colon = message.find(':', 14);
                size_t second_colon = message.find(':', first_colon + 1);
                
                if (first_colon != std::string::npos && second_colon != std::string::npos) {
                    std::string peer_id = message.substr(14, first_colon - 14);
                    std::string port_str = message.substr(second_colon + 1);
                    uint16_t peer_port = static_cast<uint16_t>(std::stoi(port_str));
                    
                    // Don't add ourselves
                    if (peer_id != g_self_peer_id) {
                        std::string peer_ip = inet_ntoa(peer_addr.sin_addr);
                        
                        if (g_peer_registry && !g_peer_registry->has_peer(peer_id)) {
                            sfs::PeerInfo peer(peer_id, peer_ip, peer_port);
                            g_peer_registry->add_peer(peer);
                            std::cout << "[UDP Discovery] Discovered peer: " << peer_id 
                                     << " at " << peer_ip << ":" << peer_port << "\n";
                        } else if (g_peer_registry) {
                            g_peer_registry->update_last_seen(peer_id);
                        }
                    }
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    close(sockfd);
}

// Beacon broadcaster thread
static void beacon_broadcaster() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "[UDP Beacon] Failed to create socket\n";
        return;
    }
    
    // Enable broadcast
    int broadcast = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    
    struct sockaddr_in broadcast_addr;
    std::memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    broadcast_addr.sin_port = htons(g_discovery_port);
    
    std::string beacon_msg = "SENTINEL_PEER:" + g_self_peer_id + ":47778";
    
    while (g_running) {
        sendto(sockfd, beacon_msg.c_str(), beacon_msg.size(), 0,
               (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    close(sockfd);
}

class UdpDiscoveryPlugin {
public:
    UdpDiscoveryPlugin() {
        std::cout << "[UDP Discovery Plugin] Initializing...\n";
        g_self_peer_id = generate_peer_id();
        g_peer_registry = new sfs::PeerRegistry();
        std::cout << "[UDP Discovery Plugin] Peer ID: " << g_self_peer_id << "\n";
    }
    
    ~UdpDiscoveryPlugin() {
        stop();
        delete g_peer_registry;
        g_peer_registry = nullptr;
    }
    
    void start() {
        if (!g_running) {
            std::cout << "[UDP Discovery Plugin] Starting discovery...\n";
            g_running = true;
            g_discovery_thread = std::thread(discovery_listener);
            g_beacon_thread = std::thread(beacon_broadcaster);
        }
    }
    
    void stop() {
        if (g_running) {
            std::cout << "[UDP Discovery Plugin] Stopping...\n";
            g_running = false;
            
            if (g_discovery_thread.joinable()) {
                g_discovery_thread.join();
            }
            
            if (g_beacon_thread.joinable()) {
                g_beacon_thread.join();
            }
        }
    }
    
    sfs::PeerRegistry* get_registry() {
        return g_peer_registry;
    }
};

extern "C" {

SFS_PluginInfo plugin_info() {
    SFS_PluginInfo info;
    info.name = "discovery.udp";
    info.version = "1.0.0";
    info.author = "SentinelFS Team";
    info.description = "UDP-based LAN peer discovery";
    info.type = SFS_PLUGIN_TYPE_NETWORK;
    info.api_version = SFS_PLUGIN_API_VERSION;
    return info;
}

void* plugin_create() {
    auto* plugin = new UdpDiscoveryPlugin();
    plugin->start();
    return plugin;
}

void plugin_destroy(void* instance) {
    delete static_cast<UdpDiscoveryPlugin*>(instance);
}

// Custom API for getting peer registry
sfs::PeerRegistry* get_peer_registry() {
    return g_peer_registry;
}

} // extern "C"
