#ifndef SENTINEL_NETWORK_METRICS_H
#define SENTINEL_NETWORK_METRICS_H

#include <chrono>
#include <vector>
#include <cstdint>
#include <string>

namespace sentinel {

/**
 * @brief Network quality metrics for a peer connection
 * 
 * Tracks RTT, jitter, packet loss, and other connection quality indicators
 */
struct NetworkMetrics {
    // Round-Trip Time (latency)
    double current_rtt_ms = 0.0;      // Current RTT measurement
    double avg_rtt_ms = 0.0;          // Moving average RTT
    double min_rtt_ms = 999999.0;     // Minimum observed RTT
    double max_rtt_ms = 0.0;          // Maximum observed RTT
    
    // Jitter (RTT variance)
    double jitter_ms = 0.0;           // Standard deviation of RTT
    
    // Packet Loss
    uint64_t packets_sent = 0;        // Total packets sent
    uint64_t packets_lost = 0;        // Packets that timed out
    double loss_rate = 0.0;           // Loss rate (0.0 to 1.0)
    
    // Bandwidth estimation
    double estimated_bandwidth_mbps = 0.0;  // Estimated bandwidth in Mbps
    
    // Connection stability
    std::chrono::steady_clock::time_point first_seen;
    std::chrono::steady_clock::time_point last_successful_ping;
    uint32_t connection_resets = 0;   // Number of reconnections
    
    // Measurement history (for calculating jitter)
    std::vector<double> rtt_history;  // Recent RTT samples
    static constexpr size_t MAX_HISTORY = 20;
    
    // Quality score (0-100, higher is better)
    double quality_score = 0.0;
    
    /**
     * @brief Update metrics with a new RTT measurement
     * @param rtt_ms Round-trip time in milliseconds
     */
    void update_rtt(double rtt_ms);
    
    /**
     * @brief Record a successful packet transmission
     */
    void record_packet_sent();
    
    /**
     * @brief Record a packet loss (timeout)
     */
    void record_packet_lost();
    
    /**
     * @brief Update bandwidth estimate based on transfer
     * @param bytes_transferred Number of bytes transferred
     * @param duration_ms Transfer duration in milliseconds
     */
    void update_bandwidth(uint64_t bytes_transferred, double duration_ms);
    
    /**
     * @brief Calculate jitter from RTT history
     * @return Jitter value in milliseconds
     */
    double calculate_jitter() const;
    
    /**
     * @brief Reset all metrics
     */
    void reset();
    
    /**
     * @brief Check if metrics are healthy (good connection)
     * @return true if connection quality is acceptable
     */
    bool is_healthy() const;
    
    /**
     * @brief Get human-readable summary of metrics
     * @return String representation of metrics
     */
    std::string to_string() const;
};

/**
 * @brief Configuration for network quality thresholds
 */
struct NetworkQualityThresholds {
    double max_acceptable_rtt_ms = 500.0;
    double max_acceptable_jitter_ms = 100.0;
    double max_acceptable_loss_rate = 0.1;  // 10%
    double min_acceptable_score = 40.0;
};

} // namespace sentinel

#endif // SENTINEL_NETWORK_METRICS_H
