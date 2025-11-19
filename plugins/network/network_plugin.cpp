#include "INetworkAPI.h"
#include "EventBus.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <mutex>

#include <random>
#include <sstream>

namespace SentinelFS {

    class NetworkPlugin : public INetworkAPI {
    public:
        bool initialize(EventBus* eventBus) override {
            std::cout << "NetworkPlugin initialized" << std::endl;
            eventBus_ = eventBus;
            
            // Generate random Peer ID
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(10000, 99999);
            localPeerId_ = "PEER_" + std::to_string(dis(gen));
            std::cout << "Local Peer ID: " << localPeerId_ << std::endl;
            
            return true;
        }

        void shutdown() override {
            std::cout << "NetworkPlugin shutdown" << std::endl;
            listening_ = false;
            discoveryRunning_ = false;
            
            if (discoverySocket_ >= 0) {
                ::shutdown(discoverySocket_, SHUT_RDWR);
                close(discoverySocket_);
                discoverySocket_ = -1;
            }

            if (tcpServer_ >= 0) {
                ::shutdown(tcpServer_, SHUT_RDWR);
                close(tcpServer_);
                tcpServer_ = -1;
            }

            {
                std::lock_guard<std::mutex> lock(connectionMutex_);
                for (auto& pair : connections_) {
                    close(pair.second);
                }
                connections_.clear();
            }

            if (listenThread_.joinable()) listenThread_.join();
            if (discoveryThread_.joinable()) discoveryThread_.join();
        }

        std::string getName() const override { return "NetworkPlugin"; }
        std::string getVersion() const override { return "1.0.0"; }

        bool connectToPeer(const std::string& address, int port) override {
            std::cout << "Connecting to peer " << address << ":" << port << std::endl;
            
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) return false;

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            if (inet_pton(AF_INET, address.c_str(), &addr.sin_addr) <= 0) {
                close(sock);
                return false;
            }

            if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                std::cerr << "Failed to connect to " << address << std::endl;
                close(sock);
                return false;
            }

            // Handshake: Send HELLO
            std::string hello = "SENTINEL_HELLO|1.0|" + localPeerId_;
            if (send(sock, hello.c_str(), hello.length(), 0) < 0) {
                std::cerr << "Failed to send handshake" << std::endl;
                close(sock);
                return false;
            }

            // Handshake: Wait for WELCOME
            char buffer[1024];
            ssize_t len = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (len <= 0) {
                std::cerr << "Handshake failed: No response" << std::endl;
                close(sock);
                return false;
            }
            buffer[len] = '\0';
            std::string response(buffer);
            
            if (response.find("SENTINEL_WELCOME|") != 0) {
                std::cerr << "Handshake failed: Invalid response: " << response << std::endl;
                close(sock);
                return false;
            }

            std::string remotePeerId = response.substr(17); // Length of "SENTINEL_WELCOME|"
            std::cout << "Handshake successful with " << remotePeerId << std::endl;

            {
                std::lock_guard<std::mutex> lock(connectionMutex_);
                connections_[remotePeerId] = sock;
            }
            
            if (eventBus_) {
                eventBus_->publish("PEER_CONNECTED", remotePeerId);
            }
            
            return true;
        }

        bool sendData(const std::string& peerId, const std::vector<uint8_t>& data) override {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            if (connections_.find(peerId) == connections_.end()) {
                std::cerr << "Peer " << peerId << " not found" << std::endl;
                return false;
            }
            
            int sock = connections_[peerId];
            ssize_t sent = send(sock, data.data(), data.size(), 0);
            if (sent < 0) {
                std::cerr << "Failed to send data to " << peerId << std::endl;
                return false;
            }
            
            std::cout << "Sent " << sent << " bytes to peer " << peerId << std::endl;
            return true;
        }

        void startListening(int port) override {
            std::cout << "Starting TCP listener on port " << port << std::endl;
            listening_ = true;
            
            tcpServer_ = socket(AF_INET, SOCK_STREAM, 0);
            if (tcpServer_ < 0) {
                std::cerr << "Failed to create TCP socket" << std::endl;
                return;
            }
            
            int opt = 1;
            setsockopt(tcpServer_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(port);

            if (bind(tcpServer_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                std::cerr << "Failed to bind TCP socket" << std::endl;
                close(tcpServer_);
                return;
            }

            if (listen(tcpServer_, 5) < 0) {
                std::cerr << "Failed to listen on TCP socket" << std::endl;
                close(tcpServer_);
                return;
            }

            listenThread_ = std::thread([this]() {
                while (listening_) {
                    struct sockaddr_in clientAddr;
                    socklen_t len = sizeof(clientAddr);
                    int clientSock = accept(tcpServer_, (struct sockaddr*)&clientAddr, &len);
                    
                    if (clientSock >= 0) {
                        std::string clientIp = inet_ntoa(clientAddr.sin_addr);
                        std::cout << "New connection from " << clientIp << std::endl;
                        
                        // For now, we just accept and maybe read in a detached thread
                        // In a real scenario, we would handle the handshake here
                        std::thread(&NetworkPlugin::handleClient, this, clientSock, clientIp).detach();
                    }
                }
            });
        }

        void startDiscovery(int port) override {
            std::cout << "Starting UDP discovery on port " << port << std::endl;
            discoveryRunning_ = true;
            
            discoverySocket_ = socket(AF_INET, SOCK_DGRAM, 0);
            if (discoverySocket_ < 0) {
                std::cerr << "Failed to create discovery socket" << std::endl;
                return;
            }

            int broadcast = 1;
            if (setsockopt(discoverySocket_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
                std::cerr << "Failed to set broadcast option" << std::endl;
                close(discoverySocket_);
                return;
            }

            int reuse = 1;
            if (setsockopt(discoverySocket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
                std::cerr << "Failed to set reuse addr option" << std::endl;
            }

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(discoverySocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
                std::cerr << "Failed to bind discovery socket" << std::endl;
                close(discoverySocket_);
                return;
            }

            discoveryThread_ = std::thread(&NetworkPlugin::discoveryLoop, this);
        }

        void broadcastPresence(int port) override {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) return;

            int broadcast = 1;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = INADDR_BROADCAST;

            std::string msg = "SENTINEL_DISCOVERY|MY_ID_123|" + std::to_string(port);
            sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
            
            close(sock);
            std::cout << "Broadcast sent: " << msg << std::endl;
        }

    private:
        EventBus* eventBus_ = nullptr;
        std::string localPeerId_;
        
        // TCP
        std::atomic<bool> listening_{false};
        std::thread listenThread_;
        int tcpServer_ = -1;
        std::map<std::string, int> connections_;
        std::mutex connectionMutex_;

        // UDP
        std::atomic<bool> discoveryRunning_{false};
        std::thread discoveryThread_;
        int discoverySocket_ = -1;

        void handleClient(int sock, std::string ip) {
            char buffer[4096];
            
            // Handshake: Expect HELLO
            ssize_t len = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (len <= 0) {
                close(sock);
                return;
            }
            buffer[len] = '\0';
            std::string msg(buffer);

            if (msg.find("SENTINEL_HELLO|") != 0) {
                std::cerr << "Invalid handshake from " << ip << std::endl;
                close(sock);
                return;
            }

            // Parse ID (SENTINEL_HELLO|VER|ID)
            size_t firstPipe = msg.find('|');
            size_t secondPipe = msg.find('|', firstPipe + 1);
            if (secondPipe == std::string::npos) {
                close(sock);
                return;
            }
            std::string remotePeerId = msg.substr(secondPipe + 1);

            // Send WELCOME
            std::string welcome = "SENTINEL_WELCOME|" + localPeerId_;
            send(sock, welcome.c_str(), welcome.length(), 0);

            std::cout << "Accepted connection from " << remotePeerId << " (" << ip << ")" << std::endl;

            {
                std::lock_guard<std::mutex> lock(connectionMutex_);
                connections_[remotePeerId] = sock;
            }

            if (eventBus_) {
                eventBus_->publish("PEER_CONNECTED", remotePeerId);
            }

            while (true) {
                len = recv(sock, buffer, sizeof(buffer), 0);
                if (len <= 0) {
                    break;
                }
                // Process received data
                std::vector<uint8_t> data(buffer, buffer + len);
                std::string dataStr(buffer, len);
                std::cout << "Received " << len << " bytes from " << remotePeerId << std::endl;
                
                if (eventBus_) {
                    eventBus_->publish("DATA_RECEIVED", dataStr);
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(connectionMutex_);
                connections_.erase(remotePeerId);
            }
            close(sock);
            std::cout << "Connection closed from " << remotePeerId << std::endl;
        }

        void discoveryLoop() {
            char buffer[1024];
            struct sockaddr_in senderAddr;
            socklen_t senderLen = sizeof(senderAddr);

            while (discoveryRunning_) {
                int len = recvfrom(discoverySocket_, buffer, sizeof(buffer) - 1, 0, 
                                 (struct sockaddr*)&senderAddr, &senderLen);
                
                if (len > 0) {
                    buffer[len] = '\0';
                    std::string msg(buffer);
                    std::cout << "Received broadcast: " << msg << " from " << inet_ntoa(senderAddr.sin_addr) << std::endl;
                    
                    if (eventBus_) {
                        eventBus_->publish("PEER_DISCOVERED", msg);
                    }
                }
            }
        }
    };


    extern "C" {
        IPlugin* create_plugin() {
            return new NetworkPlugin();
        }

        void destroy_plugin(IPlugin* plugin) {
            delete plugin;
        }
    }

}