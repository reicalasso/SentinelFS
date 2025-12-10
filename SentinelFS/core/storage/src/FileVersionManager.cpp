#include "FileVersionManager.h"
#include "Logger.h"
#include "SHA256.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <ctime>
#include <random>
#include <regex>

// Simple JSON helpers (minimal implementation to avoid dependencies)
namespace {
    std::string escapeJson(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }
    
    std::string unescapeJson(const std::string& s) {
        std::string result;
        for (size_t i = 0; i < s.length(); ++i) {
            if (s[i] == '\\' && i + 1 < s.length()) {
                switch (s[i + 1]) {
                    case '"': result += '"'; ++i; break;
                    case '\\': result += '\\'; ++i; break;
                    case 'n': result += '\n'; ++i; break;
                    case 'r': result += '\r'; ++i; break;
                    case 't': result += '\t'; ++i; break;
                    default: result += s[i];
                }
            } else {
                result += s[i];
            }
        }
        return result;
    }
    
    std::string extractJsonString(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":\"";
        auto pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.length();
        auto end = json.find("\"", pos);
        if (end == std::string::npos) return "";
        return unescapeJson(json.substr(pos, end - pos));
    }
    
    uint64_t extractJsonNumber(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\":";
        auto pos = json.find(search);
        if (pos == std::string::npos) return 0;
        pos += search.length();
        std::string numStr;
        while (pos < json.length() && (std::isdigit(json[pos]) || json[pos] == '-')) {
            numStr += json[pos++];
        }
        return numStr.empty() ? 0 : std::stoull(numStr);
    }
}

namespace SentinelFS {

FileVersionManager::FileVersionManager(
    const std::string& watchDirectory,
    const VersioningConfig& config
) : watchDirectory_(watchDirectory), config_(config) {
    namespace fs = std::filesystem;
    
    // Ensure watch directory path is absolute
    if (!fs::path(watchDirectory_).is_absolute()) {
        watchDirectory_ = fs::absolute(watchDirectory_).string();
    }
    
    // Create version storage path
    versionStoragePath_ = watchDirectory_ + "/" + config_.versionStoragePath;
    
    try {
        if (!fs::exists(versionStoragePath_)) {
            fs::create_directories(versionStoragePath_);
            Logger::instance().info("Created version storage directory: " + versionStoragePath_, "FileVersionManager");
        }
    } catch (const std::exception& e) {
        Logger::instance().error("Failed to create version storage: " + std::string(e.what()), "FileVersionManager");
    }
}

FileVersionManager::~FileVersionManager() = default;

std::optional<FileVersion> FileVersionManager::createVersion(
    const std::string& filePath,
    const std::string& changeType,
    const std::string& peerId,
    const std::string& /* comment */
) {
    namespace fs = std::filesystem;
    
    if (!config_.enableVersioning) {
        return std::nullopt;
    }
    
    if (isExcluded(filePath)) {
        Logger::instance().debug("File excluded from versioning: " + filePath, "FileVersionManager");
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Check if file exists
        if (!fs::exists(filePath)) {
            Logger::instance().warn("Cannot version non-existent file: " + filePath, "FileVersionManager");
            return std::nullopt;
        }
        
        // Read file content
        std::ifstream file(filePath, std::ios::binary);
        std::vector<uint8_t> content((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
        file.close();
        
        // Calculate hash
        std::string hash = SHA256::hashBytes(content);
        
        // Get file timestamp (C++17 compatible)
        auto ftime = fs::last_write_time(filePath);
        // Convert file_time to time_t using duration_cast
        auto duration = ftime.time_since_epoch();
        // Adjust for filesystem epoch difference (typically 1970 vs 1601 on some systems)
        // Use current time as fallback for more reliable timestamps
        uint64_t timestamp = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
        (void)duration; // Suppress unused variable warning
        
        // Create version from data
        return createVersionFromData(filePath, content, hash, timestamp, peerId, changeType);
        
    } catch (const std::exception& e) {
        Logger::instance().error("Failed to create version for " + filePath + ": " + e.what(), "FileVersionManager");
        return std::nullopt;
    }
}

std::optional<FileVersion> FileVersionManager::createVersionFromData(
    const std::string& filePath,
    const std::vector<uint8_t>& data,
    const std::string& hash,
    uint64_t timestamp,
    const std::string& peerId,
    const std::string& changeType
) {
    namespace fs = std::filesystem;
    
    if (!config_.enableVersioning) {
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Get or create version directory for this file
        std::string versionDir = getVersionDirForFile(filePath);
        ensureVersionDirectory(versionDir);
        
        // Load existing versions
        std::vector<FileVersion> versions;
        loadMetadata(versionDir, versions);
        
        // Check if this exact version already exists (same hash)
        for (const auto& v : versions) {
            if (v.hash == hash) {
                Logger::instance().debug("Version with same hash already exists: " + hash, "FileVersionManager");
                return v;
            }
        }
        
        // Generate version info
        FileVersion version;
        version.versionId = generateVersionId();
        version.filePath = filePath;
        version.hash = hash;
        version.peerId = peerId;
        version.timestamp = timestamp;
        version.size = data.size();
        version.changeType = changeType;
        
        // Generate version filename
        fs::path originalPath(filePath);
        std::string ext = originalPath.extension().string();
        version.versionPath = versionDir + "/v_" + std::to_string(timestamp) + "_" + 
                             hash.substr(0, 8) + ext;
        
        // Write versioned file
        std::ofstream outFile(version.versionPath, std::ios::binary);
        outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
        outFile.close();
        
        // Add to versions list
        versions.push_back(version);
        
        // Sort by timestamp (newest first)
        std::sort(versions.begin(), versions.end(), [](const FileVersion& a, const FileVersion& b) {
            return a.timestamp > b.timestamp;
        });
        
        // Prune old versions
        while (versions.size() > config_.maxVersionsPerFile) {
            const auto& oldVersion = versions.back();
            try {
                fs::remove(oldVersion.versionPath);
                Logger::instance().debug("Pruned old version: " + oldVersion.versionPath, "FileVersionManager");
            } catch (...) {}
            versions.pop_back();
        }
        
        // Save metadata
        saveMetadata(versionDir, versions);
        
        Logger::instance().info("Created version " + std::to_string(version.versionId) + 
                               " for " + filePath + " (type: " + changeType + ")", "FileVersionManager");
        
        return version;
        
    } catch (const std::exception& e) {
        Logger::instance().error("Failed to create version from data: " + std::string(e.what()), "FileVersionManager");
        return std::nullopt;
    }
}

std::vector<FileVersion> FileVersionManager::getVersions(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<FileVersion> versions;
    std::string versionDir = getVersionDirForFile(filePath);
    
    if (std::filesystem::exists(versionDir)) {
        loadMetadata(versionDir, versions);
    }
    
    // Sort by timestamp descending (newest first)
    std::sort(versions.begin(), versions.end(), [](const FileVersion& a, const FileVersion& b) {
        return a.timestamp > b.timestamp;
    });
    
    return versions;
}

std::optional<FileVersion> FileVersionManager::getVersion(
    const std::string& filePath,
    uint64_t versionId
) const {
    auto versions = getVersions(filePath);
    for (const auto& v : versions) {
        if (v.versionId == versionId) {
            return v;
        }
    }
    return std::nullopt;
}

std::optional<FileVersion> FileVersionManager::getLatestVersion(const std::string& filePath) const {
    auto versions = getVersions(filePath);
    if (versions.empty()) {
        return std::nullopt;
    }
    return versions.front();
}

bool FileVersionManager::restoreVersion(
    const std::string& filePath,
    uint64_t versionId,
    bool createBackup
) {
    namespace fs = std::filesystem;
    
    auto version = getVersion(filePath, versionId);
    if (!version) {
        Logger::instance().error("Version not found: " + std::to_string(versionId), "FileVersionManager");
        return false;
    }
    
    try {
        // Create backup of current file if requested
        if (createBackup && fs::exists(filePath)) {
            createVersion(filePath, "backup", "", "Backup before restore");
        }
        
        // Copy version file to original location
        fs::copy_file(version->versionPath, filePath, fs::copy_options::overwrite_existing);
        
        Logger::instance().info("Restored version " + std::to_string(versionId) + 
                               " to " + filePath, "FileVersionManager");
        return true;
        
    } catch (const std::exception& e) {
        Logger::instance().error("Failed to restore version: " + std::string(e.what()), "FileVersionManager");
        return false;
    }
}

std::optional<std::vector<uint8_t>> FileVersionManager::readVersionContent(
    const std::string& filePath,
    uint64_t versionId
) const {
    auto version = getVersion(filePath, versionId);
    if (!version) {
        return std::nullopt;
    }
    
    try {
        std::ifstream file(version->versionPath, std::ios::binary);
        std::vector<uint8_t> content((std::istreambuf_iterator<char>(file)),
                                      std::istreambuf_iterator<char>());
        return content;
    } catch (...) {
        return std::nullopt;
    }
}

bool FileVersionManager::deleteVersion(const std::string& filePath, uint64_t versionId) {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string versionDir = getVersionDirForFile(filePath);
    std::vector<FileVersion> versions;
    loadMetadata(versionDir, versions);
    
    auto it = std::find_if(versions.begin(), versions.end(), [versionId](const FileVersion& v) {
        return v.versionId == versionId;
    });
    
    if (it == versions.end()) {
        return false;
    }
    
    try {
        fs::remove(it->versionPath);
        versions.erase(it);
        saveMetadata(versionDir, versions);
        return true;
    } catch (...) {
        return false;
    }
}

size_t FileVersionManager::deleteAllVersions(const std::string& filePath) {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string versionDir = getVersionDirForFile(filePath);
    
    if (!fs::exists(versionDir)) {
        return 0;
    }
    
    std::vector<FileVersion> versions;
    loadMetadata(versionDir, versions);
    size_t count = versions.size();
    
    try {
        fs::remove_all(versionDir);
    } catch (...) {}
    
    return count;
}

size_t FileVersionManager::pruneVersions(const std::string& filePath) {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t pruned = 0;
    
    if (!filePath.empty()) {
        // Prune single file
        std::string versionDir = getVersionDirForFile(filePath);
        std::vector<FileVersion> versions;
        loadMetadata(versionDir, versions);
        
        std::sort(versions.begin(), versions.end(), [](const FileVersion& a, const FileVersion& b) {
            return a.timestamp > b.timestamp;
        });
        
        while (versions.size() > config_.maxVersionsPerFile) {
            try {
                fs::remove(versions.back().versionPath);
                pruned++;
            } catch (...) {}
            versions.pop_back();
        }
        
        saveMetadata(versionDir, versions);
    } else {
        // Prune all files if total storage exceeds limit
        size_t totalBytes = getTotalVersionStorageBytes();
        
        if (totalBytes > config_.maxTotalVersionsBytes) {
            // Collect all versions across all files
            std::vector<std::pair<std::string, FileVersion>> allVersions;
            
            for (const auto& entry : fs::directory_iterator(versionStoragePath_)) {
                if (entry.is_directory()) {
                    std::vector<FileVersion> versions;
                    loadMetadata(entry.path().string(), versions);
                    for (const auto& v : versions) {
                        allVersions.emplace_back(entry.path().string(), v);
                    }
                }
            }
            
            // Sort by timestamp (oldest first)
            std::sort(allVersions.begin(), allVersions.end(), 
                     [](const auto& a, const auto& b) {
                         return a.second.timestamp < b.second.timestamp;
                     });
            
            // Delete oldest versions until under limit
            for (const auto& [dir, version] : allVersions) {
                if (totalBytes <= config_.maxTotalVersionsBytes) break;
                
                try {
                    fs::remove(version.versionPath);
                    totalBytes -= version.size;
                    pruned++;
                } catch (...) {}
            }
        }
    }
    
    return pruned;
}

size_t FileVersionManager::getTotalVersionStorageBytes() const {
    namespace fs = std::filesystem;
    
    size_t total = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(versionStoragePath_)) {
            if (entry.is_regular_file()) {
                total += entry.file_size();
            }
        }
    } catch (...) {}
    
    return total;
}

size_t FileVersionManager::getVersionCount(const std::string& filePath) const {
    return getVersions(filePath).size();
}

std::map<std::string, size_t> FileVersionManager::getVersionedFiles() const {
    namespace fs = std::filesystem;
    std::map<std::string, size_t> result;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        for (const auto& entry : fs::directory_iterator(versionStoragePath_)) {
            if (entry.is_directory()) {
                std::vector<FileVersion> versions;
                loadMetadata(entry.path().string(), versions);
                if (!versions.empty()) {
                    result[versions[0].filePath] = versions.size();
                }
            }
        }
    } catch (...) {}
    
    return result;
}

bool FileVersionManager::isExcluded(const std::string& filePath) const {
    std::filesystem::path p(filePath);
    std::string filename = p.filename().string();
    
    // Check if in version storage directory
    if (filePath.find(config_.versionStoragePath) != std::string::npos) {
        return true;
    }
    
    // Check exclusion patterns
    for (const auto& pattern : config_.excludePatterns) {
        if (matchesPattern(filename, pattern)) {
            return true;
        }
    }
    
    return false;
}

void FileVersionManager::setConfig(const VersioningConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    pruneVersions();  // Apply new limits
}

// Private helpers

std::string FileVersionManager::getVersionDirForFile(const std::string& filePath) const {
    std::string pathHash = hashPath(filePath);
    return versionStoragePath_ + "/file_" + pathHash;
}

std::string FileVersionManager::generateVersionFilename(const std::string& originalPath, uint64_t timestamp) const {
    std::filesystem::path p(originalPath);
    return "v_" + std::to_string(timestamp) + p.extension().string();
}

std::string FileVersionManager::hashPath(const std::string& path) const {
    // Simple hash for path - use first 16 chars of SHA256
    return SHA256::hash(path).substr(0, 16);
}

bool FileVersionManager::matchesPattern(const std::string& filename, const std::string& pattern) const {
    // Convert glob pattern to regex
    std::string regexPattern;
    for (char c : pattern) {
        switch (c) {
            case '*': regexPattern += ".*"; break;
            case '?': regexPattern += "."; break;
            case '.': regexPattern += "\\."; break;
            default: regexPattern += c;
        }
    }
    
    try {
        std::regex rx(regexPattern, std::regex::icase);
        return std::regex_match(filename, rx);
    } catch (...) {
        return false;
    }
}

void FileVersionManager::loadMetadata(const std::string& versionDir, std::vector<FileVersion>& versions) const {
    namespace fs = std::filesystem;
    
    std::string metadataPath = versionDir + "/metadata.json";
    if (!fs::exists(metadataPath)) {
        return;
    }
    
    try {
        std::ifstream file(metadataPath);
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();
        
        // Simple JSON array parsing
        size_t pos = 0;
        while ((pos = content.find("{\"versionId\":", pos)) != std::string::npos) {
            size_t end = content.find("},", pos);
            if (end == std::string::npos) {
                end = content.find("}]", pos);
            }
            if (end == std::string::npos) break;
            
            std::string obj = content.substr(pos, end - pos + 1);
            
            FileVersion v;
            v.versionId = extractJsonNumber(obj, "versionId");
            v.filePath = extractJsonString(obj, "filePath");
            v.versionPath = extractJsonString(obj, "versionPath");
            v.hash = extractJsonString(obj, "hash");
            v.peerId = extractJsonString(obj, "peerId");
            v.timestamp = extractJsonNumber(obj, "timestamp");
            v.size = extractJsonNumber(obj, "size");
            v.changeType = extractJsonString(obj, "changeType");
            v.comment = extractJsonString(obj, "comment");
            
            if (v.versionId > 0 && fs::exists(v.versionPath)) {
                versions.push_back(v);
            }
            
            pos = end + 1;
        }
    } catch (const std::exception& e) {
        Logger::instance().warn("Failed to load version metadata: " + std::string(e.what()), "FileVersionManager");
    }
}

void FileVersionManager::saveMetadata(const std::string& versionDir, const std::vector<FileVersion>& versions) {
    std::string metadataPath = versionDir + "/metadata.json";
    
    try {
        std::ofstream file(metadataPath);
        file << "[\n";
        
        for (size_t i = 0; i < versions.size(); ++i) {
            const auto& v = versions[i];
            file << "  {";
            file << "\"versionId\":" << v.versionId << ",";
            file << "\"filePath\":\"" << escapeJson(v.filePath) << "\",";
            file << "\"versionPath\":\"" << escapeJson(v.versionPath) << "\",";
            file << "\"hash\":\"" << v.hash << "\",";
            file << "\"peerId\":\"" << escapeJson(v.peerId) << "\",";
            file << "\"timestamp\":" << v.timestamp << ",";
            file << "\"size\":" << v.size << ",";
            file << "\"changeType\":\"" << escapeJson(v.changeType) << "\",";
            file << "\"comment\":\"" << escapeJson(v.comment) << "\"";
            file << "}";
            if (i < versions.size() - 1) file << ",";
            file << "\n";
        }
        
        file << "]\n";
    } catch (const std::exception& e) {
        Logger::instance().error("Failed to save version metadata: " + std::string(e.what()), "FileVersionManager");
    }
}

uint64_t FileVersionManager::generateVersionId() const {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 9999);
    
    return ms * 10000 + dis(gen);
}

void FileVersionManager::ensureVersionDirectory(const std::string& versionDir) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(versionDir)) {
        fs::create_directories(versionDir);
    }
}

} // namespace SentinelFS
