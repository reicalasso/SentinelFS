#include "QUICTransport.h"
#include "EventBus.h"
#include "Logger.h"
#include "BandwidthLimiter.h"
#include "MetricsCollector.h"
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <random>

#ifdef HAVE_NGTCP2
#ifdef HAVE_GNUTLS
#include <ngtcp2/ngtcp2_crypto_gnutls.h>
#endif
#endif

namespace SentinelFS {
namespace NetFalcon {

QUICTransport::QUICTransport(EventBus* eventBus, SessionManager* sessionManager, BandwidthManager* bandwidth)
    : eventBus_(eventBus)
    , sessionManager_(sessionManager)
    , bandwidthManager_(bandwidth)
{
#ifdef HAVE_NGTCP2
    initTLS();
#endif
}

QUICTransport::~QUICTransport() {
    shutdown();
#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)
    if (tlsCred_) {
        gnutls_certificate_free_credentials(tlsCred_);
        tlsCred_ = nullptr;
    }
#endif
}

bool QUICTransport::isAvailable() {
#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)
    return true;
#else
    return false;
#endif
}

std::string QUICTransport::getLibraryVersion() {
#ifdef HAVE_NGTCP2
    return "ngtcp2 " + std::string(NGTCP2_VERSION) + " + GnuTLS";
#else
    return "Not available";
#endif
}

#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)

bool QUICTransport::initTLS() {
    auto& logger = Logger::instance();
    
    // Initialize GnuTLS
    gnutls_global_init();
    
    // Create credentials
    int ret = gnutls_certificate_allocate_credentials(&tlsCred_);
    if (ret < 0) {
        logger.log(LogLevel::ERROR, "Failed to allocate GnuTLS credentials: " + 
                   std::string(gnutls_strerror(ret)), "QUICTransport");
        return false;
    }
    
    logger.log(LogLevel::INFO, "QUIC TLS initialized with GnuTLS", "QUICTransport");
    return true;
}

// Handshake completed callback
int QUICTransport::handshakeCompleted(ngtcp2_conn* conn, void* user_data) {
    auto* transport = static_cast<QUICTransport*>(user_data);
    if (!transport) return 0;
    
    std::lock_guard<std::mutex> lock(transport->connMutex_);
    for (auto& [peerId, info] : transport->connections_) {
        if (info.conn == conn && info.state == ConnectionState::CONNECTING) {
            info.state = ConnectionState::CONNECTED;
            info.connectedAt = std::chrono::steady_clock::now();
            Logger::instance().log(LogLevel::INFO, "QUIC handshake completed: " + peerId, "QUICTransport");
            transport->emitEvent(TransportEvent::CONNECTED, peerId);
            break;
        }
    }
    
    return 0;
}

void QUICTransport::randCallback(uint8_t* dest, size_t destlen, const ngtcp2_rand_ctx* rand_ctx) {
    (void)rand_ctx;
    gnutls_rnd(GNUTLS_RND_RANDOM, dest, destlen);
}

int QUICTransport::getNewConnectionIdCallback(ngtcp2_conn* conn, ngtcp2_cid* cid, uint8_t* token,
                                               size_t cidlen, void* user_data) {
    (void)conn;
    (void)user_data;
    
    cid->datalen = cidlen;
    gnutls_rnd(GNUTLS_RND_RANDOM, cid->data, cidlen);
    gnutls_rnd(GNUTLS_RND_RANDOM, token, NGTCP2_STATELESS_RESET_TOKENLEN);
    
    return 0;
}

int QUICTransport::removeConnectionIdCallback(ngtcp2_conn* conn, const ngtcp2_cid* cid, void* user_data) {
    (void)conn;
    (void)cid;
    (void)user_data;
    return 0;
}

int QUICTransport::recvStreamDataCallback(ngtcp2_conn* conn, uint32_t flags, int64_t stream_id,
                                          uint64_t offset, const uint8_t* data, size_t datalen,
                                          void* user_data, void* stream_user_data) {
    (void)flags;
    (void)stream_id;
    (void)offset;
    (void)stream_user_data;
    
    auto* transport = static_cast<QUICTransport*>(user_data);
    if (!transport) return 0;
    
    std::string peerId;
    {
        std::lock_guard<std::mutex> lock(transport->connMutex_);
        for (const auto& [id, info] : transport->connections_) {
            if (info.conn == conn) {
                peerId = id;
                break;
            }
        }
    }
    
    if (!peerId.empty() && datalen > 0) {
        std::vector<uint8_t> recvData(data, data + datalen);
        MetricsCollector::instance().incrementBytesReceived(datalen);
        transport->emitEvent(TransportEvent::DATA_RECEIVED, peerId, "", recvData);
    }
    
    return 0;
}

int QUICTransport::streamOpenCallback(ngtcp2_conn* conn, int64_t stream_id, void* user_data) {
    (void)conn;
    (void)stream_id;
    (void)user_data;
    return 0;
}

int QUICTransport::streamCloseCallback(ngtcp2_conn* conn, uint32_t flags, int64_t stream_id,
                                       uint64_t app_error_code, void* user_data, void* stream_user_data) {
    (void)conn;
    (void)flags;
    (void)stream_id;
    (void)app_error_code;
    (void)user_data;
    (void)stream_user_data;
    return 0;
}

int QUICTransport::ackedStreamDataOffsetCallback(ngtcp2_conn* conn, int64_t stream_id,
                                                  uint64_t offset, uint64_t datalen, void* user_data,
                                                  void* stream_user_data) {
    (void)conn;
    (void)stream_id;
    (void)offset;
    (void)datalen;
    (void)user_data;
    (void)stream_user_data;
    return 0;
}

#endif // HAVE_NGTCP2 && HAVE_GNUTLS

bool QUICTransport::startListening(int port) {
    auto& logger = Logger::instance();
    
    if (!isAvailable()) {
        logger.log(LogLevel::WARN, "QUIC not available", "QUICTransport");
        return false;
    }
    
#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)
    serverSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket_ < 0) {
        logger.log(LogLevel::ERROR, "Failed to create UDP socket", "QUICTransport");
        return false;
    }
    
    int flags = fcntl(serverSocket_, F_GETFL, 0);
    fcntl(serverSocket_, F_SETFL, flags | O_NONBLOCK);
    
    // Allow address reuse
    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        logger.log(LogLevel::ERROR, "Failed to bind QUIC socket: " + std::string(strerror(errno)), "QUICTransport");
        close(serverSocket_);
        serverSocket_ = -1;
        return false;
    }
    
    listeningPort_ = port;
    running_ = true;
    
    eventThread_ = std::thread(&QUICTransport::eventLoop, this);
    
    logger.log(LogLevel::INFO, "QUIC listening on UDP port " + std::to_string(port), "QUICTransport");
    return true;
#else
    (void)port;
    return false;
#endif
}

void QUICTransport::stopListening() {
    running_ = false;
    
    if (eventThread_.joinable()) {
        eventThread_.join();
    }
    
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    listeningPort_ = 0;
}

bool QUICTransport::connect(const std::string& address, int port, const std::string& peerId) {
    auto& logger = Logger::instance();
    
    if (!isAvailable()) {
        logger.log(LogLevel::WARN, "QUIC connect failed - not available", "QUICTransport");
        return false;
    }
    
#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)
    std::string targetPeer = peerId.empty() ? (address + ":" + std::to_string(port)) : peerId;
    
    // Resolve address
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    if (getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) {
        logger.log(LogLevel::ERROR, "Failed to resolve: " + address, "QUICTransport");
        return false;
    }
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        freeaddrinfo(res);
        return false;
    }
    
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    if (::connect(sock, res->ai_addr, res->ai_addrlen) < 0 && errno != EINPROGRESS) {
        freeaddrinfo(res);
        close(sock);
        return false;
    }
    
    freeaddrinfo(res);
    
    // Create connection info
    QUICConnectionInfo connInfo;
    connInfo.peerId = targetPeer;
    connInfo.address = address;
    connInfo.port = port;
    connInfo.socket = sock;
    connInfo.state = ConnectionState::CONNECTING;
    connInfo.connectedAt = std::chrono::steady_clock::now();
    
    // Create GnuTLS session
    int ret = gnutls_init(&connInfo.session, GNUTLS_CLIENT | GNUTLS_NONBLOCK);
    if (ret < 0) {
        logger.log(LogLevel::ERROR, "Failed to init GnuTLS session", "QUICTransport");
        close(sock);
        return false;
    }
    
    // Set up TLS 1.3 for QUIC
    ret = gnutls_priority_set_direct(connInfo.session, 
        "NORMAL:-VERS-ALL:+VERS-TLS1.3:-KX-ALL:+ECDHE-ECDSA:+ECDHE-RSA:+DHE-RSA", nullptr);
    if (ret < 0) {
        logger.log(LogLevel::ERROR, "Failed to set GnuTLS priority", "QUICTransport");
        gnutls_deinit(connInfo.session);
        close(sock);
        return false;
    }
    
    gnutls_certificate_allocate_credentials(&connInfo.cred);
    gnutls_credentials_set(connInfo.session, GNUTLS_CRD_CERTIFICATE, connInfo.cred);
    
    // Set server name for SNI
    gnutls_server_name_set(connInfo.session, GNUTLS_NAME_DNS, address.c_str(), address.length());
    
    // Create ngtcp2 connection IDs
    ngtcp2_cid scid, dcid;
    scid.datalen = 16;
    dcid.datalen = 16;
    gnutls_rnd(GNUTLS_RND_RANDOM, scid.data, 16);
    gnutls_rnd(GNUTLS_RND_RANDOM, dcid.data, 16);
    
    // Setup callbacks with GnuTLS crypto
    ngtcp2_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    
    // Use ngtcp2_crypto_gnutls callbacks
    callbacks.client_initial = ngtcp2_crypto_client_initial_cb;
    callbacks.recv_crypto_data = ngtcp2_crypto_recv_crypto_data_cb;
    callbacks.encrypt = ngtcp2_crypto_encrypt_cb;
    callbacks.decrypt = ngtcp2_crypto_decrypt_cb;
    callbacks.hp_mask = ngtcp2_crypto_hp_mask_cb;
    callbacks.recv_retry = ngtcp2_crypto_recv_retry_cb;
    callbacks.update_key = ngtcp2_crypto_update_key_cb;
    callbacks.delete_crypto_aead_ctx = ngtcp2_crypto_delete_crypto_aead_ctx_cb;
    callbacks.delete_crypto_cipher_ctx = ngtcp2_crypto_delete_crypto_cipher_ctx_cb;
    callbacks.get_path_challenge_data = ngtcp2_crypto_get_path_challenge_data_cb;
    callbacks.version_negotiation = ngtcp2_crypto_version_negotiation_cb;
    
    // Application callbacks
    callbacks.recv_stream_data = recvStreamDataCallback;
    callbacks.acked_stream_data_offset = ackedStreamDataOffsetCallback;
    callbacks.stream_open = streamOpenCallback;
    callbacks.stream_close = streamCloseCallback;
    callbacks.rand = randCallback;
    callbacks.get_new_connection_id = getNewConnectionIdCallback;
    callbacks.remove_connection_id = removeConnectionIdCallback;
    callbacks.handshake_completed = handshakeCompleted;
    
    // Setup settings
    ngtcp2_settings settings;
    ngtcp2_settings_default(&settings);
    settings.initial_ts = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Setup transport params
    ngtcp2_transport_params params;
    ngtcp2_transport_params_default(&params);
    params.initial_max_streams_bidi = 100;
    params.initial_max_streams_uni = 100;
    params.initial_max_data = 1024 * 1024;
    params.initial_max_stream_data_bidi_local = 256 * 1024;
    params.initial_max_stream_data_bidi_remote = 256 * 1024;
    
    // Get local address
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    getsockname(sock, (struct sockaddr*)&local_addr, &local_len);
    
    ngtcp2_path path;
    path.local.addrlen = local_len;
    path.local.addr = (struct sockaddr*)&local_addr;
    
    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &remote_addr.sin_addr);
    path.remote.addrlen = sizeof(remote_addr);
    path.remote.addr = (struct sockaddr*)&remote_addr;
    
    ret = ngtcp2_conn_client_new(&connInfo.conn, &dcid, &scid, &path,
                                  NGTCP2_PROTO_VER_V1, &callbacks, &settings,
                                  &params, nullptr, this);
    if (ret != 0) {
        logger.log(LogLevel::ERROR, "Failed to create QUIC connection: " + 
                   std::string(ngtcp2_strerror(ret)), "QUICTransport");
        gnutls_deinit(connInfo.session);
        gnutls_certificate_free_credentials(connInfo.cred);
        close(sock);
        return false;
    }
    
    // Configure GnuTLS for ngtcp2
    ngtcp2_crypto_gnutls_configure_client_session(connInfo.session);
    ngtcp2_conn_set_tls_native_handle(connInfo.conn, connInfo.session);
    
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        connections_[targetPeer] = std::move(connInfo);
    }
    
    logger.log(LogLevel::INFO, "QUIC connecting to " + address + ":" + std::to_string(port), "QUICTransport");
    
    // Send initial packet
    {
        std::lock_guard<std::mutex> lock(connMutex_);
        sendPacket(connections_[targetPeer]);
    }
    
    return true;
#else
    (void)address;
    (void)port;
    (void)peerId;
    return false;
#endif
}

void QUICTransport::disconnect(const std::string& peerId) {
    auto& logger = Logger::instance();
    
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it == connections_.end()) return;
    
#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)
    if (it->second.conn) {
        ngtcp2_conn_del(it->second.conn);
    }
    if (it->second.session) {
        gnutls_deinit(it->second.session);
    }
    if (it->second.cred) {
        gnutls_certificate_free_credentials(it->second.cred);
    }
#endif
    if (it->second.socket >= 0) {
        close(it->second.socket);
    }
    
    connections_.erase(it);
    
    logger.log(LogLevel::INFO, "QUIC disconnected: " + peerId, "QUICTransport");
    emitEvent(TransportEvent::DISCONNECTED, peerId);
}

bool QUICTransport::send(const std::string& peerId, const std::vector<uint8_t>& data) {
    if (!isAvailable()) return false;
    
#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it == connections_.end() || it->second.state != ConnectionState::CONNECTED) {
        return false;
    }
    
    // Open a stream and send data
    int64_t stream_id;
    int rv = ngtcp2_conn_open_bidi_stream(it->second.conn, &stream_id, nullptr);
    if (rv != 0) {
        return false;
    }
    
    ngtcp2_vec datav;
    datav.base = const_cast<uint8_t*>(data.data());
    datav.len = data.size();
    
    uint8_t buf[1400];
    ngtcp2_pkt_info pi;
    
    ngtcp2_ssize nwrite = ngtcp2_conn_writev_stream(it->second.conn, nullptr, &pi, buf, sizeof(buf),
                                                    nullptr, NGTCP2_WRITE_STREAM_FLAG_FIN,
                                                    stream_id, &datav, 1,
                                                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                        std::chrono::steady_clock::now().time_since_epoch()).count());
    
    if (nwrite > 0) {
        ::send(it->second.socket, buf, nwrite, 0);
        MetricsCollector::instance().incrementBytesSent(data.size());
        return true;
    }
    
    return false;
#else
    (void)peerId;
    (void)data;
    return false;
#endif
}

bool QUICTransport::isConnected(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    return it != connections_.end() && it->second.state == ConnectionState::CONNECTED;
}

ConnectionState QUICTransport::getConnectionState(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    return it != connections_.end() ? it->second.state : ConnectionState::DISCONNECTED;
}

ConnectionQuality QUICTransport::getConnectionQuality(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it == connections_.end()) return ConnectionQuality{};
    
#ifdef HAVE_NGTCP2
    if (it->second.conn) {
        ngtcp2_conn_info cinfo;
        ngtcp2_conn_get_conn_info(it->second.conn, &cinfo);
        
        ConnectionQuality quality;
        quality.rttMs = static_cast<int>(cinfo.smoothed_rtt / 1000000);
        quality.jitterMs = 0;
        return quality;
    }
#endif
    
    return it->second.quality;
}

std::vector<std::string> QUICTransport::getConnectedPeers() const {
    std::lock_guard<std::mutex> lock(connMutex_);
    std::vector<std::string> peers;
    for (const auto& [peerId, info] : connections_) {
        if (info.state == ConnectionState::CONNECTED) {
            peers.push_back(peerId);
        }
    }
    return peers;
}

void QUICTransport::setEventCallback(TransportEventCallback callback) {
    eventCallback_ = std::move(callback);
}

int QUICTransport::measureRTT(const std::string& peerId) {
#ifdef HAVE_NGTCP2
    std::lock_guard<std::mutex> lock(connMutex_);
    auto it = connections_.find(peerId);
    if (it != connections_.end() && it->second.conn) {
        ngtcp2_conn_info cinfo;
        ngtcp2_conn_get_conn_info(it->second.conn, &cinfo);
        return static_cast<int>(cinfo.smoothed_rtt / 1000000);
    }
#else
    (void)peerId;
#endif
    return -1;
}

void QUICTransport::shutdown() {
    stopListening();
    
    std::lock_guard<std::mutex> lock(connMutex_);
    for (auto& [peerId, info] : connections_) {
#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)
        if (info.conn) {
            ngtcp2_conn_del(info.conn);
            info.conn = nullptr;
        }
        if (info.session) {
            gnutls_deinit(info.session);
            info.session = nullptr;
        }
        if (info.cred) {
            gnutls_certificate_free_credentials(info.cred);
            info.cred = nullptr;
        }
#endif
        if (info.socket >= 0) {
            close(info.socket);
            info.socket = -1;
        }
    }
    connections_.clear();
}

void QUICTransport::eventLoop() {
#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)
    uint8_t buf[65535];
    
    while (running_) {
        struct pollfd pfd;
        pfd.fd = serverSocket_;
        pfd.events = POLLIN;
        
        int ret = poll(&pfd, 1, 100);
        if (ret <= 0) {
            // Process connections even without incoming data
            std::lock_guard<std::mutex> lock(connMutex_);
            for (auto& [peerId, info] : connections_) {
                processConnection(info);
            }
            continue;
        }
        
        if (pfd.revents & POLLIN) {
            struct sockaddr_storage addr;
            socklen_t addrlen = sizeof(addr);
            
            ssize_t nread = recvfrom(serverSocket_, buf, sizeof(buf), 0,
                                     (struct sockaddr*)&addr, &addrlen);
            if (nread > 0) {
                handlePacket(buf, nread, (struct sockaddr*)&addr, addrlen);
            }
        }
        
        // Process all connections
        {
            std::lock_guard<std::mutex> lock(connMutex_);
            for (auto& [peerId, info] : connections_) {
                processConnection(info);
            }
        }
    }
#endif
}

void QUICTransport::processConnection(QUICConnectionInfo& conn) {
#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)
    if (!conn.conn) return;
    
    // Check if handshake completed
    if (conn.state == ConnectionState::CONNECTING) {
        if (ngtcp2_conn_get_handshake_completed(conn.conn)) {
            conn.state = ConnectionState::CONNECTED;
            conn.connectedAt = std::chrono::steady_clock::now();
            emitEvent(TransportEvent::CONNECTED, conn.peerId);
            Logger::instance().log(LogLevel::INFO, "QUIC connected: " + conn.peerId, "QUICTransport");
        }
    }
    
    // Send pending packets
    sendPacket(conn);
#else
    (void)conn;
#endif
}

#if defined(HAVE_NGTCP2) && defined(HAVE_GNUTLS)
void QUICTransport::handlePacket(const uint8_t* data, size_t len, const sockaddr* addr, socklen_t addrlen) {
    std::lock_guard<std::mutex> lock(connMutex_);
    
    for (auto& [peerId, info] : connections_) {
        if (!info.conn) continue;
        
        ngtcp2_path path;
        struct sockaddr_in local_addr;
        socklen_t local_len = sizeof(local_addr);
        getsockname(info.socket, (struct sockaddr*)&local_addr, &local_len);
        
        path.local.addr = (struct sockaddr*)&local_addr;
        path.local.addrlen = local_len;
        path.remote.addr = const_cast<sockaddr*>(addr);
        path.remote.addrlen = addrlen;
        
        ngtcp2_pkt_info pi;
        memset(&pi, 0, sizeof(pi));
        
        int rv = ngtcp2_conn_read_pkt(info.conn, &path, &pi, data, len,
                                       std::chrono::duration_cast<std::chrono::nanoseconds>(
                                           std::chrono::steady_clock::now().time_since_epoch()).count());
        if (rv == 0) {
            info.lastActivity = std::chrono::steady_clock::now();
            return;
        }
    }
}

int QUICTransport::sendPacket(QUICConnectionInfo& conn) {
    if (!conn.conn || conn.socket < 0) return -1;
    
    uint8_t buf[1400];
    ngtcp2_pkt_info pi;
    ngtcp2_path_storage ps;
    ngtcp2_path_storage_zero(&ps);
    
    for (;;) {
        ngtcp2_ssize nwrite = ngtcp2_conn_write_pkt(conn.conn, &ps.path, &pi, buf, sizeof(buf),
                                                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                                                        std::chrono::steady_clock::now().time_since_epoch()).count());
        if (nwrite <= 0) break;
        
        ::send(conn.socket, buf, nwrite, 0);
    }
    
    return 0;
}

int QUICTransport::writeStreams(QUICConnectionInfo& conn) {
    (void)conn;
    return 0;
}
#endif

void QUICTransport::emitEvent(TransportEvent event, const std::string& peerId,
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
                eventBus_->publish("QUIC_PEER_CONNECTED", peerId);
                break;
            case TransportEvent::DISCONNECTED:
                eventBus_->publish("QUIC_PEER_DISCONNECTED", peerId);
                break;
            case TransportEvent::DATA_RECEIVED:
                eventBus_->publish("QUIC_DATA_RECEIVED", std::make_pair(peerId, data));
                break;
            default:
                break;
        }
    }
}

} // namespace NetFalcon
} // namespace SentinelFS
