#pragma once
#include <vector>
#include <cstdint>

namespace SentinelFS {

    class DeltaSync {
    public:
        static uint32_t calculateAdler32(const std::vector<uint8_t>& data);
    };

}
