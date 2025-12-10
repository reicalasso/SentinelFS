#pragma once

/**
 * @file WebRTCTransport.h
 * @brief WebRTC transport implementation for NetFalcon using libdatachannel
 * 
 * WebRTC provides:
 * - NAT traversal via ICE (STUN/TURN)
 * - Peer-to-peer data channels
 * - DTLS encryption
 * - Browser compatibility
 */

#include "ITransport.h"
#include "SessionManager.h"
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <chrono>
#include <functional>
#include <memory>

#ifdef HAVE_WEBRTC
#include <rtc/rtc.hpp>
#endif

namespace SentinelFS {

class EventBus;
class BandwidthManager;

namespace NetFalcon {

/**
 * @brief WebRTC connection info
 */
struct WebRTCConnectionInfo {
    std::string peerId;
    ConnectionState state{ConnectionState::DISCONNECTED};
    ConnectionQuality quality;
    std::chrono::steady_clock::time_point connectedAt;
    std::chrono::steady_clock::time_point lastActivity;
    
#ifdef HAVE_WEBRTC
    std::shared_ptr<rtc::PeerConnection> peerConnection;
    std::shared_ptr<rtc::DataChannel> dataChannel;
#endif
    
    // ICE candidates to send
    std::vector<std::string> pendingCandidates;
    std::string localDescription;
    bool isOfferer{false};
};

/**
 * @brief Signaling callback for WebRTC
 * Used to exchange SDP and ICE candidates between peers
 */
using SignalingCallback = std::function<void(const std::string& peerId, 
                                              const std::string& type,
                                              const std::string& data)>;

/**
 * @brief WebRTC transport implementation using libdatachannel
 */
class WebRTCTransport : public ITransport {
public:
    WebRTCTransport(EventBus* eventBus, SessionManager* sessionManager, BandwidthManager* bandwidth);
    ~WebRTCTransport() override;

    // ITransport interface
    TransportType getType() const override { return TransportType::WEBRTC; }
    std::string getName() const override { return "WebRTC"; }
    
    bool startListening(int port) override;
    void stopListening() override;
    int getListeningPort() const override { return 0; } // WebRTC doesn't use traditional ports
    
    bool connect(const std::string& address, int port, const std::string& peerId = "") override;
    void disconnect(const std::string& peerId) override;
    bool send(const std::string& peerId, const std::vector<uint8_t>& data) override;
    
    bool isConnected(const std::string& peerId) const override;
    ConnectionState getConnectionState(const std::string& peerId) const override;
    ConnectionQuality getConnectionQuality(const std::string& peerId) const override;
    std::vector<std::string> getConnectedPeers() const override;
    
    void setEventCallback(TransportEventCallback callback) override;
    int measureRTT(const std::string& peerId) override;
    void shutdown() override;

    /**
     * @brief Check if WebRTC library is available
     */
    static bool isAvailable();
    
    /**
     * @brief Get WebRTC library version
     */
    static std::string getLibraryVersion();
    
    // WebRTC-specific methods
    
    /**
     * @brief Set signaling callback for SDP/ICE exchange
     */
    void setSignalingCallback(SignalingCallback callback);
    
    /**
     * @brief Create an offer to connect to a peer
     * @return Local SDP description to send to peer
     */
    std::string createOffer(const std::string& peerId);
    
    /**
     * @brief Handle incoming offer from peer
     * @return Local SDP answer to send back
     */
    std::string handleOffer(const std::string& peerId, const std::string& sdp);
    
    /**
     * @brief Handle incoming answer from peer
     */
    void handleAnswer(const std::string& peerId, const std::string& sdp);
    
    /**
     * @brief Add ICE candidate from peer
     */
    void addIceCandidate(const std::string& peerId, const std::string& candidate);
    
    /**
     * @brief Set STUN/TURN servers
     */
    void setIceServers(const std::vector<std::string>& stunServers,
                       const std::vector<std::string>& turnServers = {});

private:
    void emitEvent(TransportEvent event, const std::string& peerId,
                   const std::string& message = "", const std::vector<uint8_t>& data = {});
    
#ifdef HAVE_WEBRTC
    std::shared_ptr<rtc::PeerConnection> createPeerConnection(const std::string& peerId);
    void setupDataChannel(const std::string& peerId, std::shared_ptr<rtc::DataChannel> dc);
    rtc::Configuration rtcConfig_;
#endif

    EventBus* eventBus_;
    SessionManager* sessionManager_;
    BandwidthManager* bandwidthManager_;
    TransportEventCallback eventCallback_;
    SignalingCallback signalingCallback_;
    
    std::atomic<bool> running_{false};
    
    mutable std::mutex connMutex_;
    std::map<std::string, WebRTCConnectionInfo> connections_;
    
    // Default STUN servers
    std::vector<std::string> stunServers_{"stun:stun.l.google.com:19302"};
    std::vector<std::string> turnServers_;
};

} // namespace NetFalcon
} // namespace SentinelFS
