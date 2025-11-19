#include "INetworkAPI.h"
#include "EventBus.h"
#include "Crypto.h"
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
        ~NetworkPlugin() {
            shutdown();
        }

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

            // Handshake: Send HELLO with session code
            std::string hello = "SENTINEL_HELLO|1.0|" + localPeerId_ + "|" + sessionCode_;
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
            
            if (response.find("ERROR|INVALID_SESSION_CODE") == 0) {
                std::cerr << "🚫 Connection rejected: Invalid session code!" << std::endl;
                std::cerr << "   Make sure all peers use the same session code." << std::endl;
                close(sock);
                return false;
            }
            
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
            
            // Start reading from this peer
            std::thread(&NetworkPlugin::readLoop, this, sock, remotePeerId).detach();
            
            return true;
        }

        bool sendData(const std::string& peerId, const std::vector<uint8_t>& data) override {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            if (connections_.find(peerId) == connections_.end()) {
                std::cerr << "Peer " << peerId << " not found" << std::endl;
                return false;
            }
            
            int sock = connections_[peerId];
            
            std::vector<uint8_t> dataToSend = data;
            
            // Encrypt if enabled
            if (encryptionEnabled_ && !encryptionKey_.empty()) {
                try {
                    auto iv = sentinel::Crypto::generateIV();
                    auto ciphertext = sentinel::Crypto::encrypt(data, encryptionKey_, iv);
                    auto hmac = sentinel::Crypto::hmacSHA256(ciphertext, encryptionKey_);
                    
                    sentinel::EncryptedMessage msg;
                    msg.iv = iv;
                    msg.ciphertext = ciphertext;
                    msg.hmac = hmac;
                    
                    dataToSend = msg.serialize();
                    std::cout << "Data encrypted (" << data.size() << " -> " << dataToSend.size() << " bytes)" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Encryption failed: " << e.what() << std::endl;
                    return false;
                }
            }
            
            // Send Length Prefix
            uint32_t len = htonl(dataToSend.size());
            if (send(sock, &len, sizeof(len), 0) < 0) {
                std::cerr << "Failed to send length prefix to " << peerId << std::endl;
                return false;
            }

            // Send Data
            ssize_t sent = send(sock, dataToSend.data(), dataToSend.size(), 0);
            if (sent < 0) {
                std::cerr << "Failed to send data to " << peerId << std::endl;
                return false;
            }
            
            std::cout << "Sent " << sent << " bytes to peer " << peerId 
                      << (encryptionEnabled_ ? " (encrypted)" : "") << std::endl;
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

        void broadcastPresence(int discoveryPort, int tcpPort) override {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock < 0) return;

            int broadcast = 1;
            setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));

            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(discoveryPort);
            addr.sin_addr.s_addr = INADDR_BROADCAST;

            std::cout << "DEBUG: BROADCASTING NOW!!!" << std::endl;
            std::string msg = "SENTINEL_DISCOVERY|" + localPeerId_ + "|" + std::to_string(tcpPort);
            sendto(sock, msg.c_str(), msg.length(), 0, (struct sockaddr*)&addr, sizeof(addr));
            
            close(sock);
            std::cout << "Broadcast sent: " << msg << " to port " << discoveryPort << std::endl;
        }

        int measureRTT(const std::string& peerId) override {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            auto it = connections_.find(peerId);
            if (it == connections_.end()) {
                return -1; // Not connected
            }

            int sock = it->second;
            
            // Send PING message
            std::string ping = "PING";
            auto start = std::chrono::high_resolution_clock::now();
            
            if (send(sock, ping.c_str(), ping.length(), 0) < 0) {
                return -1;
            }

            // Wait for PONG response (with timeout)
            char buffer[64];
            struct timeval tv;
            tv.tv_sec = 2;  // 2 second timeout
            tv.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            
            ssize_t len = recv(sock, buffer, sizeof(buffer) - 1, MSG_PEEK);
            if (len <= 0) {
                return -1;
            }
            
            auto end = std::chrono::high_resolution_clock::now();
            int rtt = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            // Store RTT
            {
                std::lock_guard<std::mutex> rttLock(rttMutex_);
                peerRTT_[peerId] = rtt;
            }
            
            std::cout << "RTT to " << peerId << ": " << rtt << "ms" << std::endl;
            return rtt;
        }

        int getPeerRTT(const std::string& peerId) override {
            std::lock_guard<std::mutex> lock(rttMutex_);
            auto it = peerRTT_.find(peerId);
            if (it != peerRTT_.end()) {
                return it->second;
            }
            return -1;
        }

        void disconnectPeer(const std::string& peerId) override {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            auto it = connections_.find(peerId);
            if (it != connections_.end()) {
                close(it->second);
                connections_.erase(it);
                std::cout << "Disconnected from peer: " << peerId << std::endl;
                
                if (eventBus_) {
                    eventBus_->publish("PEER_DISCONNECTED", peerId);
                }
            }
            
            // Clear RTT data
            {
                std::lock_guard<std::mutex> rttLock(rttMutex_);
                peerRTT_.erase(peerId);
            }
        }

        bool isPeerConnected(const std::string& peerId) override {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            return connections_.find(peerId) != connections_.end();
        }

        void setSessionCode(const std::string& code) override {
            sessionCode_ = code;
            std::cout << "Session code set for peer group" << std::endl;
            
            // Derive encryption key from session code if encryption is enabled
            if (encryptionEnabled_ && !code.empty()) {
                try {
                    // Use a fixed salt derived from "SentinelFS" for deterministic key derivation
                    std::vector<uint8_t> salt = {0x53, 0x65, 0x6E, 0x74, 0x69, 0x6E, 0x65, 0x6C, 
                                                   0x46, 0x53, 0x5F, 0x32, 0x30, 0x32, 0x35};
                    encryptionKey_ = sentinel::Crypto::deriveKeyFromSessionCode(code, salt);
                    std::cout << "Encryption key derived from session code" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Failed to derive encryption key: " << e.what() << std::endl;
                }
            }
        }

        std::string getSessionCode() const override {
            return sessionCode_;
        }

        void setEncryptionEnabled(bool enable) override {
            encryptionEnabled_ = enable;
            std::cout << "Encryption " << (enable ? "enabled" : "disabled") << std::endl;
            
            // Derive key if enabling and session code exists
            if (enable && !sessionCode_.empty()) {
                try {
                    std::vector<uint8_t> salt = {0x53, 0x65, 0x6E, 0x74, 0x69, 0x6E, 0x65, 0x6C, 
                                                   0x46, 0x53, 0x5F, 0x32, 0x30, 0x32, 0x35};
                    encryptionKey_ = sentinel::Crypto::deriveKeyFromSessionCode(sessionCode_, salt);
                    std::cout << "Encryption key derived" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Failed to derive encryption key: " << e.what() << std::endl;
                    encryptionEnabled_ = false;
                }
            } else if (!enable) {
                encryptionKey_.clear();
            }
        }

        bool isEncryptionEnabled() const override {
            return encryptionEnabled_;
        }

    private:
        EventBus* eventBus_ = nullptr;
        std::string localPeerId_;
        std::string sessionCode_;  // Session code for this peer group
        bool encryptionEnabled_ = false;  // Encryption toggle
        std::vector<uint8_t> encryptionKey_;  // Derived from session code
        
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

        // RTT tracking
        std::map<std::string, int> peerRTT_;
        std::mutex rttMutex_;

        void readLoop(int sock, std::string remotePeerId) {
            while (true) {
                uint32_t netLen;
                ssize_t received = recv(sock, &netLen, sizeof(netLen), MSG_WAITALL);
                if (received <= 0) break;

                uint32_t len = ntohl(netLen);
                std::vector<uint8_t> receivedData(len);
                
                received = recv(sock, receivedData.data(), len, MSG_WAITALL);
                if (received <= 0) break;

                std::vector<uint8_t> data = receivedData;
                
                // Decrypt if encryption is enabled
                if (encryptionEnabled_ && !encryptionKey_.empty()) {
                    try {
                        auto msg = sentinel::EncryptedMessage::deserialize(receivedData);
                        
                        // Verify HMAC
                        auto expectedHmac = sentinel::Crypto::hmacSHA256(msg.ciphertext, encryptionKey_);
                        if (msg.hmac != expectedHmac) {
                            std::cerr << "HMAC verification failed from " << remotePeerId << std::endl;
                            break;
                        }
                        
                        // Decrypt
                        data = sentinel::Crypto::decrypt(msg.ciphertext, encryptionKey_, msg.iv);
                        std::cout << "Data decrypted (" << receivedData.size() << " -> " << data.size() << " bytes)" << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "Decryption failed from " << remotePeerId << ": " << e.what() << std::endl;
                        break;
                    }
                }

                if (eventBus_) {
                    eventBus_->publish("DATA_RECEIVED", std::make_pair(remotePeerId, data));
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(connectionMutex_);
                connections_.erase(remotePeerId);
            }
            close(sock);
            std::cout << "Connection closed from " << remotePeerId << std::endl;
        }

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

            // Parse ID and Session Code (SENTINEL_HELLO|VER|ID|CODE)
            size_t firstPipe = msg.find('|');
            size_t secondPipe = msg.find('|', firstPipe + 1);
            size_t thirdPipe = msg.find('|', secondPipe + 1);
            
            if (secondPipe == std::string::npos) {
                close(sock);
                return;
            }
            
            std::string remotePeerId;
            std::string remoteSessionCode;
            
            if (thirdPipe != std::string::npos) {
                // New format with session code
                remotePeerId = msg.substr(secondPipe + 1, thirdPipe - secondPipe - 1);
                remoteSessionCode = msg.substr(thirdPipe + 1);
            } else {
                // Old format without session code (backward compatibility)
                remotePeerId = msg.substr(secondPipe + 1);
            }
            
            // Validate session code if set
            if (!sessionCode_.empty()) {
                if (remoteSessionCode != sessionCode_) {
                    std::cerr << "⚠️  Session code mismatch! Expected: " << sessionCode_ 
                              << ", Got: " << remoteSessionCode << std::endl;
                    std::cerr << "🚫 Rejecting connection from " << ip << std::endl;
                    
                    std::string error = "ERROR|INVALID_SESSION_CODE";
                    send(sock, error.c_str(), error.length(), 0);
                    close(sock);
                    return;
                }
                std::cout << "✓ Session code validated for " << remotePeerId << std::endl;
            }

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

            readLoop(sock, remotePeerId);
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