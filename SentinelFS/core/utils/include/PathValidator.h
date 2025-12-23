#pragma once

#include <string>
#include <filesystem>

namespace SentinelFS {

/**
 * @brief Path validation utilities for security
 * 
 * Provides functions to validate and sanitize file paths to prevent
 * directory traversal attacks and ensure paths stay within allowed directories.
 */
class PathValidator {
public:
    /**
     * @brief Validates that a relative path stays within the base directory
     * @param basePath The base directory (must be absolute)
     * @param relativePath The relative path from network/external source
     * @return true if the path is safe, false otherwise
     */
    static bool isPathWithinDirectory(const std::string& basePath, const std::string& relativePath);
    
    /**
     * @brief Sanitizes a path and returns the absolute path if valid
     * @param basePath The base directory (must be absolute)
     * @param relativePath The relative path to sanitize
     * @return Absolute path if valid, empty string if invalid
     */
    static std::string sanitizePath(const std::string& basePath, const std::string& relativePath);
    
    /**
     * @brief Checks if a path contains suspicious patterns
     * @param path The path to check
     * @return true if the path contains suspicious patterns
     */
    static bool containsSuspiciousPatterns(const std::string& path);
    
private:
    static constexpr const char* SUSPICIOUS_PATTERNS[] = {
        "..", "~", "$", "%", "*", "?", "\"", "<", ">", "|"
    };
};

} // namespace SentinelFS
