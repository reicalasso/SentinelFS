#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <memory>
#include "../auto_remesh/network_metrics.h"

namespace sfs {

struct PeerInfo {
    std::string peer_id;
    std::string address;
    uint16_t port;
    std::chrono::steady_clock::time_point last_seen;
    bool is_connected;
    int64_t latency_ms;  // RTT in milliseconds (legacy, use metrics.avg_rtt_ms)
    
    // Network quality metrics (Sprint 6)
    sentinel::NetworkMetrics metrics;
    
    PeerInfo() : port(0), is_connected(false), latency_ms(-1) {
        last_seen = std::chrono::steady_clock::now();
        metrics.first_seen = last_seen;
    }
    
    PeerInfo(const std::string& id, const std::string& addr, uint16_t p)
        : peer_id(id), address(addr), port(p), is_connected(false), latency_ms(-1) {
        last_seen = std::chrono::steady_clock::now();
        metrics.first_seen = last_seen;
    }
};

class PeerRegistry {
public:
    PeerRegistry();
    ~PeerRegistry();
    
    // Peer management
    void add_peer(const PeerInfo& peer);
    void remove_peer(const std::string& peer_id);
    void update_last_seen(const std::string& peer_id);
    void set_connected(const std::string& peer_id, bool connected);
    void update_latency(const std::string& peer_id, int64_t latency_ms);
    
    // Network metrics management (Sprint 6)
    void update_rtt(const std::string& peer_id, double rtt_ms);
    void record_packet_sent(const std::string& peer_id);
    void record_packet_lost(const std::string& peer_id);
    void update_bandwidth(const std::string& peer_id, uint64_t bytes, double duration_ms);
    void update_quality_score(const std::string& peer_id, double score);
    
    // Queries
    bool has_peer(const std::string& peer_id) const;
    PeerInfo get_peer(const std::string& peer_id) const;
    std::vector<PeerInfo> get_all_peers() const;
    std::vector<PeerInfo> get_connected_peers() const;
    
    // Quality-based queries (Sprint 6)
    std::vector<PeerInfo> get_peers_by_score(size_t limit = 0) const;  // Sorted by quality score
    std::vector<PeerInfo> get_healthy_peers() const;  // Only peers above quality threshold
    PeerInfo get_best_peer() const;  // Highest quality peer
    
    // Alive check - remove peers not seen for timeout_seconds
    std::vector<std::string> check_timeouts(int timeout_seconds = 30);
    
    // Stats
    size_t peer_count() const;
    size_t connected_count() const;
    double get_average_quality_score() const;
    
private:
    mutable std::mutex mutex_;
    std::map<std::string, PeerInfo> peers_;
};

} // namespace sfs
