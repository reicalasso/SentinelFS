#ifndef SENTINEL_PEER_SCORER_H
#define SENTINEL_PEER_SCORER_H

#include "auto_remesh/network_metrics.h"
#include <string>

namespace sentinel {

/**
 * @brief Configuration for peer scoring weights
 */
struct PeerScoringConfig {
    // Weights for different metrics (should sum to 1.0)
    double rtt_weight = 0.4;           // Weight for latency
    double jitter_weight = 0.3;        // Weight for jitter
    double loss_weight = 0.3;          // Weight for packet loss
    
    // Reference values for normalization
    double reference_rtt_ms = 100.0;    // "Good" RTT baseline
    double reference_jitter_ms = 20.0;  // "Good" jitter baseline
    double reference_loss_rate = 0.02;  // "Good" loss rate (2%)
    
    // Stability bonus
    double stability_bonus_weight = 0.1;  // Bonus for stable connections
    uint64_t stable_connection_threshold_sec = 300;  // 5 minutes
    
    // Validate configuration
    bool is_valid() const {
        return (rtt_weight + jitter_weight + loss_weight) > 0.99 &&
               (rtt_weight + jitter_weight + loss_weight) < 1.01;
    }
};

/**
 * @brief Peer scoring algorithm for auto-remesh engine
 * 
 * Calculates a composite quality score (0-100) based on network metrics
 */
class PeerScorer {
public:
    explicit PeerScorer(const PeerScoringConfig& config = PeerScoringConfig());
    
    /**
     * @brief Calculate quality score for a peer based on metrics
     * @param metrics Network metrics for the peer
     * @return Quality score (0-100, higher is better)
     */
    double calculate_score(const NetworkMetrics& metrics) const;
    
    /**
     * @brief Calculate RTT component of the score
     * @param avg_rtt_ms Average round-trip time
     * @return Normalized score (0-100)
     */
    double score_rtt(double avg_rtt_ms) const;
    
    /**
     * @brief Calculate jitter component of the score
     * @param jitter_ms RTT variance
     * @return Normalized score (0-100)
     */
    double score_jitter(double jitter_ms) const;
    
    /**
     * @brief Calculate packet loss component of the score
     * @param loss_rate Packet loss rate (0.0 to 1.0)
     * @return Normalized score (0-100)
     */
    double score_loss(double loss_rate) const;
    
    /**
     * @brief Calculate stability bonus
     * @param metrics Network metrics containing connection uptime
     * @return Bonus score (0-10)
     */
    double calculate_stability_bonus(const NetworkMetrics& metrics) const;
    
    /**
     * @brief Update scoring configuration
     * @param config New configuration
     */
    void update_config(const PeerScoringConfig& config);
    
    /**
     * @brief Get current configuration
     * @return Current scoring configuration
     */
    const PeerScoringConfig& get_config() const { return config_; }
    
private:
    PeerScoringConfig config_;
    
    /**
     * @brief Normalize a value using exponential decay
     * @param value The value to normalize
     * @param reference Reference "good" value
     * @param decay_rate How quickly score drops (higher = faster decay)
     * @return Score 0-100
     */
    double normalize_exponential(double value, double reference, double decay_rate = 2.0) const;
};

} // namespace sentinel

#endif // SENTINEL_PEER_SCORER_H
