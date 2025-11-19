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

namespace SentinelFS {

    class NetworkPlugin : public INetworkAPI {
    public:
        bool initialize(EventBus* eventBus) override {
            std::cout << "NetworkPlugin initialized" << std::endl;
            eventBus_ = eventBus;
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

            if (listenThread_.joinable()) listenThread_.join();
            if (discoveryThread_.joinable()) discoveryThread_.join();
        }

        std::string getName() const override { return "NetworkPlugin"; }
        std::string getVersion() const override { return "1.0.0"; }

        bool connectToPeer(const std::string& address, int port) override {
            std::cout << "Connecting to peer " << address << ":" << port << std::endl;
            return true;
        }

        bool sendData(const std::string& peerId, const std::vector<uint8_t>& data) override {
            std::cout << "Sending " << data.size() << " bytes to peer " << peerId << std::endl;
            return true;
        }

        void startListening(int port) override {
            std::cout << "Starting to listen on port " << port << std::endl;
            listening_ = true;
            listenThread_ = std::thread([this]() {
                while (listening_) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
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
        std::atomic<bool> listening_{false};
        std::thread listenThread_;
        
        std::atomic<bool> discoveryRunning_{false};
        std::thread discoveryThread_;
        int discoverySocket_ = -1;

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