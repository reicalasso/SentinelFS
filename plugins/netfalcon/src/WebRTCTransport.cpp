#include "WebRTCTransport.h"
#include "EventBus.h"
#include "Logger.h"
#include "BandwidthLimiter.h"
#include "MetricsCollector.h"
#include <algorithm>

namespace SentinelFS {
namespace NetFalcon {

WebRTCTransport::WebRTCTransport(EventBus* eventBus, SessionManager* sessionManager, BandwidthManager* bandwidth)
    : eventBus_(eventBus)
    , sessionManager_(sessionManager)
    , bandwidthManager_(bandwidth)
{
#ifdef HAVE_WEBRTC
    // Initialize libdatachannel
    rtc::InitLogger(rtc::LogLevel::Warning);
    
    // Setup default configuration
    rtcConfig_.iceServers.emplace_back("stun:stun.l.google.com:19302");
    rtcConfig_.iceServers.emplace_back("stun:stun1.l.google.com:19302");
#endif
    
    Logger::instance().log(LogLevel::INFO, "WebRTC transport initialized", "WebRTCTransport");
}

WebRTCTransport::~WebRTCTransport() {
    shutdown();
}

bool WebRTCTransport::isAvailable() {
#ifdef HAVE_WEBRTC
    return true;
#else
    return false;
#endif
}

std::string WebRTCTransport::getLibraryVersion() {
#ifdef HAVE_WEBRTC
    return "libdatachannel";
#else
    return "Not available";
#endif
}

void WebRTCTransport::setIceServers(const std::vector<std::string>& stunServers,
                                     const std::vector<std::string>& turnServers) {
    stunServers_ = stunServers;
    turnServers_ = turnServers;
    
#ifdef HAVE_WEBRTC
    rtcConfig_.iceServers.clear();
    for (const auto& server : stunServers) {
        rtcConfig_.iceServers.emplace_back(server);
    }
    for (const auto& server : turnServers) {
        rtcConfig_.iceServers.emplace_back(server);
    }
#endif
}

bool WebRTCTransport::startListening(int port) {
    (void)port; // WebRTC doesn't use traditional listening
    
    auto& logger = Logger::instance();
    
    if (!isAvailable()) {
        logger.log(LogLevel::WARN, "WebRTC not available", "WebRTCTransport");
        return false;
    }
    
    running_ = true;
    logger.log(LogLevel::INFO, "WebRTC transport ready for connections", "WebRTCTransport");
    return true;
}

void WebRTCTransport::stopListening() {
    running_ = false;
}

#ifdef HAVE_WEBRTC
std::shared_ptr<rtc::PeerConnection> WebRTCTransport::createPeerConnection(const std::string& peerId) {
    auto pc = std::make_shared<rtc::PeerConnection>(rtcConfig_);
    
    pc->onStateChange([this, peerId](rtc::PeerConnection::State state) {
        auto& logger = Logger::instance();
        
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(peerId);
        if (it == connections_.end()) return;
        
        switch (state) {
            case rtc::PeerConnection::State::Connected:
                it->second.state = ConnectionState::CONNECTED;
                it->second.connectedAt = std::chrono::steady_clock::now();
                logger.log(LogLevel::INFO, "WebRTC connected: " + peerId, "WebRTCTransport");
                emitEvent(TransportEvent::CONNECTED, peerId);
                break;
            case rtc::PeerConnection::State::Disconnected:
            case rtc::PeerConnection::State::Failed:
            case rtc::PeerConnection::State::Closed:
                it->second.state = ConnectionState::DISCONNECTED;
                logger.log(LogLevel::INFO, "WebRTC disconnected: " + peerId, "WebRTCTransport");
                emitEvent(TransportEvent::DISCONNECTED, peerId);
                break;
            default:
                break;
        }
    });
    
    pc->onGatheringStateChange([this, peerId](rtc::PeerConnection::GatheringState state) {
        if (state == rtc::PeerConnection::GatheringState::Complete) {
            std::lock_guard<std::mutex> lock(connMutex_);
            auto it = connections_.find(peerId);
            if (it != connections_.end() && signalingCallback_) {
                // Send all pending candidates
                for (const auto& candidate : it->second.pendingCandidates) {
                    signalingCallback_(peerId, "candidate", candidate);
                }
                it->second.pendingCandidates.clear();
            }
        }
    });
    
    pc->onLocalCandidate([this, peerId](rtc::Candidate candidate) {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(peerId);
        if (it != connections_.end()) {
            std::string candidateStr = std::string(candidate);
            if (signalingCallback_) {
                signalingCallback_(peerId, "candidate", candidateStr);
            } else {
                it->second.pendingCandidates.push_back(candidateStr);
            }
        }
    });
    
    pc->onLocalDescription([this, peerId](rtc::Description description) {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(peerId);
        if (it != connections_.end()) {
            it->second.localDescription = std::string(description);
            if (signalingCallback_) {
                std::string type = (description.type() == rtc::Description::Type::Offer) ? "offer" : "answer";
                signalingCallback_(peerId, type, it->second.localDescription);
            }
        }
    });
    
    pc->onDataChannel([this, peerId](std::shared_ptr<rtc::DataChannel> dc) {
        setupDataChannel(peerId, dc);
    });
    
    return pc;
}

void WebRTCTransport::setupDataChannel(const std::string& peerId, std::shared_ptr<rtc::DataChannel> dc) {
    auto& logger = Logger::instance();
    
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(peerId);
        if (it != connections_.end()) {
            it->second.dataChannel = dc;
        }
    }
    
    dc->onOpen([this, peerId]() {
        Logger::instance().log(LogLevel::INFO, "WebRTC data channel open: " + peerId, "WebRTCTransport");
    });
    
    dc->onClosed([this, peerId]() {
        Logger::instance().log(LogLevel::INFO, "WebRTC data channel closed: " + peerId, "WebRTCTransport");
    });
    
    dc->onMessage([this, peerId](rtc::message_variant data) {
        std::vector<uint8_t> recvData;
        
        if (std::holds_alternative<rtc::binary>(data)) {
            auto& binary = std::get<rtc::binary>(data);
            recvData.reserve(binary.size());
            for (auto b : binary) {
                recvData.push_back(static_cast<uint8_t>(b));
            }
        } else if (std::holds_alternative<std::string>(data)) {
            auto& str = std::get<std::string>(data);
            recvData.assign(str.begin(), str.end());
        }
        
        if (!recvData.empty()) {
            MetricsCollector::instance().incrementBytesReceived(recvData.size());
            emitEvent(TransportEvent::DATA_RECEIVED, peerId, "", recvData);
        }
        
        std::lock_guard<std::mutex> lock(connMutex_);
        auto it = connections_.find(peerId);
        if (it != connections_.end()) {
            it->second.lastActivity = std::chrono::steady_clock::now();
        }
    });
}
#endif

bool WebRTCTransport::connect(const std::string& address, int port, const std::string& peerId) {
    (void)address;
    (void)port;
    
    auto& logger = Logger::instance();
    
    if (!isAvailable()) {
        logger.log(LogLevel::WARN, "WebRTC connect failed - not available", "WebRTCTransport");
        return false;
    }
    
#ifdef HAVE_WEBRTC
    std::string targetPeer = peerId.empty() ? address : peerId;
    
    // Create connection info
    WebRTCConnectionInfo connInfo;
    connInfo.peerId = targetPeer;
    connInfo.state = ConnectionState::CONNECTING;
    connInfo.isOfferer = true;
    
    // Create peer connection
    connInfo.peerConnection = createPeerConnection(targetPeer);
    
    // Create data channel
    connInfo.dataChannel = connInfo.peerConnection->createDataChannel("sentinel");
    setupDataChannel(targetPeer, connInfo.dataChannel);
    
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        connections_[targetPeer] = std::move(connInfo);
    }
    
    logger.log(LogLevel::INFO, "WebRTC initiating connection to: " + targetPeer, "WebRTCTransport");
    return true;
#else
    return false;
#endif
}

std::string WebRTCTransport::createOffer(const std::string& peerId) {
#ifdef HAVE_WEBRTC
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it == connections_.end() || !it->second.peerConnection) {
        return "";
    }
    
    it->second.peerConnection->setLocalDescription(rtc::Description::Type::Offer);
    
    // Wait a bit for description to be set
    // In real implementation, use async callback
    return it->second.localDescription;
#else
    (void)peerId;
    return "";
#endif
}

std::string WebRTCTransport::handleOffer(const std::string& peerId, const std::string& sdp) {
    auto& logger = Logger::instance();
    
#ifdef HAVE_WEBRTC
    // Create connection if doesn't exist
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        if (connections_.find(peerId) == connections_.end()) {
            WebRTCConnectionInfo connInfo;
            connInfo.peerId = peerId;
            connInfo.state = ConnectionState::CONNECTING;
            connInfo.isOfferer = false;
            connInfo.peerConnection = createPeerConnection(peerId);
            connections_[peerId] = std::move(connInfo);
        }
    }
    
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it == connections_.end() || !it->second.peerConnection) {
        return "";
    }
    
    // Set remote description (offer)
    it->second.peerConnection->setRemoteDescription(rtc::Description(sdp, rtc::Description::Type::Offer));
    
    // Create answer
    it->second.peerConnection->setLocalDescription(rtc::Description::Type::Answer);
    
    logger.log(LogLevel::INFO, "WebRTC handling offer from: " + peerId, "WebRTCTransport");
    return it->second.localDescription;
#else
    (void)peerId;
    (void)sdp;
    return "";
#endif
}

void WebRTCTransport::handleAnswer(const std::string& peerId, const std::string& sdp) {
#ifdef HAVE_WEBRTC
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it == connections_.end() || !it->second.peerConnection) {
        return;
    }
    
    it->second.peerConnection->setRemoteDescription(rtc::Description(sdp, rtc::Description::Type::Answer));
    Logger::instance().log(LogLevel::INFO, "WebRTC handling answer from: " + peerId, "WebRTCTransport");
#else
    (void)peerId;
    (void)sdp;
#endif
}

void WebRTCTransport::addIceCandidate(const std::string& peerId, const std::string& candidate) {
#ifdef HAVE_WEBRTC
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it == connections_.end() || !it->second.peerConnection) {
        return;
    }
    
    it->second.peerConnection->addRemoteCandidate(rtc::Candidate(candidate));
#else
    (void)peerId;
    (void)candidate;
#endif
}

void WebRTCTransport::disconnect(const std::string& peerId) {
    auto& logger = Logger::instance();
    
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it == connections_.end()) return;
    
#ifdef HAVE_WEBRTC
    if (it->second.dataChannel) {
        it->second.dataChannel->close();
    }
    if (it->second.peerConnection) {
        it->second.peerConnection->close();
    }
#endif
    
    connections_.erase(it);
    
    logger.log(LogLevel::INFO, "WebRTC disconnected: " + peerId, "WebRTCTransport");
    emitEvent(TransportEvent::DISCONNECTED, peerId);
}

bool WebRTCTransport::send(const std::string& peerId, const std::vector<uint8_t>& data) {
    if (!isAvailable()) return false;
    
#ifdef HAVE_WEBRTC
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it == connections_.end() || !it->second.dataChannel) {
        return false;
    }
    
    if (!it->second.dataChannel->isOpen()) {
        return false;
    }
    
    try {
        // Convert uint8_t to std::byte for rtc::binary
        rtc::binary binary(data.size());
        std::transform(data.begin(), data.end(), binary.begin(),
                       [](uint8_t b) { return std::byte(b); });
        it->second.dataChannel->send(binary);
        MetricsCollector::instance().incrementBytesSent(data.size());
        return true;
    } catch (const std::exception& e) {
        Logger::instance().log(LogLevel::ERROR, "WebRTC send failed: " + std::string(e.what()), "WebRTCTransport");
        return false;
    }
#else
    (void)peerId;
    (void)data;
    return false;
#endif
}

bool WebRTCTransport::isConnected(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    return it != connections_.end() && it->second.state == ConnectionState::CONNECTED;
}

ConnectionState WebRTCTransport::getConnectionState(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    return it != connections_.end() ? it->second.state : ConnectionState::DISCONNECTED;
}

ConnectionQuality WebRTCTransport::getConnectionQuality(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it == connections_.end()) return ConnectionQuality{};
    return it->second.quality;
}

std::vector<std::string> WebRTCTransport::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(connMutex_);
    std::vector<std::string> peers;
    for (const auto& [peerId, info] : connections_) {
        if (info.state == ConnectionState::CONNECTED) {
            peers.push_back(peerId);
        }
    }
    return peers;
}

void WebRTCTransport::setEventCallback(TransportEventCallback callback) {
    eventCallback_ = std::move(callback);
}

void WebRTCTransport::setSignalingCallback(SignalingCallback callback) {
    signalingCallback_ = std::move(callback);
}

int WebRTCTransport::measureRTT(const std::string& peerId) {
    // WebRTC doesn't expose RTT directly in libdatachannel
    // Would need to implement ping/pong at application level
    (void)peerId;
    return -1;
}

void WebRTCTransport::shutdown() {
    stopListening();
    
    std::lock_guard<std::mutex> lock(connMutex_);
    for (auto& [peerId, info] : connections_) {
#ifdef HAVE_WEBRTC
        if (info.dataChannel) {
            info.dataChannel->close();
        }
        if (info.peerConnection) {
            info.peerConnection->close();
        }
#endif
    }
    connections_.clear();
}

void WebRTCTransport::emitEvent(TransportEvent event, const std::string& peerId,
                                 const std::string& message, const std::vector<uint8_t>& data) {
    if (eventCallback_) {
        TransportEventData eventData;
        eventData.event = event;
        eventData.peerId = peerId;
        eventData.message = message;
        eventData.data = data;
        eventCallback_(eventData);
    }
    
    if (eventBus_) {
        switch (event) {
            case TransportEvent::CONNECTED:
                eventBus_->publish("WEBRTC_PEER_CONNECTED", peerId);
                break;
            case TransportEvent::DISCONNECTED:
                eventBus_->publish("WEBRTC_PEER_DISCONNECTED", peerId);
                break;
            case TransportEvent::DATA_RECEIVED:
                eventBus_->publish("WEBRTC_DATA_RECEIVED", std::make_pair(peerId, data));
                break;
            default:
                break;
        }
    }
}

} // namespace NetFalcon
} // namespace SentinelFS
