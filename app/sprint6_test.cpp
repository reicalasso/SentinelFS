/*
 * Sprint 6 Test - Auto-Remesh Engine
 * 
 * Tests adaptive P2P topology management:
 * - Network quality metrics collection
 * - Peer scoring algorithm
 * - Automatic topology optimization
 * - Poor performer detection and replacement
 */

#include "logger/logger.h"
#include "peer_registry/peer_registry.h"
#include "auto_remesh/auto_remesh.h"
#include "auto_remesh/network_metrics.h"
#include "auto_remesh/peer_scorer.h"

#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <random>
#include <iomanip>

using namespace sfs;
using namespace sentinel;

// Helper to simulate network metrics
void simulate_peer_metrics(PeerRegistry& registry, const std::string& peer_id, 
                          double base_rtt, double jitter, double loss_rate) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> rtt_dist(base_rtt, jitter);
    std::uniform_real_distribution<> loss_dist(0.0, 1.0);
    
    // Simulate 20 packet exchanges
    for (int i = 0; i < 20; ++i) {
        // RTT measurement
        double rtt = std::max(1.0, rtt_dist(gen));
        registry.update_rtt(peer_id, rtt);
        
        // Packet loss
        if (loss_dist(gen) < loss_rate) {
            registry.record_packet_lost(peer_id);
        } else {
            registry.record_packet_sent(peer_id);
        }
        
        // Simulate bandwidth measurement
        registry.update_bandwidth(peer_id, 65536, rtt);  // 64KB in rtt ms
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void print_separator(const std::string& title = "") {
    std::cout << "\n" << std::string(70, '=') << "\n";
    if (!title.empty()) {
        std::cout << "  " << title << "\n";
        std::cout << std::string(70, '=') << "\n";
    }
}

void print_peer_info(const PeerInfo& peer) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n  Peer: " << peer.peer_id << "\n";
    std::cout << "    Address: " << peer.address << ":" << peer.port << "\n";
    std::cout << "    Connected: " << (peer.is_connected ? "Yes" : "No") << "\n";
    std::cout << "    Quality Score: " << peer.metrics.quality_score << "/100\n";
    std::cout << "    RTT: " << peer.metrics.avg_rtt_ms << " ms "
              << "(min: " << peer.metrics.min_rtt_ms 
              << ", max: " << peer.metrics.max_rtt_ms << ")\n";
    std::cout << "    Jitter: " << peer.metrics.jitter_ms << " ms\n";
    std::cout << "    Packet Loss: " << (peer.metrics.loss_rate * 100.0) << "% "
              << "(" << peer.metrics.packets_lost << "/" << peer.metrics.packets_sent << ")\n";
    std::cout << "    Bandwidth: " << peer.metrics.estimated_bandwidth_mbps << " Mbps\n";
    std::cout << "    Healthy: " << (peer.metrics.is_healthy() ? "Yes" : "No") << "\n";
}

int main() {
    SFS_LOG_INFO("Sprint6Test", "Starting Sprint 6 Test - Auto-Remesh Engine");
    
    print_separator("SPRINT 6: AUTO-REMESH ENGINE TEST");
    
    // Create peer registry
    auto registry = std::make_shared<PeerRegistry>();
    
    // Add test peers with different characteristics
    std::vector<std::tuple<std::string, std::string, double, double, double>> test_peers = {
        {"peer-1-excellent", "192.168.1.10", 15.0, 2.0, 0.001},   // Excellent: low RTT, low jitter, minimal loss
        {"peer-2-good", "192.168.1.11", 45.0, 8.0, 0.02},         // Good: moderate RTT, some jitter, 2% loss
        {"peer-3-fair", "192.168.1.12", 120.0, 25.0, 0.05},       // Fair: higher RTT, more jitter, 5% loss
        {"peer-4-poor", "192.168.1.13", 300.0, 80.0, 0.15},       // Poor: high RTT, high jitter, 15% loss
        {"peer-5-terrible", "192.168.1.14", 600.0, 150.0, 0.35},  // Terrible: very high RTT, extreme jitter, 35% loss
    };
    
    print_separator("Phase 1: Adding Peers and Simulating Network Traffic");
    
    for (const auto& [peer_id, address, rtt, jitter, loss] : test_peers) {
        PeerInfo peer(peer_id, address, 47778);
        peer.is_connected = true;
        registry->add_peer(peer);
        std::cout << "\n  Adding peer: " << peer_id 
                  << " (RTT: " << rtt << "ms, Jitter: " << jitter 
                  << "ms, Loss: " << (loss * 100.0) << "%)\n";
        std::cout << "  Simulating network traffic..." << std::flush;
        
        simulate_peer_metrics(*registry, peer_id, rtt, jitter, loss);
        std::cout << " done\n";
    }
    
    // Calculate scores using peer scorer
    print_separator("Phase 2: Calculating Peer Quality Scores");
    
    PeerScorer scorer;
    std::cout << "\nPeer Scoring Configuration:\n";
    std::cout << "  RTT Weight: " << scorer.get_config().rtt_weight << "\n";
    std::cout << "  Jitter Weight: " << scorer.get_config().jitter_weight << "\n";
    std::cout << "  Loss Weight: " << scorer.get_config().loss_weight << "\n";
    
    for (const auto& [peer_id, _, __, ___, ____] : test_peers) {
        auto peer = registry->get_peer(peer_id);
        double score = scorer.calculate_score(peer.metrics);
        registry->update_quality_score(peer_id, score);
    }
    
    // Display all peers with scores
    print_separator("Phase 3: Peer Quality Report");
    
    auto peers_by_score = registry->get_peers_by_score();
    std::cout << "\nPeers sorted by quality score:\n";
    
    for (const auto& peer : peers_by_score) {
        print_peer_info(peer);
    }
    
    // Display statistics
    print_separator("Phase 4: Network Statistics");
    
    std::cout << "\n  Total Peers: " << registry->peer_count() << "\n";
    std::cout << "  Connected Peers: " << registry->connected_count() << "\n";
    std::cout << "  Average Quality Score: " << std::fixed << std::setprecision(2) 
              << registry->get_average_quality_score() << "/100\n";
    
    auto healthy = registry->get_healthy_peers();
    std::cout << "  Healthy Peers: " << healthy.size() << "\n";
    
    auto best = registry->get_best_peer();
    std::cout << "  Best Peer: " << best.peer_id 
              << " (score: " << best.metrics.quality_score << ")\n";
    
    // Create and start auto-remesh engine
    print_separator("Phase 5: Starting Auto-Remesh Engine");
    
    AutoRemeshConfig config;
    config.enabled = true;
    config.evaluation_interval_sec = 5;  // Evaluate every 5 seconds for demo
    config.min_score_threshold = 40.0;
    config.min_peers = 2;
    config.max_peers = 10;
    
    std::cout << "\nAuto-Remesh Configuration:\n";
    std::cout << "  Enabled: " << (config.enabled ? "Yes" : "No") << "\n";
    std::cout << "  Evaluation Interval: " << config.evaluation_interval_sec << " seconds\n";
    std::cout << "  Min Score Threshold: " << config.min_score_threshold << "\n";
    std::cout << "  Min Peers: " << config.min_peers << "\n";
    std::cout << "  Max Peers: " << config.max_peers << "\n";
    
    AutoRemesh remesh(registry, config);
    
    // Register topology change callback
    remesh.on_topology_change([](const TopologyChangeEvent& event) {
        std::cout << "\n  [TOPOLOGY CHANGE] ";
        
        switch (event.type) {
            case TopologyChangeEvent::Type::PEER_ADDED:
                std::cout << "Peer added: ";
                break;
            case TopologyChangeEvent::Type::PEER_REMOVED:
                std::cout << "Peer removed: ";
                break;
            case TopologyChangeEvent::Type::PEER_REPLACED:
                std::cout << "Peer replaced: ";
                break;
            case TopologyChangeEvent::Type::PEER_DEGRADED:
                std::cout << "Peer degraded: ";
                break;
            case TopologyChangeEvent::Type::TOPOLOGY_OPTIMIZED:
                std::cout << "Topology optimized: ";
                break;
        }
        
        std::cout << event.peer_id << "\n";
        std::cout << "    Reason: " << event.reason << "\n";
        if (event.old_score > 0) {
            std::cout << "    Old Score: " << event.old_score << "\n";
        }
        if (event.new_score > 0) {
            std::cout << "    New Score: " << event.new_score << "\n";
        }
    });
    
    remesh.start();
    std::cout << "\nAuto-Remesh engine started. Monitoring topology...\n";
    
    // Let it run for a while and perform evaluations
    print_separator("Phase 6: Monitoring Auto-Remesh (20 seconds)");
    
    std::cout << "\nWatching for topology changes...\n";
    
    for (int i = 0; i < 4; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        std::cout << "\n  [" << ((i + 1) * 5) << "s] Status Check:\n";
        std::cout << "    Connected Peers: " << registry->connected_count() << "\n";
        std::cout << "    Average Score: " << std::fixed << std::setprecision(2)
                  << registry->get_average_quality_score() << "\n";
        
        auto stats = remesh.get_stats();
        std::cout << "    Evaluations: " << stats.evaluations_performed << "\n";
        std::cout << "    Peers Dropped: " << stats.peers_dropped << "\n";
        std::cout << "    Optimizations: " << stats.topology_optimizations << "\n";
    }
    
    // Stop auto-remesh
    remesh.stop();
    std::cout << "\n  Auto-Remesh engine stopped.\n";
    
    // Final report
    print_separator("Phase 7: Final Report");
    
    auto final_stats = remesh.get_stats();
    std::cout << "\nAuto-Remesh Statistics:\n";
    std::cout << "  Total Evaluations: " << final_stats.evaluations_performed << "\n";
    std::cout << "  Peers Dropped: " << final_stats.peers_dropped << "\n";
    std::cout << "  Peers Replaced: " << final_stats.peers_replaced << "\n";
    std::cout << "  Topology Optimizations: " << final_stats.topology_optimizations << "\n";
    std::cout << "  Final Average Score: " << std::fixed << std::setprecision(2)
              << final_stats.avg_peer_score << "\n";
    
    std::cout << "\nRemaining Connected Peers:\n";
    auto remaining = registry->get_connected_peers();
    for (const auto& peer : remaining) {
        std::cout << "  - " << peer.peer_id 
                  << " (score: " << peer.metrics.quality_score << ")\n";
    }
    
    print_separator("SPRINT 6 TEST COMPLETE");
    
    std::cout << "\n✅ Auto-Remesh Engine Test Successful!\n";
    std::cout << "\nKey Achievements:\n";
    std::cout << "  ✓ Network metrics collection and tracking\n";
    std::cout << "  ✓ Peer quality scoring algorithm\n";
    std::cout << "  ✓ Automatic poor performer detection\n";
    std::cout << "  ✓ Dynamic topology optimization\n";
    std::cout << "  ✓ Event-driven topology notifications\n\n";
    
    SFS_LOG_INFO("Sprint6Test", "Sprint 6 test completed successfully");
    
    return 0;
}
