#include "ErrorCodes.h"
#include <sstream>
#include <algorithm>

namespace SentinelFS {
namespace Core {

std::unordered_map<ErrorCode, std::string> ErrorRegistry::errorMessages_;

void ErrorRegistry::initialize() {
    errorMessages_ = {
        {ErrorCode::CONNECTION_FAILED, "Failed to establish connection"},
        {ErrorCode::PEER_NOT_FOUND, "Peer not found in network"},
        {ErrorCode::DISCOVERY_FAILED, "Peer discovery failed"},
        {ErrorCode::BANDWIDTH_LIMIT_EXCEEDED, "Bandwidth limit exceeded"},
        
        {ErrorCode::SESSION_CODE_MISMATCH, "Session code mismatch between peers"},
        {ErrorCode::CERTIFICATE_VERIFICATION_FAILED, "Certificate verification failed"},
        {ErrorCode::ENCRYPTION_FAILED, "Encryption operation failed"},
        {ErrorCode::AUTHENTICATION_FAILED, "Authentication failed"},
        
        {ErrorCode::FILE_NOT_FOUND, "File not found"},
        {ErrorCode::CONFLICT_DETECTED, "File conflict detected"},
        {ErrorCode::SYNC_IN_PROGRESS, "Synchronization already in progress"},
        {ErrorCode::DELTA_GENERATION_FAILED, "Failed to generate file delta"},
        
        {ErrorCode::DISK_FULL, "Disk space full"},
        {ErrorCode::PERMISSION_DENIED, "Permission denied"},
        {ErrorCode::FILE_CORRUPTED, "File corrupted"},
        
        {ErrorCode::DAEMON_NOT_RUNNING, "SentinelFS daemon is not running"},
        {ErrorCode::INTERNAL_ERROR, "Internal system error"},
        {ErrorCode::INVALID_CONFIGURATION, "Invalid configuration"},
        
        {ErrorCode::SUCCESS, "Operation successful"}
    };
}

std::string ErrorRegistry::getMessage(ErrorCode code) {
    auto it = errorMessages_.find(code);
    if (it != errorMessages_.end()) {
        return it->second;
    }
    return "Unknown error";
}

ErrorInfo ErrorRegistry::createError(ErrorCode code, const std::string& details) {
    return ErrorInfo(code, getMessage(code), details);
}

std::string ErrorInfo::toJson() const {
    std::ostringstream oss;
    oss << "{\"code\":" << static_cast<int>(code) 
        << ",\"message\":\"" << message << "\""
        << ",\"details\":\"" << details << "\"}";
    return oss.str();
}

ErrorInfo ErrorInfo::fromJson(const std::string& json) {
    // Simple JSON parsing - in production, use a proper JSON library
    // This is a simplified implementation for demonstration
    size_t codePos = json.find("\"code\":");
    size_t msgPos = json.find("\"message\":");
    size_t detPos = json.find("\"details\":");
    
    if (codePos == std::string::npos || msgPos == std::string::npos) {
        return ErrorInfo(ErrorCode::INTERNAL_ERROR, "Failed to parse error JSON");
    }
    
    // Extract code (simplified)
    size_t codeStart = json.find(":", codePos) + 1;
    size_t codeEnd = json.find(",", codeStart);
    std::string codeStr = json.substr(codeStart, codeEnd - codeStart);
    int codeInt = std::stoi(codeStr);
    
    return ErrorInfo(static_cast<ErrorCode>(codeInt), "Parsed error", "Parsed details");
}

std::string ErrorInfo::getErrorCodeString(ErrorCode code) {
    switch (code) {
        case ErrorCode::CONNECTION_FAILED: return "CONNECTION_FAILED";
        case ErrorCode::PEER_NOT_FOUND: return "PEER_NOT_FOUND";
        case ErrorCode::DISCOVERY_FAILED: return "DISCOVERY_FAILED";
        case ErrorCode::BANDWIDTH_LIMIT_EXCEEDED: return "BANDWIDTH_LIMIT_EXCEEDED";
        
        case ErrorCode::SESSION_CODE_MISMATCH: return "SESSION_CODE_MISMATCH";
        case ErrorCode::CERTIFICATE_VERIFICATION_FAILED: return "CERTIFICATE_VERIFICATION_FAILED";
        case ErrorCode::ENCRYPTION_FAILED: return "ENCRYPTION_FAILED";
        case ErrorCode::AUTHENTICATION_FAILED: return "AUTHENTICATION_FAILED";
        
        case ErrorCode::FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case ErrorCode::CONFLICT_DETECTED: return "CONFLICT_DETECTED";
        case ErrorCode::SYNC_IN_PROGRESS: return "SYNC_IN_PROGRESS";
        case ErrorCode::DELTA_GENERATION_FAILED: return "DELTA_GENERATION_FAILED";
        
        case ErrorCode::DISK_FULL: return "DISK_FULL";
        case ErrorCode::PERMISSION_DENIED: return "PERMISSION_DENIED";
        case ErrorCode::FILE_CORRUPTED: return "FILE_CORRUPTED";
        
        case ErrorCode::DAEMON_NOT_RUNNING: return "DAEMON_NOT_RUNNING";
        case ErrorCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
        case ErrorCode::INVALID_CONFIGURATION: return "INVALID_CONFIGURATION";
        
        case ErrorCode::SUCCESS: return "SUCCESS";
        default: return "UNKNOWN";
    }
}

} // namespace Core
} // namespace SentinelFS
