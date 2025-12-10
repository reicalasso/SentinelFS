#pragma once

#include "SQLiteHandler.h"
#include <string>
#include <vector>

namespace SentinelFS {

    /**
     * @brief Manages device table records.
     */
    class DeviceManager {
    public:
        explicit DeviceManager(SQLiteHandler* handler) : handler_(handler) {}

        bool upsertDevice(const std::string& deviceId,
                          const std::string& name,
                          long long lastSeen,
                          const std::string& platform,
                          const std::string& version);

        std::vector<std::string> getAllDeviceIds();

    private:
        SQLiteHandler* handler_;
    };

} // namespace SentinelFS
