#include "../../core/plugin_api.h"
#include "../../core/peer_registry/peer_registry.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <map>
#include <mutex>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

static std::atomic<bool> g_running{false};
static std::thread g_server_thread;
static uint16_t g_listen_port = 47778;

struct TransferSession {
    int socket_fd;
    std::string peer_id;
    std::thread handler_thread;
};

static std::vector<TransferSession*> g_active_sessions;
static std::mutex g_sessions_mutex;

// Handle individual client connection
static void handle_client(int client_fd, const std::string& peer_addr) {
    std::cout << "[TCP Transfer] Connection from " << peer_addr << "\n";
    
    char buffer[4096];
    
    while (g_running) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        
        if (n > 0) {
            buffer[n] = '\0';
            std::string message(buffer);
            
            // Simple protocol: "PING" -> "PONG"
            if (message.find("PING") == 0) {
                const char* response = "PONG";
                send(client_fd, response, strlen(response), 0);
                std::cout << "[TCP Transfer] Responded to PING from " << peer_addr << "\n";
            }
            // File data transfer would go here in a real implementation
            else if (message.find("DATA:") == 0) {
                std::cout << "[TCP Transfer] Received data: " << message.substr(5, 50) << "...\n";
                const char* ack = "ACK";
                send(client_fd, ack, strlen(ack), 0);
            }
        } else if (n == 0) {
            // Connection closed
            break;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Error occurred
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    close(client_fd);
    std::cout << "[TCP Transfer] Connection closed from " << peer_addr << "\n";
}

// TCP server thread
static void tcp_server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "[TCP Transfer] Failed to create socket\n";
        return;
    }
    
    // Set socket options
    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // Set non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(g_listen_port);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[TCP Transfer] Failed to bind to port " << g_listen_port << "\n";
        close(server_fd);
        return;
    }
    
    if (listen(server_fd, 10) < 0) {
        std::cerr << "[TCP Transfer] Failed to listen\n";
        close(server_fd);
        return;
    }
    
    std::cout << "[TCP Transfer] Listening on port " << g_listen_port << "\n";
    
    while (g_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd >= 0) {
            std::string peer_addr = std::string(inet_ntoa(client_addr.sin_addr)) + 
                                   ":" + std::to_string(ntohs(client_addr.sin_port));
            
            // Spawn handler thread
            std::thread handler(handle_client, client_fd, peer_addr);
            handler.detach();
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    close(server_fd);
}

class TcpTransferPlugin {
public:
    TcpTransferPlugin() {
        std::cout << "[TCP Transfer Plugin] Initializing...\n";
    }
    
    ~TcpTransferPlugin() {
        stop();
    }
    
    void start() {
        if (!g_running) {
            std::cout << "[TCP Transfer Plugin] Starting TCP server...\n";
            g_running = true;
            g_server_thread = std::thread(tcp_server);
        }
    }
    
    void stop() {
        if (g_running) {
            std::cout << "[TCP Transfer Plugin] Stopping...\n";
            g_running = false;
            
            if (g_server_thread.joinable()) {
                g_server_thread.join();
            }
        }
    }
};

extern "C" {

SFS_PluginInfo plugin_info() {
    SFS_PluginInfo info;
    info.name = "transfer.tcp";
    info.version = "1.0.0";
    info.author = "SentinelFS Team";
    info.description = "TCP-based peer-to-peer file transfer";
    info.type = SFS_PLUGIN_TYPE_NETWORK;
    info.api_version = SFS_PLUGIN_API_VERSION;
    return info;
}

void* plugin_create() {
    auto* plugin = new TcpTransferPlugin();
    plugin->start();
    return plugin;
}

void plugin_destroy(void* instance) {
    delete static_cast<TcpTransferPlugin*>(instance);
}

// Helper function to connect to a peer
int connect_to_peer(const char* address, uint16_t port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }
    
    struct sockaddr_in peer_addr;
    std::memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address, &peer_addr.sin_addr) <= 0) {
        close(sockfd);
        return -1;
    }
    
    if (connect(sockfd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

// Helper function to send data
int send_data(int sockfd, const char* data, size_t len) {
    return send(sockfd, data, len, 0);
}

} // extern "C"
