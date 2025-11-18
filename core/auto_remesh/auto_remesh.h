#ifndef SENTINEL_AUTO_REMESH_H
#define SENTINEL_AUTO_REMESH_H

#include "auto_remesh/peer_scorer.h"
#include "auto_remesh/network_metrics.h"
#include "peer_registry/peer_registry.h"
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

namespace sentinel {

/**
 * @brief Configuration for auto-remesh engine
 */
struct AutoRemeshConfig {
    bool enabled = true;
    
    // Evaluation timing
    uint32_t evaluation_interval_sec = 30;  // How often to evaluate topology
    
    // Peer limits
    size_t max_peers = 10;                  // Maximum connected peers
    size_t min_peers = 2;                   // Minimum peers to maintain
    size_t optimal_peers = 5;               // Target number of peers
    
    // Quality thresholds
    double min_score_threshold = 40.0;      // Drop peers below this score
    double replacement_threshold = 60.0;     // Consider replacing if new peer scores above this
    
    // Hysteresis to prevent flapping
    double hysteresis_margin = 10.0;        // Score must change by this much to trigger action
    uint32_t min_evaluation_count = 3;      // Require this many bad evaluations before dropping
    
    // Peer scoring configuration
    PeerScoringConfig scoring_config;
};

/**
 * @brief Callback for topology change events
 */
struct TopologyChangeEvent {
    enum class Type {
        PEER_ADDED,
        PEER_REMOVED,
        PEER_REPLACED,
        PEER_DEGRADED,
        TOPOLOGY_OPTIMIZED
    };
    
    Type type;
    std::string peer_id;
    std::string reason;
    double old_score = 0.0;
    double new_score = 0.0;
};

using TopologyChangeCallback = std::function<void(const TopologyChangeEvent&)>;

/**
 * @brief Auto-remesh engine for adaptive P2P topology
 * 
 * Continuously monitors peer connection quality and dynamically
 * optimizes the network topology by replacing poor-performing peers
 * with better alternatives.
 */
class AutoRemesh {
public:
    explicit AutoRemesh(
        std::shared_ptr<sfs::PeerRegistry> registry,
        const AutoRemeshConfig& config = AutoRemeshConfig()
    );
    
    ~AutoRemesh();
    
    /**
     * @brief Start the auto-remesh engine
     */
    void start();
    
    /**
     * @brief Stop the auto-remesh engine
     */
    void stop();
    
    /**
     * @brief Check if engine is running
     */
    bool is_running() const { return running_; }
    
    /**
     * @brief Manually trigger a topology evaluation
     * @return Number of topology changes made
     */
    size_t evaluate_topology();
    
    /**
     * @brief Update peer scores based on current metrics
     */
    void update_all_scores();
    
    /**
     * @brief Register callback for topology changes
     */
    void on_topology_change(TopologyChangeCallback callback);
    
    /**
     * @brief Update configuration
     */
    void update_config(const AutoRemeshConfig& config);
    
    /**
     * @brief Get current configuration
     */
    const AutoRemeshConfig& get_config() const { return config_; }
    
    /**
     * @brief Get peer scorer
     */
    const PeerScorer& get_scorer() const { return scorer_; }
    
    /**
     * @brief Get statistics
     */
    struct Stats {
        size_t evaluations_performed = 0;
        size_t peers_replaced = 0;
        size_t peers_dropped = 0;
        size_t topology_optimizations = 0;
        double avg_peer_score = 0.0;
    };
    
    Stats get_stats() const;
    
private:
    std::shared_ptr<sfs::PeerRegistry> registry_;
    AutoRemeshConfig config_;
    PeerScorer scorer_;
    
    // Threading
    std::atomic<bool> running_;
    std::thread evaluation_thread_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    Stats stats_;
    
    // Callbacks
    std::vector<TopologyChangeCallback> callbacks_;
    mutable std::mutex callbacks_mutex_;
    
    // Track poor performers to avoid flapping
    struct PeerEvaluation {
        std::string peer_id;
        int consecutive_bad_evals = 0;
        double last_score = 0.0;
    };
    std::map<std::string, PeerEvaluation> peer_evals_;
    mutable std::mutex evals_mutex_;
    
    /**
     * @brief Main evaluation loop (runs in thread)
     */
    void evaluation_loop();
    
    /**
     * @brief Identify and drop poor-performing peers
     * @return List of dropped peer IDs
     */
    std::vector<std::string> identify_poor_performers();
    
    /**
     * @brief Check if we should replace a peer
     * @param peer_id Peer to evaluate
     * @return true if peer should be replaced
     */
    bool should_replace_peer(const std::string& peer_id);
    
    /**
     * @brief Notify registered callbacks of topology change
     */
    void notify_topology_change(const TopologyChangeEvent& event);
    
    /**
     * @brief Update statistics
     */
    void update_stats();
};

} // namespace sentinel

#endif // SENTINEL_AUTO_REMESH_H
