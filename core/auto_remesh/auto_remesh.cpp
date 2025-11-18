#include "auto_remesh/auto_remesh.h"
#include <algorithm>
#include <iostream>

namespace sentinel {

AutoRemesh::AutoRemesh(
    std::shared_ptr<sfs::PeerRegistry> registry,
    const AutoRemeshConfig& config
)
    : registry_(registry)
    , config_(config)
    , scorer_(config.scoring_config)
    , running_(false)
{
}

AutoRemesh::~AutoRemesh() {
    stop();
}

void AutoRemesh::start() {
    if (running_.exchange(true)) {
        return;  // Already running
    }
    
    if (!config_.enabled) {
        running_ = false;
        return;
    }
    
    evaluation_thread_ = std::thread(&AutoRemesh::evaluation_loop, this);
}

void AutoRemesh::stop() {
    if (!running_.exchange(false)) {
        return;  // Not running
    }
    
    if (evaluation_thread_.joinable()) {
        evaluation_thread_.join();
    }
}

void AutoRemesh::evaluation_loop() {
    while (running_) {
        // Wait for evaluation interval
        for (uint32_t i = 0; i < config_.evaluation_interval_sec && running_; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (!running_) break;
        
        // Perform evaluation
        try {
            evaluate_topology();
        } catch (const std::exception& e) {
            std::cerr << "AutoRemesh evaluation error: " << e.what() << std::endl;
        }
    }
}

size_t AutoRemesh::evaluate_topology() {
    size_t changes = 0;
    
    // Update all peer scores
    update_all_scores();
    
    // Get current connected peers
    auto connected = registry_->get_connected_peers();
    
    // Identify poor performers
    auto poor_performers = identify_poor_performers();
    
    // Drop poor performers if we have enough peers
    for (const auto& peer_id : poor_performers) {
        if (connected.size() - changes <= config_.min_peers) {
            break;  // Don't drop below minimum
        }
        
        auto peer = registry_->get_peer(peer_id);
        
        TopologyChangeEvent event;
        event.type = TopologyChangeEvent::Type::PEER_REMOVED;
        event.peer_id = peer_id;
        event.old_score = peer.metrics.quality_score;
        event.reason = "Poor performance (score: " + 
                      std::to_string(peer.metrics.quality_score) + ")";
        
        // In a real system, you would actually disconnect here
        // For now, just mark as disconnected
        registry_->set_connected(peer_id, false);
        
        notify_topology_change(event);
        changes++;
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.peers_dropped++;
        }
    }
    
    // Update statistics
    update_stats();
    
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.evaluations_performed++;
        if (changes > 0) {
            stats_.topology_optimizations++;
        }
    }
    
    return changes;
}

void AutoRemesh::update_all_scores() {
    auto all_peers = registry_->get_all_peers();
    
    for (const auto& peer : all_peers) {
        double score = scorer_.calculate_score(peer.metrics);
        registry_->update_quality_score(peer.peer_id, score);
    }
}

std::vector<std::string> AutoRemesh::identify_poor_performers() {
    std::lock_guard<std::mutex> lock(evals_mutex_);
    std::vector<std::string> poor_performers;
    
    auto connected = registry_->get_connected_peers();
    
    for (const auto& peer : connected) {
        double score = peer.metrics.quality_score;
        
        // Update or create evaluation record
        auto& eval = peer_evals_[peer.peer_id];
        eval.peer_id = peer.peer_id;
        
        // Check if peer is below threshold
        if (score < config_.min_score_threshold) {
            eval.consecutive_bad_evals++;
            
            // Only mark for removal after multiple bad evaluations
            if (eval.consecutive_bad_evals >= static_cast<int>(config_.min_evaluation_count)) {
                poor_performers.push_back(peer.peer_id);
            }
        } else {
            // Reset counter if peer recovers
            eval.consecutive_bad_evals = 0;
        }
        
        eval.last_score = score;
    }
    
    // Clean up evaluations for disconnected peers
    for (auto it = peer_evals_.begin(); it != peer_evals_.end();) {
        bool found = false;
        for (const auto& peer : connected) {
            if (peer.peer_id == it->first) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            it = peer_evals_.erase(it);
        } else {
            ++it;
        }
    }
    
    return poor_performers;
}

bool AutoRemesh::should_replace_peer(const std::string& peer_id) {
    auto peer = registry_->get_peer(peer_id);
    
    // Check if peer is below threshold
    if (peer.metrics.quality_score < config_.min_score_threshold) {
        return true;
    }
    
    // Check hysteresis
    std::lock_guard<std::mutex> lock(evals_mutex_);
    auto it = peer_evals_.find(peer_id);
    if (it != peer_evals_.end()) {
        double score_change = std::abs(peer.metrics.quality_score - it->second.last_score);
        if (score_change < config_.hysteresis_margin) {
            return false;  // Not enough change to warrant action
        }
    }
    
    return false;
}

void AutoRemesh::on_topology_change(TopologyChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    callbacks_.push_back(callback);
}

void AutoRemesh::notify_topology_change(const TopologyChangeEvent& event) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    for (const auto& callback : callbacks_) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            std::cerr << "AutoRemesh callback error: " << e.what() << std::endl;
        }
    }
}

void AutoRemesh::update_config(const AutoRemeshConfig& config) {
    config_ = config;
    scorer_.update_config(config.scoring_config);
}

void AutoRemesh::update_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.avg_peer_score = registry_->get_average_quality_score();
}

AutoRemesh::Stats AutoRemesh::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

} // namespace sentinel
