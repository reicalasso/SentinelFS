#pragma once

/**
 * @file QUICTransport.h
 * @brief QUIC transport implementation for NetFalcon using ngtcp2
 * 
 * QUIC provides:
 * - Multiplexed streams over single connection
 * - Built-in encryption (TLS 1.3)
 * - Connection migration
 * - Reduced latency (0-RTT)
 * - Better congestion control
 */

#include "ITransport.h"
#include "SessionManager.h"
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <chrono>

#ifdef HAVE_NGTCP2
#include <ngtcp2/ngtcp2.h>
#include <ngtcp2/ngtcp2_crypto.h>
#ifdef HAVE_GNUTLS
#include <ngtcp2/ngtcp2_crypto_gnutls.h>
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#endif
#endif

namespace SentinelFS {

class EventBus;
class BandwidthManager;

namespace NetFalcon {

/**
 * @brief QUIC connection info
 */
struct QUICConnectionInfo {
    std::string peerId;
    std::string address;
    int port{0};
    ConnectionState state{ConnectionState::DISCONNECTED};
    ConnectionQuality quality;
    std::chrono::steady_clock::time_point connectedAt;
    std::chrono::steady_clock::time_point lastActivity;
    
#ifdef HAVE_NGTCP2
    ngtcp2_conn* conn{nullptr};
#ifdef HAVE_GNUTLS
    gnutls_session_t session{nullptr};
    gnutls_certificate_credentials_t cred{nullptr};
#endif
#endif
    int socket{-1};
};

/**
 * @brief QUIC transport implementation using ngtcp2
 */
class QUICTransport : public ITransport {
public:
    QUICTransport(EventBus* eventBus, SessionManager* sessionManager, BandwidthManager* bandwidth);
    ~QUICTransport() override;

    // ITransport interface
    TransportType getType() const override { return TransportType::QUIC; }
    std::string getName() const override { return "QUIC"; }
    
    bool startListening(int port) override;
    void stopListening() override;
    int getListeningPort() const override { return listeningPort_; }
    
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
     * @brief Check if QUIC library is available
     */
    static bool isAvailable();
    
    /**
     * @brief Get QUIC library version
     */
    static std::string getLibraryVersion();

private:
    void emitEvent(TransportEvent event, const std::string& peerId,
                   const std::string& message = "", const std::vector<uint8_t>& data = {});
    
    // Event loop
    void eventLoop();
    void processConnection(QUICConnectionInfo& conn);
    
    // ngtcp2 callbacks
#ifdef HAVE_NGTCP2
    bool initTLS();
    void handlePacket(const uint8_t* data, size_t len, const sockaddr* addr, socklen_t addrlen);
    int sendPacket(QUICConnectionInfo& conn);
    int writeStreams(QUICConnectionInfo& conn);
    
    // ngtcp2 callbacks
    static int recvStreamDataCallback(ngtcp2_conn* conn, uint32_t flags, int64_t stream_id,
                                      uint64_t offset, const uint8_t* data, size_t datalen,
                                      void* user_data, void* stream_user_data);
    static int ackedStreamDataOffsetCallback(ngtcp2_conn* conn, int64_t stream_id,
                                             uint64_t offset, uint64_t datalen, void* user_data,
                                             void* stream_user_data);
    static int streamOpenCallback(ngtcp2_conn* conn, int64_t stream_id, void* user_data);
    static int streamCloseCallback(ngtcp2_conn* conn, uint32_t flags, int64_t stream_id,
                                   uint64_t app_error_code, void* user_data, void* stream_user_data);
    static void randCallback(uint8_t* dest, size_t destlen, const ngtcp2_rand_ctx* rand_ctx);
    static int getNewConnectionIdCallback(ngtcp2_conn* conn, ngtcp2_cid* cid, uint8_t* token,
                                          size_t cidlen, void* user_data);
    static int removeConnectionIdCallback(ngtcp2_conn* conn, const ngtcp2_cid* cid, void* user_data);
    static int handshakeCompleted(ngtcp2_conn* conn, void* user_data);
    
#ifdef HAVE_GNUTLS
    gnutls_certificate_credentials_t tlsCred_{nullptr};
#endif
#endif

    EventBus* eventBus_;
    SessionManager* sessionManager_;
    BandwidthManager* bandwidthManager_;
    TransportEventCallback eventCallback_;
    
    int serverSocket_{-1};
    int listeningPort_{0};
    std::atomic<bool> running_{false};
    
    std::thread eventThread_;
    
    mutable std::mutex connMutex_;
    std::map<std::string, QUICConnectionInfo> connections_;
    
    // Pending data to send
    mutable std::mutex sendMutex_;
    std::map<std::string, std::queue<std::vector<uint8_t>>> sendQueues_;
};

} // namespace NetFalcon
} // namespace SentinelFS
