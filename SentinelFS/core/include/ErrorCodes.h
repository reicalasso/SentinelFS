#pragma once

#include <string>
#include <unordered_map>

namespace SentinelFS {
namespace Core {

enum class ErrorCode : int {
    // Network Errors (1000-1999)
    CONNECTION_FAILED = 1000,
    PEER_NOT_FOUND = 1001,
    DISCOVERY_FAILED = 1002,
    BANDWIDTH_LIMIT_EXCEEDED = 1003,
    
    // Security Errors (2000-2999)
    SESSION_CODE_MISMATCH = 2000,
    CERTIFICATE_VERIFICATION_FAILED = 2001,
    ENCRYPTION_FAILED = 2002,
    AUTHENTICATION_FAILED = 2003,
    
    // Sync Errors (3000-3999)
    FILE_NOT_FOUND = 3000,
    CONFLICT_DETECTED = 3001,
    SYNC_IN_PROGRESS = 3002,
    DELTA_GENERATION_FAILED = 3003,
    
    // Storage Errors (4000-4999)
    DISK_FULL = 4000,
    PERMISSION_DENIED = 4001,
    FILE_CORRUPTED = 4002,
    
    // System Errors (5000-5999)
    DAEMON_NOT_RUNNING = 5000,
    INTERNAL_ERROR = 5001,
    INVALID_CONFIGURATION = 5002,
    
    // Success
    SUCCESS = 0
};

class ErrorInfo {
public:
    ErrorCode code;
    std::string message;
    std::string details;
    
    ErrorInfo(ErrorCode code, const std::string& message, const std::string& details = "")
        : code(code), message(message), details(details) {}
    
    std::string toJson() const;
    static ErrorInfo fromJson(const std::string& json);
    static std::string getErrorCodeString(ErrorCode code);
};

class ErrorRegistry {
private:
    static std::unordered_map<ErrorCode, std::string> errorMessages_;
    
public:
    static void initialize();
    static std::string getMessage(ErrorCode code);
    static ErrorInfo createError(ErrorCode code, const std::string& details = "");
};

} // namespace Core
} // namespace SentinelFS
