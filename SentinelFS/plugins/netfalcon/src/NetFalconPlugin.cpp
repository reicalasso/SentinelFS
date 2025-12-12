#include "NetFalconPlugin.h"
#include "TCPTransport.h"
#include "EventBus.h"
#include "Logger.h"
#include "MetricsCollector.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>

namespace SentinelFS {
namespace NetFalcon {

NetFalconPlugin::NetFalconPlugin() = default;

NetFalconPlugin::~NetFalconPlugin() {
    shutdown();
}

bool NetFalconPlugin::initialize(EventBus* eventBus) {
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Initializing NetFalcon plugin", "NetFalcon");
    
    eventBus_ = eventBus;
    
    // Generate local peer ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 99999);
    std::string localPeerId = "FALCON_" + std::to_string(dis(gen));
    
    sessionManager_.setLocalPeerId(localPeerId);
    logger.log(LogLevel::INFO, "Local Peer ID: " + localPeerId, "NetFalcon");
    
    // Setup transports
    setupTransports();
    
    // Setup event handlers
    setupEventHandlers();
    
    logger.log(LogLevel::INFO, "NetFalcon plugin initialized", "NetFalcon");
    return true;
}

void NetFalconPlugin::shutdown() {
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Shutting down NetFalcon plugin", "NetFalcon");
    
    discoveryService_.stop();
    transportRegistry_.shutdownAll();
    
    logger.log(LogLevel::INFO, "NetFalcon plugin shutdown complete", "NetFalcon");
}

void NetFalconPlugin::setupTransports() {
    auto& logger = Logger::instance();
    
    // Create TCP transport
    if (config_.enableTcp) {
        auto tcpTransport = std::make_unique<TCPTransport>(eventBus_, &sessionManager_, &bandwidthManager_);
        tcpTransport->setMaxConnections(config_.maxConnections);
        tcpTransport->setAutoReconnect(config_.autoReconnect);
        
        // Set event callback
        tcpTransport->setEventCallback([this](const TransportEventData& event) {
            handleTransportEvent(event);
        });
        
        transportRegistry_.registerTransport(TransportType::TCP, std::move(tcpTransport));
        logger.log(LogLevel::DEBUG, "TCP transport registered", "NetFalcon");
    }
    
    // Create QUIC transport (if enabled and available)
    if (config_.enableQuic) {
        if (QUICTransport::isAvailable()) {
            auto quicTransport = std::make_unique<QUICTransport>(eventBus_, &sessionManager_, &bandwidthManager_);
            
            quicTransport->setEventCallback([this](const TransportEventData& event) {
                handleTransportEvent(event);
            });
            
            transportRegistry_.registerTransport(TransportType::QUIC, std::move(quicTransport));
            logger.log(LogLevel::DEBUG, "QUIC transport registered", "NetFalcon");
        } else {
            logger.log(LogLevel::WARN, "QUIC enabled but library not available", "NetFalcon");
        }
    }
    
    // Create WebRTC transport (before Relay - it has built-in NAT traversal)
    if (config_.enableWebRtc) {
        if (WebRTCTransport::isAvailable()) {
            auto webrtcTransport = std::make_unique<WebRTCTransport>(eventBus_, &sessionManager_, &bandwidthManager_);
            
            webrtcTransport->setEventCallback([this](const TransportEventData& event) {
                handleTransportEvent(event);
            });
            
            // WebRTC requires signaling - set up callback for SDP/ICE exchange
            webrtcTransport->setSignalingCallback([this](const std::string& peerId, 
                                                          const std::string& type,
                                                          const std::string& data) {
                // Publish signaling data through event bus for relay/other transport
                if (eventBus_) {
                    eventBus_->publish("WEBRTC_SIGNALING", peerId + "|" + type + "|" + data);
                }
            });
            
            // Keep a raw pointer for direct access
            webrtcTransport_ = webrtcTransport.get();
            
            // Register in TransportRegistry for automatic selection
            transportRegistry_.registerTransport(TransportType::WEBRTC, std::move(webrtcTransport));
            logger.log(LogLevel::DEBUG, "WebRTC transport registered", "NetFalcon");
        } else {
            logger.log(LogLevel::WARN, "WebRTC enabled but library not available", "NetFalcon");
        }
    }
    
    // Create Relay transport (fallback - always works)
    if (config_.enableRelay) {
        auto relayTransport = std::make_unique<RelayTransport>(eventBus_, &sessionManager_);
        
        relayTransport->setEventCallback([this](const TransportEventData& event) {
            handleTransportEvent(event);
        });
        
        // Keep a raw pointer for direct access (relay server connection)
        relayTransport_ = relayTransport.get();
        
        // Register in TransportRegistry for automatic selection
        transportRegistry_.registerTransport(TransportType::RELAY, std::move(relayTransport));
        logger.log(LogLevel::DEBUG, "Relay transport registered", "NetFalcon");
    }
    
    // Set transport strategy
    transportRegistry_.setStrategy(config_.transportStrategy);
}

void NetFalconPlugin::setupEventHandlers() {
    // Setup discovery callback
    discoveryService_.setDiscoveryCallback([this](const DiscoveredPeer& peer) {
        handleDiscoveredPeer(peer);
    });
}

void NetFalconPlugin::handleTransportEvent(const TransportEventData& event) {
    auto& logger = Logger::instance();
    
    switch (event.event) {
        case TransportEvent::CONNECTED:
            logger.log(LogLevel::INFO, "Peer connected: " + event.peerId, "NetFalcon");
            break;
            
        case TransportEvent::DISCONNECTED:
            logger.log(LogLevel::INFO, "Peer disconnected: " + event.peerId, "NetFalcon");
            {
                std::lock_guard<std::mutex> lock(rttMutex_);
                rttCache_.erase(event.peerId);
            }
            break;
            
        case TransportEvent::DATA_RECEIVED:
            handleReceivedData(event.peerId, event.data);
            break;
            
        case TransportEvent::QUALITY_CHANGED:
            transportRegistry_.updateQuality(event.peerId, TransportType::TCP, event.quality);
            break;
            
        default:
            break;
    }
}

void NetFalconPlugin::handleDiscoveredPeer(const DiscoveredPeer& peer) {
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Discovered peer: " + peer.peerId + " at " + peer.address + ":" + std::to_string(peer.port), "NetFalcon");
    
    // Publish to EventBus
    if (eventBus_) {
        std::string msg = "FALCON_DISCOVERY|" + peer.peerId + "|" + std::to_string(peer.port) + "|" + peer.address;
        eventBus_->publish("PEER_DISCOVERED", msg);
    }
}

void NetFalconPlugin::handleReceivedData(const std::string& peerId, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> processedData = data;
    
    // Decrypt if encryption enabled
    if (sessionManager_.isEncryptionEnabled()) {
        processedData = sessionManager_.decrypt(data, peerId);
        if (processedData.empty()) {
            Logger::instance().log(LogLevel::WARN, "Decryption failed from " + peerId, "NetFalcon");
            return;
        }
    }
    
    // Publish to EventBus
    if (eventBus_) {
        eventBus_->publish("DATA_RECEIVED", std::make_pair(peerId, processedData));
    }
}

// INetworkAPI Implementation

bool NetFalconPlugin::connectToPeer(const std::string& address, int port) {
    auto* transport = transportRegistry_.selectTransport("");
    if (!transport) {
        Logger::instance().log(LogLevel::ERROR, "No transport available", "NetFalcon");
        return false;
    }
    
    return transport->connect(address, port);
}

void NetFalconPlugin::disconnectPeer(const std::string& peerId) {
    auto binding = transportRegistry_.getBinding(peerId);
    if (binding) {
        auto* transport = transportRegistry_.getTransport(binding->activeTransport);
        if (transport) {
            transport->disconnect(peerId);
        }
    }
    
    transportRegistry_.unbindPeer(peerId);
}

bool NetFalconPlugin::isPeerConnected(const std::string& peerId) {
    auto binding = transportRegistry_.getBinding(peerId);
    if (binding) {
        auto* transport = transportRegistry_.getTransport(binding->activeTransport);
        if (transport) {
            return transport->isConnected(peerId);
        }
    }
    
    // Check all transports
    for (auto type : transportRegistry_.getRegisteredTransports()) {
        auto* transport = transportRegistry_.getTransport(type);
        if (transport && transport->isConnected(peerId)) {
            return true;
        }
    }
    
    return false;
}

bool NetFalconPlugin::sendData(const std::string& peerId, const std::vector<uint8_t>& data) {
    auto* transport = transportRegistry_.selectTransport(peerId);
    if (!transport) {
        Logger::instance().log(LogLevel::WARN, "No transport for peer: " + peerId, "NetFalcon");
        return false;
    }
    
    std::vector<uint8_t> dataToSend = data;
    
    // Encrypt if enabled
    if (sessionManager_.isEncryptionEnabled()) {
        dataToSend = sessionManager_.encrypt(data, peerId);
        if (dataToSend.empty()) {
            Logger::instance().log(LogLevel::ERROR, "Encryption failed", "NetFalcon");
            return false;
        }
    }
    
    return transport->send(peerId, dataToSend);
}

void NetFalconPlugin::startListening(int port) {
    listeningPort_ = port;
    
    auto* tcpTransport = transportRegistry_.getTransport(TransportType::TCP);
    if (tcpTransport) {
        tcpTransport->startListening(port);
    }
}

void NetFalconPlugin::startDiscovery(int port) {
    discoveryPort_ = port;
    
    DiscoveryConfig config = discoveryService_.getConfig();
    config.udpPort = port;
    discoveryService_.setConfig(config);
    
    discoveryService_.setLocalPeer(sessionManager_.getLocalPeerId(), listeningPort_, sessionManager_.getSessionCode());
    discoveryService_.start();
}

void NetFalconPlugin::broadcastPresence(int discoveryPort, int tcpPort) {
    discoveryService_.setLocalPeer(sessionManager_.getLocalPeerId(), tcpPort, sessionManager_.getSessionCode());
    discoveryService_.broadcastPresence();
}

int NetFalconPlugin::measureRTT(const std::string& peerId) {
    auto* transport = transportRegistry_.selectTransport(peerId);
    if (!transport) return -1;
    
    int rtt = transport->measureRTT(peerId);
    
    if (rtt >= 0) {
        std::lock_guard<std::mutex> lock(rttMutex_);
        rttCache_[peerId] = rtt;
    }
    
    return rtt;
}

int NetFalconPlugin::getPeerRTT(const std::string& peerId) {
    {
        std::lock_guard<std::mutex> lock(rttMutex_);
        auto it = rttCache_.find(peerId);
        if (it != rttCache_.end()) {
            return it->second;
        }
    }
    
    return measureRTT(peerId);
}

void NetFalconPlugin::setSessionCode(const std::string& code) {
    sessionManager_.setSessionCode(code, config_.encryptionEnabled);
    discoveryService_.setLocalPeer(sessionManager_.getLocalPeerId(), listeningPort_, code);
}

std::string NetFalconPlugin::getSessionCode() const {
    return sessionManager_.getSessionCode();
}

void NetFalconPlugin::setEncryptionEnabled(bool enable) {
    config_.encryptionEnabled = enable;
    sessionManager_.setEncryptionEnabled(enable);
}

bool NetFalconPlugin::isEncryptionEnabled() const {
    return sessionManager_.isEncryptionEnabled();
}

void NetFalconPlugin::setGlobalUploadLimit(std::size_t bytesPerSecond) {
    config_.globalUploadLimit = bytesPerSecond;
    bandwidthManager_.setGlobalUploadLimit(bytesPerSecond);
}

void NetFalconPlugin::setGlobalDownloadLimit(std::size_t bytesPerSecond) {
    config_.globalDownloadLimit = bytesPerSecond;
    bandwidthManager_.setGlobalDownloadLimit(bytesPerSecond);
}

std::string NetFalconPlugin::getBandwidthStats() const {
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
    ss << "Total Uploaded: " << uploadedMB << " MB\n";
    ss << "Total Downloaded: " << downloadedMB << " MB\n";
    ss << "Active Peers: " << stats.activePeers;
    
    return ss.str();
}

void NetFalconPlugin::setRelayEnabled(bool enabled) {
    config_.enableRelay = enabled;
    
    auto& logger = Logger::instance();
    logger.log(LogLevel::INFO, "Relay transport " + std::string(enabled ? "enabled" : "disabled"), "NetFalcon");
    
    if (enabled && !relayTransport_) {
        // Create and register relay transport
        auto relayTransport = std::make_unique<RelayTransport>(eventBus_, &sessionManager_);
        relayTransport->setEventCallback([this](const TransportEventData& event) {
            handleTransportEvent(event);
        });
        relayTransport_ = relayTransport.get();
        transportRegistry_.registerTransport(TransportType::RELAY, std::move(relayTransport));
    } else if (!enabled && relayTransport_) {
        relayTransport_->disconnectFromServer();
    }
}

bool NetFalconPlugin::isRelayEnabled() const {
    return config_.enableRelay;
}

bool NetFalconPlugin::isRelayConnected() const {
    return relayTransport_ && relayTransport_->isServerConnected();
}

std::string NetFalconPlugin::getLocalPeerId() const {
    return sessionManager_.getLocalPeerId();
}

int NetFalconPlugin::getLocalPort() const {
    return listeningPort_;
}

bool NetFalconPlugin::connectToRelay(const std::string& host, int port, const std::string& sessionCode) {
    auto& logger = Logger::instance();
    
    if (!config_.enableRelay) {
        logger.log(LogLevel::WARN, "Relay is disabled, enabling it first", "NetFalcon");
        setRelayEnabled(true);
    }
    
    if (!relayTransport_) {
        logger.log(LogLevel::ERROR, "Relay transport not available", "NetFalcon");
        return false;
    }
    
    // Set session code for discovery
    sessionManager_.setSessionCode(sessionCode, config_.encryptionEnabled);
    
    return relayTransport_->connectToServer(host, port, sessionCode);
}

void NetFalconPlugin::disconnectFromRelay() {
    if (relayTransport_) {
        relayTransport_->disconnectFromServer();
    }
}

std::vector<INetworkAPI::RelayPeerInfo> NetFalconPlugin::getRelayPeers() const {
    if (!relayTransport_) {
        return {};
    }
    
    auto relayPeers = relayTransport_->getRelayPeers();
    std::vector<INetworkAPI::RelayPeerInfo> result;
    result.reserve(relayPeers.size());
    
    for (const auto& peer : relayPeers) {
        INetworkAPI::RelayPeerInfo info;
        info.id = peer.peerId;
        info.ip = peer.publicIp;
        info.port = peer.publicPort;
        info.natType = peer.natType;
        info.connectedAt = peer.connectedAt;
        result.push_back(info);
    }
    
    return result;
}

void NetFalconPlugin::setConfig(const NetFalconConfig& config) {
    config_ = config;
    
    transportRegistry_.setStrategy(config.transportStrategy);
    bandwidthManager_.setGlobalUploadLimit(config.globalUploadLimit);
    bandwidthManager_.setGlobalDownloadLimit(config.globalDownloadLimit);
    
    discoveryService_.setConfig(config.discovery);
}

NetFalconConfig NetFalconPlugin::getConfig() const {
    return config_;
}

std::size_t NetFalconPlugin::getConnectedPeerCount() const {
    std::size_t count = 0;
    for (auto type : transportRegistry_.getRegisteredTransports()) {
        auto* transport = transportRegistry_.getTransport(type);
        if (transport) {
            count += transport->getConnectedPeers().size();
        }
    }
    return count;
}

std::vector<std::string> NetFalconPlugin::getConnectedPeers() const {
    std::vector<std::string> allPeers;
    for (auto type : transportRegistry_.getRegisteredTransports()) {
        auto* transport = transportRegistry_.getTransport(type);
        if (transport) {
            auto peers = transport->getConnectedPeers();
            allPeers.insert(allPeers.end(), peers.begin(), peers.end());
        }
    }
    return allPeers;
}

ConnectionQuality NetFalconPlugin::getConnectionQuality(const std::string& peerId) const {
    auto binding = transportRegistry_.getBinding(peerId);
    if (binding) {
        auto* transport = transportRegistry_.getTransport(binding->activeTransport);
        if (transport) {
            return transport->getConnectionQuality(peerId);
        }
    }
    return ConnectionQuality{};
}

void NetFalconPlugin::forceTransport(const std::string& peerId, TransportType type) {
    transportRegistry_.bindPeer(peerId, type);
}

// INetworkAPI Transport Strategy implementation
void NetFalconPlugin::setTransportStrategy(TransportStrategy strategy) {
    auto& logger = Logger::instance();
    
    // Map INetworkAPI::TransportStrategy to NetFalcon::TransportStrategy
    NetFalcon::TransportStrategy nfStrategy;
    switch (strategy) {
        case TransportStrategy::PREFER_FAST:
            nfStrategy = NetFalcon::TransportStrategy::PREFER_FAST;
            break;
        case TransportStrategy::PREFER_RELIABLE:
            nfStrategy = NetFalcon::TransportStrategy::PREFER_RELIABLE;
            break;
        case TransportStrategy::ADAPTIVE:
            nfStrategy = NetFalcon::TransportStrategy::ADAPTIVE;
            break;
        case TransportStrategy::FALLBACK_CHAIN:
        default:
            nfStrategy = NetFalcon::TransportStrategy::FALLBACK_CHAIN;
            break;
    }
    
    config_.transportStrategy = nfStrategy;
    transportRegistry_.setStrategy(nfStrategy);
    
    logger.log(LogLevel::INFO, "Transport strategy set to: " + std::to_string(static_cast<int>(strategy)), "NetFalcon");
}

INetworkAPI::TransportStrategy NetFalconPlugin::getTransportStrategy() const {
    switch (config_.transportStrategy) {
        case NetFalcon::TransportStrategy::PREFER_FAST:
            return TransportStrategy::PREFER_FAST;
        case NetFalcon::TransportStrategy::PREFER_RELIABLE:
            return TransportStrategy::PREFER_RELIABLE;
        case NetFalcon::TransportStrategy::ADAPTIVE:
            return TransportStrategy::ADAPTIVE;
        case NetFalcon::TransportStrategy::FALLBACK_CHAIN:
        default:
            return TransportStrategy::FALLBACK_CHAIN;
    }
}

void NetFalconPlugin::setTransportEnabled(const std::string& transport, bool enabled) {
    auto& logger = Logger::instance();
    
    if (transport == "tcp") {
        config_.enableTcp = enabled;
        logger.log(LogLevel::INFO, "TCP transport " + std::string(enabled ? "enabled" : "disabled"), "NetFalcon");
    } else if (transport == "quic") {
        config_.enableQuic = enabled;
        logger.log(LogLevel::INFO, "QUIC transport " + std::string(enabled ? "enabled" : "disabled"), "NetFalcon");
    } else if (transport == "relay") {
        setRelayEnabled(enabled);
    } else if (transport == "webrtc") {
        config_.enableWebRtc = enabled;
        logger.log(LogLevel::INFO, "WebRTC transport " + std::string(enabled ? "enabled" : "disabled"), "NetFalcon");
    }
}

bool NetFalconPlugin::isTransportEnabled(const std::string& transport) const {
    if (transport == "tcp") return config_.enableTcp;
    if (transport == "quic") return config_.enableQuic;
    if (transport == "relay") return config_.enableRelay;
    if (transport == "webrtc") return config_.enableWebRtc;
    return false;
}

std::vector<std::string> NetFalconPlugin::getAvailableTransports() const {
    std::vector<std::string> transports;
    if (config_.enableTcp) transports.push_back("tcp");
    if (config_.enableQuic) transports.push_back("quic");
    if (config_.enableRelay) transports.push_back("relay");
    if (config_.enableWebRtc) transports.push_back("webrtc");
    return transports;
}

std::vector<std::string> NetFalconPlugin::getConnectedPeerIds() const {
    return transportRegistry_.getConnectedPeerIds();
}

} // namespace NetFalcon

// Plugin factory functions
extern "C" {
    IPlugin* create_plugin() {
        return new NetFalcon::NetFalconPlugin();
    }
    
    void destroy_plugin(IPlugin* plugin) {
        delete plugin;
    }
}

} // namespace SentinelFS
