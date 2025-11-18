#include "auto_remesh/network_metrics.h"
#include <cmath>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace sentinel {

void NetworkMetrics::update_rtt(double rtt_ms) {
    current_rtt_ms = rtt_ms;
    
    // Update min/max
    min_rtt_ms = std::min(min_rtt_ms, rtt_ms);
    max_rtt_ms = std::max(max_rtt_ms, rtt_ms);
    
    // Add to history
    rtt_history.push_back(rtt_ms);
    if (rtt_history.size() > MAX_HISTORY) {
        rtt_history.erase(rtt_history.begin());
    }
    
    // Calculate moving average
    if (!rtt_history.empty()) {
        avg_rtt_ms = std::accumulate(rtt_history.begin(), rtt_history.end(), 0.0) 
                     / rtt_history.size();
    }
    
    // Update jitter
    jitter_ms = calculate_jitter();
    
    // Update last successful ping time
    last_successful_ping = std::chrono::steady_clock::now();
}

void NetworkMetrics::record_packet_sent() {
    packets_sent++;
    // Recalculate loss rate
    if (packets_sent > 0) {
        loss_rate = static_cast<double>(packets_lost) / packets_sent;
    }
}

void NetworkMetrics::record_packet_lost() {
    packets_lost++;
    packets_sent++;  // Lost packet still counts as sent
    // Recalculate loss rate
    loss_rate = static_cast<double>(packets_lost) / packets_sent;
}

void NetworkMetrics::update_bandwidth(uint64_t bytes_transferred, double duration_ms) {
    if (duration_ms > 0) {
        // Convert to Mbps: (bytes * 8) / (duration_ms / 1000) / 1000000
        double bits_per_second = (bytes_transferred * 8.0) / (duration_ms / 1000.0);
        double mbps = bits_per_second / 1000000.0;
        
        // Exponential moving average (smoothing factor = 0.3)
        if (estimated_bandwidth_mbps == 0.0) {
            estimated_bandwidth_mbps = mbps;
        } else {
            estimated_bandwidth_mbps = 0.7 * estimated_bandwidth_mbps + 0.3 * mbps;
        }
    }
}

double NetworkMetrics::calculate_jitter() const {
    if (rtt_history.size() < 2) {
        return 0.0;
    }
    
    // Calculate standard deviation of RTT
    double mean = avg_rtt_ms;
    double sq_sum = 0.0;
    
    for (double rtt : rtt_history) {
        double diff = rtt - mean;
        sq_sum += diff * diff;
    }
    
    double variance = sq_sum / rtt_history.size();
    return std::sqrt(variance);
}

void NetworkMetrics::reset() {
    current_rtt_ms = 0.0;
    avg_rtt_ms = 0.0;
    min_rtt_ms = 999999.0;
    max_rtt_ms = 0.0;
    jitter_ms = 0.0;
    packets_sent = 0;
    packets_lost = 0;
    loss_rate = 0.0;
    estimated_bandwidth_mbps = 0.0;
    connection_resets = 0;
    quality_score = 0.0;
    rtt_history.clear();
}

bool NetworkMetrics::is_healthy() const {
    NetworkQualityThresholds thresholds;
    
    return avg_rtt_ms < thresholds.max_acceptable_rtt_ms &&
           jitter_ms < thresholds.max_acceptable_jitter_ms &&
           loss_rate < thresholds.max_acceptable_loss_rate &&
           quality_score >= thresholds.min_acceptable_score;
}

std::string NetworkMetrics::to_string() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "NetworkMetrics {"
        << " RTT: " << avg_rtt_ms << "ms"
        << " (min: " << min_rtt_ms << ", max: " << max_rtt_ms << ")"
        << ", Jitter: " << jitter_ms << "ms"
        << ", Loss: " << (loss_rate * 100.0) << "%"
        << " (" << packets_lost << "/" << packets_sent << ")"
        << ", BW: " << estimated_bandwidth_mbps << " Mbps"
        << ", Score: " << quality_score
        << ", Resets: " << connection_resets
        << " }";
    return oss.str();
}

} // namespace sentinel
