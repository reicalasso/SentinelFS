#pragma once

#include "SQLiteHandler.h"
#include <string>

namespace SentinelFS {

    /**
     * @brief Manages session table records.
     */
    class SessionManager {
    public:
        explicit SessionManager(SQLiteHandler* handler) : handler_(handler) {}

        bool upsertSession(const std::string& sessionId,
                           const std::string& deviceId,
                           long long createdAt,
                           long long lastActive,
                           const std::string& sessionCodeHash);

        bool touchSession(const std::string& sessionId, long long lastActive);

    private:
        SQLiteHandler* handler_;
    };

} // namespace SentinelFS
