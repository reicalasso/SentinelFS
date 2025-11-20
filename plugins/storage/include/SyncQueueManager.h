#pragma once

#include "SQLiteHandler.h"
#include <string>
#include <vector>

namespace SentinelFS {

    /**
     * @brief Manages sync_queue table records.
     */
    class SyncQueueManager {
    public:
        explicit SyncQueueManager(SQLiteHandler* handler) : handler_(handler) {}

        bool enqueue(const std::string& filePath,
                     const std::string& opType,
                     const std::string& status);

    private:
        SQLiteHandler* handler_;
    };

} // namespace SentinelFS
