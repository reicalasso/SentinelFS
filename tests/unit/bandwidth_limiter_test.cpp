#include <chrono>
#include <future>
#include <iostream>
#include "BandwidthLimiter.h"

using namespace SentinelFS;

int main() {
    std::cout << "Starting BandwidthLimiter stats test..." << std::endl;

    const std::size_t rateBytesPerSec = 32 * 1024;   // 32 KB/s
    const std::size_t bytesToSend     = rateBytesPerSec / 2; // half second of data

    BandwidthLimiter limiter(rateBytesPerSec, rateBytesPerSec / 4);

    limiter.requestTransfer(bytesToSend);

    auto stats = limiter.getStats();
    if (stats.second == 0) {
        std::cerr << "BandwidthLimiter did not wait despite throttling" << std::endl;
        return 1;
    }

    std::cout << "Configured rate: " << (rateBytesPerSec / 1024) << " KB/s, bytes: "
              << bytesToSend << ", wait: " << stats.second << " ms" << std::endl;
    std::cout << "BandwidthLimiter stats test PASSED" << std::endl;
    return 0;
}
