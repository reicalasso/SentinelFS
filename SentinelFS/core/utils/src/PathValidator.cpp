#include "PathValidator.h"
#include "Logger.h"
#include <algorithm>

namespace SentinelFS {

bool PathValidator::isPathWithinDirectory(const std::string& basePath, const std::string& relativePath) {
    try {
        // Check for suspicious patterns first
        if (containsSuspiciousPatterns(relativePath)) {
            return false;
        }
        
        // Convert base path to absolute path
        std::filesystem::path absBase = std::filesystem::absolute(basePath);
        
        // Construct full path from base and relative
        std::filesystem::path fullPath = absBase / relativePath;
        
        // Normalize the path (resolves ".." and "." components)
        std::filesystem::path normalized = fullPath.lexically_normal();
        
        // Convert to absolute path to resolve any remaining relative components
        std::filesystem::path absPath = std::filesystem::absolute(normalized);
        
        // For paths that don't exist yet, use weakly_canonical
        if (!std::filesystem::exists(absPath)) {
            absPath = std::filesystem::weakly_canonical(absPath);
        }
        
        // Check if the absolute path starts with the base path
        auto absBaseStr = absBase.string();
        auto absPathStr = absPath.string();
        
        // Ensure both paths end with separator for proper comparison
        if (absBaseStr.back() != '/' && absBaseStr.back() != '\\') {
            absBaseStr += std::filesystem::path::preferred_separator;
        }
        
        // Check if the resolved path is within base directory
        return absPathStr.find(absBaseStr) == 0;
        
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::instance().error("Path validation error: " + std::string(e.what()), "PathValidator");
        return false;
    } catch (const std::exception& e) {
        Logger::instance().error("Path validation error: " + std::string(e.what()), "PathValidator");
        return false;
    }
}

std::string PathValidator::sanitizePath(const std::string& basePath, const std::string& relativePath) {
    if (!isPathWithinDirectory(basePath, relativePath)) {
        Logger::instance().warn("Rejected unsafe path: " + relativePath, "PathValidator");
        return "";
    }
    
    try {
        std::filesystem::path absBase = std::filesystem::absolute(basePath);
        std::filesystem::path fullPath = absBase / relativePath;
        std::filesystem::path normalized = fullPath.lexically_normal();
        
        return std::filesystem::absolute(normalized).string();
    } catch (const std::exception& e) {
        Logger::instance().error("Path sanitization error: " + std::string(e.what()), "PathValidator");
        return "";
    }
}

bool PathValidator::containsSuspiciousPatterns(const std::string& path) {
    // Check for path traversal patterns
    if (path.find("..") != std::string::npos) {
        return true;
    }
    
    // Check for other suspicious characters/patterns
    for (const char* pattern : SUSPICIOUS_PATTERNS) {
        if (path.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    // Check for null bytes
    if (path.find('\0') != std::string::npos) {
        return true;
    }
    
    // Check for Windows UNC paths
    if (path.find("\\\\") == 0) {
        return true;
    }
    
    // Check for Windows drive letters (shouldn't be in relative paths)
    if (path.length() >= 2 && path[1] == ':') {
        return true;
    }
    
    return false;
}

} // namespace SentinelFS
