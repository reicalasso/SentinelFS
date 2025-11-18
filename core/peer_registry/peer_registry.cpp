#include "peer_registry.h"
#include <algorithm>

namespace sfs {

PeerRegistry::PeerRegistry() = default;
PeerRegistry::~PeerRegistry() = default;

void PeerRegistry::add_peer(const PeerInfo& peer) {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_[peer.peer_id] = peer;
    peers_[peer.peer_id].last_seen = std::chrono::steady_clock::now();
}

void PeerRegistry::remove_peer(const std::string& peer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    peers_.erase(peer_id);
}

void PeerRegistry::update_last_seen(const std::string& peer_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(peer_id);
    if (it != peers_.end()) {
        it->second.last_seen = std::chrono::steady_clock::now();
    }
}

void PeerRegistry::set_connected(const std::string& peer_id, bool connected) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(peer_id);
    if (it != peers_.end()) {
        it->second.is_connected = connected;
        it->second.last_seen = std::chrono::steady_clock::now();
    }
}

void PeerRegistry::update_latency(const std::string& peer_id, int64_t latency_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(peer_id);
    if (it != peers_.end()) {
        it->second.latency_ms = latency_ms;
    }
}

bool PeerRegistry::has_peer(const std::string& peer_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peers_.find(peer_id) != peers_.end();
}

PeerInfo PeerRegistry::get_peer(const std::string& peer_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = peers_.find(peer_id);
    if (it != peers_.end()) {
        return it->second;
    }
    return PeerInfo();
}

std::vector<PeerInfo> PeerRegistry::get_all_peers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PeerInfo> result;
    result.reserve(peers_.size());
    for (const auto& pair : peers_) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<PeerInfo> PeerRegistry::get_connected_peers() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PeerInfo> result;
    for (const auto& pair : peers_) {
        if (pair.second.is_connected) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<std::string> PeerRegistry::check_timeouts(int timeout_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> timed_out;
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = peers_.begin(); it != peers_.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_seen).count();
        
        if (elapsed > timeout_seconds) {
            timed_out.push_back(it->first);
            it = peers_.erase(it);
        } else {
            ++it;
        }
    }
    
    return timed_out;
}

size_t PeerRegistry::peer_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return peers_.size();
}

size_t PeerRegistry::connected_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& pair : peers_) {
        if (pair.second.is_connected) {
            ++count;
        }
    }
    return count;
}

} // namespace sfs
