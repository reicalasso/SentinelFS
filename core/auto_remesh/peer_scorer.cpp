#include "auto_remesh/peer_scorer.h"
#include <cmath>
#include <algorithm>

namespace sentinel {

PeerScorer::PeerScorer(const PeerScoringConfig& config) 
    : config_(config) {
    if (!config_.is_valid()) {
        // Use default config if invalid
        config_ = PeerScoringConfig();
    }
}

double PeerScorer::calculate_score(const NetworkMetrics& metrics) const {
    // If no data yet, return neutral score
    if (metrics.packets_sent == 0 || metrics.avg_rtt_ms == 0.0) {
        return 50.0;
    }
    
    // Calculate component scores
    double rtt_score = score_rtt(metrics.avg_rtt_ms);
    double jitter_score = score_jitter(metrics.jitter_ms);
    double loss_score = score_loss(metrics.loss_rate);
    
    // Weighted composite score
    double composite_score = 
        rtt_score * config_.rtt_weight +
        jitter_score * config_.jitter_weight +
        loss_score * config_.loss_weight;
    
    // Add stability bonus
    double stability_bonus = calculate_stability_bonus(metrics);
    composite_score += stability_bonus;
    
    // Clamp to 0-100 range
    return std::max(0.0, std::min(100.0, composite_score));
}

double PeerScorer::score_rtt(double avg_rtt_ms) const {
    if (avg_rtt_ms <= 0.0) {
        return 50.0;  // Neutral score for invalid data
    }
    
    // Lower RTT is better
    // Use exponential decay: score = 100 * e^(-k * (rtt / ref))
    // Where k controls how fast the score drops
    return normalize_exponential(avg_rtt_ms, config_.reference_rtt_ms, 2.0);
}

double PeerScorer::score_jitter(double jitter_ms) const {
    if (jitter_ms < 0.0) {
        return 50.0;  // Neutral score for invalid data
    }
    
    // Lower jitter is better
    return normalize_exponential(jitter_ms, config_.reference_jitter_ms, 2.5);
}

double PeerScorer::score_loss(double loss_rate) const {
    if (loss_rate < 0.0 || loss_rate > 1.0) {
        return 50.0;  // Neutral score for invalid data
    }
    
    // Lower loss rate is better
    // Convert to percentage for normalization
    double loss_percentage = loss_rate * 100.0;
    double reference_percentage = config_.reference_loss_rate * 100.0;
    
    return normalize_exponential(loss_percentage, reference_percentage, 3.0);
}

double PeerScorer::calculate_stability_bonus(const NetworkMetrics& metrics) const {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        now - metrics.first_seen
    ).count();
    
    // Bonus for stable, long-lived connections
    if (uptime >= static_cast<int64_t>(config_.stable_connection_threshold_sec)) {
        // Scale bonus based on number of resets (penalize reconnections)
        double reset_penalty = std::max(0.0, 1.0 - (metrics.connection_resets * 0.2));
        return 10.0 * config_.stability_bonus_weight * reset_penalty;
    }
    
    // Partial bonus for shorter connections
    double uptime_ratio = static_cast<double>(uptime) / 
                         config_.stable_connection_threshold_sec;
    double reset_penalty = std::max(0.0, 1.0 - (metrics.connection_resets * 0.2));
    
    return 10.0 * config_.stability_bonus_weight * uptime_ratio * reset_penalty;
}

void PeerScorer::update_config(const PeerScoringConfig& config) {
    if (config.is_valid()) {
        config_ = config;
    }
}

double PeerScorer::normalize_exponential(double value, double reference, double decay_rate) const {
    if (value <= 0.0 || reference <= 0.0) {
        return 50.0;
    }
    
    // Exponential decay function: 100 * e^(-k * (value / reference))
    // When value == reference, score ≈ 13.5 * e^(-2) ≈ 18
    // When value == reference/2, score ≈ 36
    // When value << reference, score approaches 100
    
    double ratio = value / reference;
    double score = 100.0 * std::exp(-decay_rate * ratio);
    
    return std::max(0.0, std::min(100.0, score));
}

} // namespace sentinel
