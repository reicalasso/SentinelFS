#include "BandwidthLimiter.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>

using namespace SentinelFS;

void test_unlimited() {
    std::cout << "Running test_unlimited..." << std::endl;
    BandwidthLimiter limiter(0); // Unlimited
    
    auto start = std::chrono::steady_clock::now();
    bool allowed = limiter.requestTransfer(1024 * 1024); // 1MB
    auto end = std::chrono::steady_clock::now();
    
    assert(allowed);
    // Should be very fast
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    assert(duration < 100); 
    
    std::cout << "test_unlimited passed." << std::endl;
}

void test_rate_limiting() {
    std::cout << "Running test_rate_limiting..." << std::endl;
    
    // 1000 bytes per second, small burst
    size_t rate = 1000;
    BandwidthLimiter limiter(rate, rate); 
    
    // First request should pass (burst capacity)
    assert(limiter.requestTransfer(500));
    
    // Second request might block if we exceed burst
    // Let's try to consume more than burst
    // If burst is 1000, and we consumed 500, we have 500 left.
    // Requesting 1000 should block for ~0.5s
    
    auto start = std::chrono::steady_clock::now();
    limiter.requestTransfer(1000);
    auto end = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // Should take at least some time, roughly 500ms worth of tokens needed
    // 500 bytes / 1000 B/s = 0.5s = 500ms
    // Allow some margin
    assert(duration >= 400);
    
    std::cout << "test_rate_limiting passed." << std::endl;
}

void test_try_transfer() {
    std::cout << "Running test_try_transfer..." << std::endl;
    
    BandwidthLimiter limiter(1000, 1000); // 1KB/s, 1KB burst
    
    // Consume all tokens
    limiter.requestTransfer(1000);
    
    // Try to transfer more immediately
    size_t allowed = limiter.tryTransfer(100);
    // Should be 0 or very small (depending on how fast it refills)
    assert(allowed < 100);
    
    std::cout << "test_try_transfer passed." << std::endl;
}

int main() {
    try {
        test_unlimited();
        test_rate_limiting();
        test_try_transfer();
        std::cout << "All BandwidthLimiter tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
