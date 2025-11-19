#include "DeltaSync.h"

namespace SentinelFS {

    uint32_t DeltaSync::calculateAdler32(const std::vector<uint8_t>& data) {
        uint32_t a = 1, b = 0;
        for (uint8_t byte : data) {
            a = (a + byte) % 65521;
            b = (b + a) % 65521;
        }
        return (b << 16) | a;
    }

}
