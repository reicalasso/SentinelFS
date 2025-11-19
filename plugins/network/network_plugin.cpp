#include "INetworkAPI.h"
#include "EventBus.h"
#include "Crypto.h"
#include "HandshakeProtocol.h"
#include "TCPHandler.h"
#include "UDPDiscovery.h"
#include <iostream>
#include <memory>
#include <random>

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
        
        tcpHandler_ = std::make_unique<TCPHandler>(eventBus_, handshake_.get());
        udpDiscovery_ = std::make_unique<UDPDiscovery>(eventBus_, localPeerId_);
        
        // Setup data callback for encryption/decryption
        tcpHandler_->setDataCallback([this](const std::string& peerId, 
                                           const std::vector<uint8_t>& data) {
            handleReceivedData(peerId, data);
        });
        
        return true;
    }

    void shutdown() override {
        std::cout << "NetworkPlugin shutdown" << std::endl;
        
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
                auto iv = sentinel::Crypto::generateIV();
                auto ciphertext = sentinel::Crypto::encrypt(data, encryptionKey_, iv);
                auto hmac = sentinel::Crypto::hmacSHA256(ciphertext, encryptionKey_);
                
                sentinel::EncryptedMessage msg;
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
                encryptionKey_ = sentinel::Crypto::deriveKeyFromSessionCode(sessionCode_, salt);
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

private:
    void handleReceivedData(const std::string& peerId, const std::vector<uint8_t>& data) {
        std::vector<uint8_t> decryptedData = data;
        
        // Decrypt if encryption is enabled
        if (encryptionEnabled_ && !encryptionKey_.empty()) {
            try {
                auto msg = sentinel::EncryptedMessage::deserialize(data);
                
                // Verify HMAC
                auto expectedHmac = sentinel::Crypto::hmacSHA256(msg.ciphertext, encryptionKey_);
                if (msg.hmac != expectedHmac) {
                    std::cerr << "HMAC verification failed from " << peerId << std::endl;
                    return;
                }
                
                // Decrypt
                decryptedData = sentinel::Crypto::decrypt(msg.ciphertext, encryptionKey_, msg.iv);
                std::cout << "Data decrypted (" << data.size() << " -> " 
                          << decryptedData.size() << " bytes)" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Decryption failed from " << peerId << ": " << e.what() << std::endl;
                return;
            }
        }
        
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
