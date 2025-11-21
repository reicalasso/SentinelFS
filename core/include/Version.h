#pragma once

#include <string>

namespace SentinelFS {
    struct Version {
        static constexpr int MAJOR = 1;
        static constexpr int MINOR = 0;
        static constexpr int PATCH = 0;
        static constexpr const char* STRING = "1.0.0";
        
        static std::string toString() {
            return std::string(STRING);
        }
    };
}
