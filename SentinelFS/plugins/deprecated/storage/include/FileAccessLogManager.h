#pragma once

#include "SQLiteHandler.h"
#include <string>

namespace SentinelFS {

    /**
     * @brief Manages file_access_log table records.
     */
    class FileAccessLogManager {
    public:
        explicit FileAccessLogManager(SQLiteHandler* handler) : handler_(handler) {}

        bool logAccess(const std::string& filePath,
                       const std::string& opType,
                       const std::string& deviceId,
                       long long timestamp);

    private:
        SQLiteHandler* handler_;
    };

} // namespace SentinelFS
