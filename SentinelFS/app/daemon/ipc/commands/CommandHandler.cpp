#include "CommandHandler.h"
#include <sstream>
#include <ctime>
#include <cstdint>
#include <iomanip>

namespace SentinelFS {

std::string CommandHandler::formatBytes(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
    return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
}

std::string CommandHandler::formatTime(int64_t timestamp) {
    auto now = std::time(nullptr);
    auto diff = now - timestamp;
    
    if (diff < 0 || timestamp <= 0) {
        return "Unknown";
    } else if (diff < 60) {
        return "Just now";
    } else if (diff < 3600) {
        return std::to_string(diff / 60) + " mins ago";
    } else if (diff < 86400) {
        return std::to_string(diff / 3600) + " hours ago";
    } else if (diff < 2592000) { // 30 days
        return std::to_string(diff / 86400) + " days ago";
    } else {
        return "Over a month ago";
    }
}

std::string CommandHandler::escapeJson(const std::string& str) {
    std::ostringstream ss;
    for (char c : str) {
        switch (c) {
            case '"': ss << "\\\""; break;
            case '\\': ss << "\\\\"; break;
            case '\b': ss << "\\b"; break;
            case '\f': ss << "\\f"; break;
            case '\n': ss << "\\n"; break;
            case '\r': ss << "\\r"; break;
            case '\t': ss << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                } else {
                    ss << c;
                }
        }
    }
    return ss.str();
}

} // namespace SentinelFS
