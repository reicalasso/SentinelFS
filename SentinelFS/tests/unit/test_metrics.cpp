#include "MetricsCollector.h"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace SentinelFS;

void test_singleton() {
    std::cout << "Running test_singleton..." << std::endl;
    MetricsCollector& m1 = MetricsCollector::instance();
    MetricsCollector& m2 = MetricsCollector::instance();
    assert(&m1 == &m2);
    std::cout << "test_singleton passed." << std::endl;
}

void test_counters() {
    std::cout << "Running test_counters..." << std::endl;
    MetricsCollector& metrics = MetricsCollector::instance();
    metrics.reset();
    
    metrics.incrementFilesSynced();
    metrics.incrementFilesSynced();
    
    auto sync = metrics.getSyncMetrics();
    assert(sync.filesSynced == 2);
    
    metrics.addBytesUploaded(1024);
    auto net = metrics.getNetworkMetrics();
    assert(net.bytesUploaded == 1024);
    
    std::cout << "test_counters passed." << std::endl;
}

void test_performance() {
    std::cout << "Running test_performance..." << std::endl;
    MetricsCollector& metrics = MetricsCollector::instance();
    metrics.reset();
    
    metrics.recordSyncLatency(100);
    metrics.recordSyncLatency(200);
    
    auto perf = metrics.getPerformanceMetrics();
    // Moving average might be implemented differently, but it should be non-zero
    assert(perf.avgSyncLatencyMs > 0);
    
    std::cout << "test_performance passed." << std::endl;
}

void test_transfers() {
    std::cout << "Running test_transfers..." << std::endl;
    MetricsCollector& metrics = MetricsCollector::instance();
    metrics.reset();
    
    std::string id = metrics.startTransfer("file.txt", "peer1", true, 1000);
    assert(!id.empty());
    
    auto active = metrics.getActiveTransfers();
    assert(active.size() == 1);
    assert(active[0].transferId == id);
    assert(active[0].filePath == "file.txt");
    
    metrics.updateTransferProgress(id, 500);
    active = metrics.getActiveTransfers();
    assert(active[0].transferredBytes == 500);
    assert(active[0].progress == 50);
    
    metrics.completeTransfer(id, true);
    active = metrics.getActiveTransfers();
    assert(active.empty());
    
    auto net = metrics.getNetworkMetrics();
    assert(net.transfersCompleted == 1);
    
    std::cout << "test_transfers passed." << std::endl;
}

int main() {
    try {
        test_singleton();
        test_counters();
        test_performance();
        test_transfers();
        std::cout << "All MetricsCollector tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
