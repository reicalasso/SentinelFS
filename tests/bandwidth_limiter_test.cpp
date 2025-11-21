#include <chrono>
#include <future>
#include <iostream>
#include "BandwidthLimiter.h"

using namespace SentinelFS;

int main() {
    std::cout << "Starting BandwidthLimiter timing test..." << std::endl;

    const std::size_t rateBytesPerSec = 32 * 1024;   // 32 KB/s
    const std::size_t bytesToSend     = rateBytesPerSec / 2; // ~0.5 second worth of data

    // Use a small burst capacity to force throttling on a single request.
    BandwidthLimiter limiter(rateBytesPerSec, rateBytesPerSec / 4);

    auto future = std::async(std::launch::async, [&]() {
        auto start = std::chrono::steady_clock::now();
        limiter.requestTransfer(bytesToSend);
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    });

    if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
        std::cerr << "BandwidthLimiter request timed out" << std::endl;
        return 1;
    }

    auto elapsedMs = future.get();

    std::cout << "Configured rate: " << (rateBytesPerSec / 1024)
              << " KB/s, bytes: " << bytesToSend
              << ", elapsed: " << elapsedMs << " ms" << std::endl;

    const auto expectedMs = static_cast<long>((bytesToSend * 1000) / rateBytesPerSec);
    if (elapsedMs + 150 < expectedMs) {
        std::cerr << "BandwidthLimiter allowed transfer too fast (" << elapsedMs
                  << " ms) — expected at least ~" << expectedMs << " ms" << std::endl;
        return 1;
    }

    std::cout << "BandwidthLimiter timing test PASSED" << std::endl;
    return 0;
}
