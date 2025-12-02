#include "INetworkAPI.h"
#include "EventBus.h"
#include "Crypto.h"
#include "HandshakeProtocol.h"
#include "TCPHandler.h"
#include "UDPDiscovery.h"
#include "TCPRelay.h"
#include "BandwidthLimiter.h"
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <iomanip>

namespace SentinelFS {

/**
 * @brief Modular network plugin
 * 
 * Delegates to specialized components:
 * - HandshakeProtocol: Session code verification
 * - TCPHandler: Connection management
 * - UDPDiscovery: Peer discovery
 */
class NetworkPlugin : public INetworkAPI {
public:
    NetworkPlugin() = default;
    
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
        
        // Create components
        handshake_ = std::make_unique<HandshakeProtocol>(
            localPeerId_, sessionCode_, encryptionEnabled_
        );
        
        tcpHandler_ = std::make_unique<TCPHandler>(eventBus_, handshake_.get(), &bandwidthManager_);
        udpDiscovery_ = std::make_unique<UDPDiscovery>(eventBus_, localPeerId_);
        
        // Initialize TCP Relay for NAT traversal
        tcpRelay_ = std::make_unique<TCPRelay>("localhost", 9000);
        tcpRelay_->setDataCallback([this](const std::string& peerId, const std::vector<uint8_t>& data) {
            handleReceivedData(peerId, data);
        });
        tcpRelay_->setPeerCallback([this](const RelayPeer& peer) {
            // Publish relay peer discovery
            if (eventBus_) {
                std::string msg = "SENTINEL_RELAY|" + peer.peerId + "|" + std::to_string(peer.publicPort) + "|" + peer.publicIp;
                eventBus_->publish("PEER_DISCOVERED", msg);
            }
        });
        
        // Setup data callback for encryption/decryption
        tcpHandler_->setDataCallback([this](const std::string& peerId, 
                                           const std::vector<uint8_t>& data) {
            handleReceivedData(peerId, data);
        });
        
        return true;
    }

    void shutdown() override {
        std::cout << "NetworkPlugin shutdown" << std::endl;
        
        if (tcpRelay_) {
            tcpRelay_->disconnect();
        }
        
        if (udpDiscovery_) {
            udpDiscovery_->stopDiscovery();
        }
        
        if (tcpHandler_) {
            tcpHandler_->stopListening();
        }
    }

    std::string getName() const override { return "NetworkPlugin"; }
    std::string getVersion() const override { return "2.0.0"; }

    bool connectToPeer(const std::string& address, int port) override {
        if (!tcpHandler_) return false;
        return tcpHandler_->connectToPeer(address, port);
    }

    bool sendData(const std::string& peerId, const std::vector<uint8_t>& data) override {
        if (!tcpHandler_) return false;
        
        std::vector<uint8_t> dataToSend = data;
        
        // Encrypt if enabled
        if (encryptionEnabled_ && !encryptionKey_.empty()) {
            try {
                auto iv = SentinelFS::Crypto::generateIV();
                auto ciphertext = SentinelFS::Crypto::encrypt(data, encryptionKey_, iv);
                auto hmac = SentinelFS::Crypto::hmacSHA256(ciphertext, encryptionKey_);
                
                SentinelFS::EncryptedMessage msg;
                msg.iv = iv;
                msg.ciphertext = ciphertext;
                msg.hmac = hmac;
                
                dataToSend = msg.serialize();
                std::cout << "Data encrypted (" << data.size() << " -> " 
                          << dataToSend.size() << " bytes)" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Encryption failed: " << e.what() << std::endl;
                return false;
            }
        }

        // Try direct connection first
        if (tcpHandler_->isPeerConnected(peerId)) {
            return tcpHandler_->sendData(peerId, dataToSend);
        }
        
        // Fall back to relay if enabled and direct connection fails
        if (tcpRelay_ && tcpRelay_->isEnabled() && tcpRelay_->isConnected()) {
            std::cout << "Using relay for peer: " << peerId << std::endl;
            return tcpRelay_->sendToPeer(peerId, dataToSend);
        }
        
        return tcpHandler_->sendData(peerId, dataToSend);
    }

    void startListening(int port) override {
        if (tcpHandler_) {
            tcpHandler_->startListening(port);
        }
    }

    void startDiscovery(int port) override {
        if (udpDiscovery_) {
            udpDiscovery_->startDiscovery(port);
        }
    }

    void broadcastPresence(int discoveryPort, int tcpPort) override {
        if (udpDiscovery_) {
            udpDiscovery_->broadcastPresence(discoveryPort, tcpPort);
        }
    }

    int measureRTT(const std::string& peerId) override {
        if (!tcpHandler_) return -1;
        return tcpHandler_->measureRTT(peerId);
    }

    int getPeerRTT(const std::string& peerId) override {
        // RTT is measured on-demand, not cached
        return measureRTT(peerId);
    }

    void disconnectPeer(const std::string& peerId) override {
        if (tcpHandler_) {
            tcpHandler_->disconnectPeer(peerId);
        }
    }

    bool isPeerConnected(const std::string& peerId) override {
        if (!tcpHandler_) return false;
        return tcpHandler_->isPeerConnected(peerId);
    }

    void setSessionCode(const std::string& code) override {
        sessionCode_ = code;
        if (handshake_) {
            handshake_->setSessionCode(code);
        }
        std::cout << "Session code updated" << std::endl;
    }

    std::string getSessionCode() const override {
        return sessionCode_;
    }

    void setEncryptionEnabled(bool enable) override {
        encryptionEnabled_ = enable;
        
        if (handshake_) {
            handshake_->setEncryptionEnabled(enable);
        }
        
        std::cout << "Encryption " << (enable ? "enabled" : "disabled") << std::endl;
        
        // Derive key if enabling and session code exists
        if (enable && !sessionCode_.empty()) {
            try {
                std::vector<uint8_t> salt = {
                    0x53, 0x65, 0x6E, 0x74, 0x69, 0x6E, 0x65, 0x6C, 
                    0x46, 0x53, 0x5F, 0x32, 0x30, 0x32, 0x35
                };
                encryptionKey_ = SentinelFS::Crypto::deriveKeyFromSessionCode(sessionCode_, salt);
                std::cout << "Encryption key derived from session code" << std::endl;
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

    void setGlobalUploadLimit(std::size_t bytesPerSecond) override {
        bandwidthManager_.setGlobalUploadLimit(bytesPerSecond);
    }

    void setGlobalDownloadLimit(std::size_t bytesPerSecond) override {
        bandwidthManager_.setGlobalDownloadLimit(bytesPerSecond);
    }

    std::string getBandwidthStats() const override {
        auto stats = bandwidthManager_.getStats();

        std::ostringstream ss;

        ss << "Global Upload Limit: ";
        if (stats.globalUploadLimit > 0) {
            ss << (stats.globalUploadLimit / 1024) << " KB/s";
        } else {
            ss << "Unlimited";
        }
        ss << "\n";

        ss << "Global Download Limit: ";
        if (stats.globalDownloadLimit > 0) {
            ss << (stats.globalDownloadLimit / 1024) << " KB/s";
        } else {
            ss << "Unlimited";
        }
        ss << "\n";

        double uploadedMB = stats.totalUploaded / (1024.0 * 1024.0);
        double downloadedMB = stats.totalDownloaded / (1024.0 * 1024.0);

        ss << std::fixed << std::setprecision(2);
        ss << "Total Uploaded (limiter): " << uploadedMB << " MB\n";
        ss << "Total Downloaded (limiter): " << downloadedMB << " MB\n";
        ss << "Upload Wait Time: " << stats.uploadWaitMs << " ms\n";
        ss << "Download Wait Time: " << stats.downloadWaitMs << " ms\n";
        ss << "Active Peers (with limits): " << stats.activePeers;

        return ss.str();
    }

private:
    void handleReceivedData(const std::string& peerId, const std::vector<uint8_t>& data) {
        std::vector<uint8_t> decryptedData = data;
        
        // Decrypt if encryption is enabled
        if (encryptionEnabled_ && !encryptionKey_.empty()) {
            try {
                auto msg = SentinelFS::EncryptedMessage::deserialize(data);
                
                // Verify HMAC
                auto expectedHmac = SentinelFS::Crypto::hmacSHA256(msg.ciphertext, encryptionKey_);
                if (msg.hmac != expectedHmac) {
                    std::cerr << "HMAC verification failed from " << peerId << std::endl;
                    return;
                }
                
                // Decrypt
                decryptedData = SentinelFS::Crypto::decrypt(msg.ciphertext, encryptionKey_, msg.iv);
                std::cout << "Data decrypted (" << data.size() << " -> " 
                          << decryptedData.size() << " bytes)" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Decryption failed from " << peerId << ": " << e.what() << std::endl;
                return;
            }
        }
        
        // Bandwidth limiting is handled internally by TCPHandler
        
        // Publish decrypted data to event bus
        if (eventBus_) {
            eventBus_->publish("DATA_RECEIVED", std::make_pair(peerId, decryptedData));
        }
    }

    EventBus* eventBus_ = nullptr;
    std::string localPeerId_;
    std::string sessionCode_;
    bool encryptionEnabled_ = false;
    std::vector<uint8_t> encryptionKey_;
    
    // Modular components
    std::unique_ptr<HandshakeProtocol> handshake_;
    std::unique_ptr<TCPHandler> tcpHandler_;
    std::unique_ptr<UDPDiscovery> udpDiscovery_;
    std::unique_ptr<TCPRelay> tcpRelay_;

    BandwidthManager bandwidthManager_;
    
public:
    // Relay control methods (INetworkAPI interface)
    void setRelayEnabled(bool enabled) override {
        if (tcpRelay_) {
            tcpRelay_->setEnabled(enabled);
            if (enabled && !sessionCode_.empty()) {
                tcpRelay_->connect(localPeerId_, sessionCode_);
            }
        }
    }
    
    bool isRelayEnabled() const override {
        return tcpRelay_ && tcpRelay_->isEnabled();
    }
    
    bool isRelayConnected() const override {
        return tcpRelay_ && tcpRelay_->isConnected();
    }
    
    std::string getLocalPeerId() const override {
        return localPeerId_;
    }
    
    int getLocalPort() const override {
        if (tcpHandler_) {
            return tcpHandler_->getListeningPort();
        }
        return 0;
    }
};

// Plugin factory functions
extern "C" {
    IPlugin* create_plugin() {
        return new NetworkPlugin();
    }

    void destroy_plugin(IPlugin* plugin) {
        delete plugin;
    }
}

} // namespace SentinelFS
