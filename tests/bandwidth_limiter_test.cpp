#include <iostream>
#include <chrono>
#include "BandwidthLimiter.h"

using namespace SentinelFS;

int main() {
    std::cout << "Starting BandwidthLimiter basic rate test..." << std::endl;

    const std::size_t rateBytesPerSec = 100 * 1024;   // 100 KB/s
    const std::size_t bytesToSend     = 300 * 1024;   // 300 KB; > 2x rate => bekleme zorunlu

    BandwidthLimiter limiter(rateBytesPerSec);

    auto start = std::chrono::steady_clock::now();
    limiter.requestTransfer(bytesToSend);
    auto end   = std::chrono::steady_clock::now();

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Configured rate: " << (rateBytesPerSec / 1024)
              << " KB/s, bytes: " << bytesToSend
              << ", elapsed: " << elapsedMs << " ms" << std::endl;

    // Teorik bekleme ~1s, biraz toleransla en az 800ms beklenmesini bekliyoruz.
    if (elapsedMs < 800) {
        std::cerr << "BandwidthLimiter allowed transfer too fast (" << elapsedMs
                  << " ms) — expected at least ~800 ms" << std::endl;
        return 1;
    }

    std::cout << "BandwidthLimiter basic rate test PASSED" << std::endl;
    return 0;
}
