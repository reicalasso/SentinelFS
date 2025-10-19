#include "version_history.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <zlib.h>
#include <openssl/sha.h>
#include <cstring>

VersionHistoryManager::VersionHistoryManager(const VersionPolicy& policy)
    : policy(policy), versionStoragePath(".sentinelfs/versions"),
      automaticCleanupEnabled(true) {
    
    // Create version storage directory
    std::filesystem::create_directories(versionStoragePath);
}

VersionHistoryManager::~VersionHistoryManager() {
    stop();
}

void VersionHistoryManager::start() {
    if (running.exchange(true)) {
        return;  // Already running
    }
    
    if (automaticCleanupEnabled) {
        cleanupThread = std::thread(&VersionHistoryManager::cleanupLoop, this);
    }
}

void VersionHistoryManager::stop() {
    if (running.exchange(false)) {
        if (cleanupThread.joinable()) {
            cleanupThread.join();
        }
    }
}

FileVersion VersionHistoryManager::createFileVersion(const std::string& filePath, 
                                                     const std::string& commitMessage,
                                                     const std::string& modifiedBy) {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    // Create new version
    FileVersion version(filePath);
    version.versionId = generateVersionId(filePath);
    version.commitMessage = commitMessage;
    version.modifiedBy = modifiedBy;
    version.checksum = calculateVersionChecksum(filePath);
    
    // Get file size
    try {
        version.fileSize = std::filesystem::file_size(filePath);
    } catch (...) {
        version.fileSize = 0;
    }
    
    version.lastModified = std::chrono::system_clock::now();
    
    // Store version file
    if (storeVersionFile(version)) {
        // Add to indexes
        fileVersions[filePath].push_back(version);
        versionIndex[version.versionId] = version;
        
        // Notify callbacks
        notifyVersionCreated(version);
        
        return version;
    }
    
    return FileVersion();  // Return empty version on failure
}

bool VersionHistoryManager::deleteFileVersion(const std::string& versionId) {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it == versionIndex.end()) {
        return false;
    }
    
    FileVersion version = it->second;
    
    // Remove version file
    std::string versionPath = getVersionStoragePath(versionId);
    bool fileDeleted = std::filesystem::remove(versionPath);
    
    // Remove compressed version if exists
    std::string compressedPath = versionPath + ".gz";
    if (std::filesystem::exists(compressedPath)) {
        std::filesystem::remove(compressedPath);
    }
    
    // Remove from indexes
    versionIndex.erase(versionId);
    
    // Remove from file versions list
    auto fileIt = fileVersions.find(version.filePath);
    if (fileIt != fileVersions.end()) {
        fileIt->second.erase(
            std::remove_if(fileIt->second.begin(), fileIt->second.end(),
                          [&versionId](const FileVersion& v) { return v.versionId == versionId; }),
            fileIt->second.end());
    }
    
    // Remove from tag index
    for (auto& tagPair : tagIndex) {
        tagPair.second.erase(versionId);
    }
    
    // Notify callbacks
    notifyVersionDeleted(versionId);
    
    return fileDeleted;
}

bool VersionHistoryManager::restoreFileVersion(const std::string& versionId, 
                                               const std::string& restorePath) {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it == versionIndex.end()) {
        return false;
    }
    
    FileVersion version = it->second;
    std::string targetPath = restorePath.empty() ? version.filePath : restorePath;
    
    // Retrieve version file
    if (retrieveVersionFile(versionId, targetPath)) {
        // Notify callbacks
        notifyVersionRestored(version, targetPath);
        return true;
    }
    
    return false;
}

std::vector<FileVersion> VersionHistoryManager::getFileVersions(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = fileVersions.find(filePath);
    if (it != fileVersions.end()) {
        return it->second;
    }
    
    return {};
}

FileVersion VersionHistoryManager::getLatestVersion(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = fileVersions.find(filePath);
    if (it != fileVersions.end() && !it->second.empty()) {
        // Return most recent version
        return *std::max_element(it->second.begin(), it->second.end(),
                                [](const FileVersion& a, const FileVersion& b) {
                                    return a.createdAt < b.createdAt;
                                });
    }
    
    return FileVersion();
}

FileVersion VersionHistoryManager::getVersionById(const std::string& versionId) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it != versionIndex.end()) {
        return it->second;
    }
    
    return FileVersion();
}

std::vector<FileVersion> VersionHistoryManager::getVersionsByDate(
    const std::chrono::system_clock::time_point& startDate,
    const std::chrono::system_clock::time_point& endDate) const {
    
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    std::vector<FileVersion> result;
    
    for (const auto& filePair : fileVersions) {
        for (const auto& version : filePair.second) {
            if (version.createdAt >= startDate && version.createdAt <= endDate) {
                result.push_back(version);
            }
        }
    }
    
    // Sort by creation time
    std::sort(result.begin(), result.end(),
             [](const FileVersion& a, const FileVersion& b) {
                 return a.createdAt < b.createdAt;
             });
    
    return result;
}

bool VersionHistoryManager::compareVersions(const std::string& versionId1, 
                                           const std::string& versionId2) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it1 = versionIndex.find(versionId1);
    auto it2 = versionIndex.find(versionId2);
    
    if (it1 == versionIndex.end() || it2 == versionIndex.end()) {
        return false;
    }
    
    // Compare checksums
    return it1->second.checksum == it2->second.checksum;
}

std::vector<std::pair<size_t, size_t>> VersionHistoryManager::getDiff(
    const std::string& versionId1, 
    const std::string& versionId2) const {
    
    // This would implement a simple diff algorithm
    // For now, return empty vector as placeholder
    return {};
}

void VersionHistoryManager::addVersionTag(const std::string& versionId, const std::string& tag) {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it != versionIndex.end()) {
        it->second.tags.insert(tag);
        tagIndex[tag].insert(versionId);
    }
}

void VersionHistoryManager::removeVersionTag(const std::string& versionId, const std::string& tag) {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it != versionIndex.end()) {
        it->second.tags.erase(tag);
        tagIndex[tag].erase(versionId);
    }
}

std::set<std::string> VersionHistoryManager::getVersionTags(const std::string& versionId) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it != versionIndex.end()) {
        return it->second.tags;
    }
    
    return {};
}

std::vector<FileVersion> VersionHistoryManager::getVersionsWithTag(const std::string& tag) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    std::vector<FileVersion> result;
    
    auto it = tagIndex.find(tag);
    if (it != tagIndex.end()) {
        for (const auto& versionId : it->second) {
            auto versionIt = versionIndex.find(versionId);
            if (versionIt != versionIndex.end()) {
                result.push_back(versionIt->second);
            }
        }
    }
    
    return result;
}

void VersionHistoryManager::setVersionPolicy(const VersionPolicy& policy) {
    std::lock_guard<std::mutex> lock(versionsMutex);
    this->policy = policy;
    
    // Apply new policy immediately
    enforceVersionPolicy();
}

VersionPolicy VersionHistoryManager::getVersionPolicy() const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    return policy;
}

bool VersionHistoryManager::isVersionImportant(const FileVersion& version) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    // Check if file matches important patterns
    for (const auto& pattern : policy.importantFilePatterns) {
        if (version.filePath.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    // Check if version has important tags
    for (const auto& tag : version.tags) {
        if (tag == "important" || tag == "critical") {
            return true;
        }
    }
    
    return false;
}

void VersionHistoryManager::enableAutomaticCleanup(bool enable) {
    automaticCleanupEnabled = enable;
    
    if (enable && !running.load()) {
        start();
    } else if (!enable && running.load()) {
        stop();
    }
}

void VersionHistoryManager::cleanupOldVersions() {
    if (policy.maxAge.count() > 0) {
        cleanupByAge();
    }
    
    cleanupByVersionCount();
}

void VersionHistoryManager::cleanupByVersionCount() {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    for (auto& filePair : fileVersions) {
        auto& versions = filePair.second;
        
        // Sort by creation time (oldest first)
        std::sort(versions.begin(), versions.end(),
                 [](const FileVersion& a, const FileVersion& b) {
                     return a.createdAt < b.createdAt;
                 });
        
        // Check if we have too many versions
        if (versions.size() > policy.maxVersions) {
            size_t toRemove = versions.size() - policy.maxVersions;
            
            // Remove oldest versions (unless they're important)
            for (size_t i = 0; i < toRemove; ++i) {
                if (i < versions.size()) {
                    const FileVersion& version = versions[i];
                    
                    // Don't remove important versions
                    if (!isVersionImportant(version)) {
                        deleteFileVersion(version.versionId);
                    }
                }
            }
        }
    }
}

void VersionHistoryManager::cleanupByAge() {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto now = std::chrono::system_clock::now();
    
    for (auto it = fileVersions.begin(); it != fileVersions.end();) {
        auto& versions = it->second;
        
        for (auto versionIt = versions.begin(); versionIt != versions.end();) {
            auto age = now - versionIt->createdAt;
            
            // Check if version is too old
            if (age > policy.maxAge && !isVersionImportant(*versionIt)) {
                deleteFileVersion(versionIt->versionId);
                versionIt = versions.erase(versionIt);
            } else {
                ++versionIt;
            }
        }
        
        // Remove file entry if no versions left
        if (versions.empty()) {
            it = fileVersions.erase(it);
        } else {
            ++it;
        }
    }
}

bool VersionHistoryManager::compressVersion(const std::string& versionId) {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it == versionIndex.end()) {
        return false;
    }
    
    FileVersion& version = it->second;
    
    // Check if already compressed
    if (version.compressed) {
        return true;
    }
    
    std::string originalPath = getVersionStoragePath(versionId);
    std::string compressedPath = originalPath + ".gz";
    
    // Open files
    std::ifstream inputFile(originalPath, std::ios::binary);
    std::ofstream outputFile(compressedPath, std::ios::binary);
    
    if (!inputFile || !outputFile) {
        return false;
    }
    
    // Compress using gzip
    gzFile gzFile = gzopen(compressedPath.c_str(), "wb");
    if (!gzFile) {
        return false;
    }
    
    char buffer[8192];
    while (inputFile.read(buffer, sizeof(buffer)) || inputFile.gcount() > 0) {
        size_t bytesRead = inputFile.gcount();
        if (gzwrite(gzFile, buffer, bytesRead) != static_cast<int>(bytesRead)) {
            gzclose(gzFile);
            std::filesystem::remove(compressedPath);
            return false;
        }
    }
    
    gzclose(gzFile);
    
    // Mark as compressed
    version.compressed = true;
    version.compressionAlgorithm = "gzip";
    
    // Remove original file to save space
    std::filesystem::remove(originalPath);
    
    return true;
}

bool VersionHistoryManager::decompressVersion(const std::string& versionId) {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it == versionIndex.end()) {
        return false;
    }
    
    FileVersion& version = it->second;
    
    // Check if already decompressed
    if (!version.compressed) {
        return true;
    }
    
    std::string compressedPath = getVersionStoragePath(versionId) + ".gz";
    std::string decompressedPath = getVersionStoragePath(versionId);
    
    // Open files
    gzFile gzFile = gzopen(compressedPath.c_str(), "rb");
    std::ofstream outputFile(decompressedPath, std::ios::binary);
    
    if (!gzFile || !outputFile) {
        return false;
    }
    
    // Decompress
    char buffer[8192];
    int bytesRead;
    while ((bytesRead = gzread(gzFile, buffer, sizeof(buffer))) > 0) {
        outputFile.write(buffer, bytesRead);
    }
    
    gzclose(gzFile);
    
    // Mark as decompressed
    version.compressed = false;
    version.compressionAlgorithm = "";
    
    // Remove compressed file
    std::filesystem::remove(compressedPath);
    
    return true;
}

bool VersionHistoryManager::isVersionCompressed(const std::string& versionId) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it != versionIndex.end()) {
        return it->second.compressed;
    }
    
    return false;
}

bool VersionHistoryManager::exportVersion(const std::string& versionId, 
                                          const std::string& exportPath) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    // Simply copy the version file to export path
    std::string versionPath = getVersionStoragePath(versionId);
    
    try {
        std::filesystem::copy_file(versionPath, exportPath,
                                   std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

FileVersion VersionHistoryManager::importVersion(const std::string& importPath) {
    // This would import a version from external source
    // For now, return empty version
    return FileVersion();
}

std::string VersionHistoryManager::calculateVersionChecksum(const std::string& filePath) const {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return "";
    }
    
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    
    char buffer[8192];
    while (file.good()) {
        file.read(buffer, sizeof(buffer));
        size_t bytesRead = file.gcount();
        if (bytesRead > 0) {
            SHA256_Update(&sha256, buffer, bytesRead);
        }
    }
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &sha256);
    
    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

size_t VersionHistoryManager::getVersionFileSize(const std::string& versionId) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it != versionIndex.end()) {
        return it->second.fileSize;
    }
    
    return 0;
}

std::chrono::system_clock::time_point VersionHistoryManager::getVersionCreationTime(const std::string& versionId) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = versionIndex.find(versionId);
    if (it != versionIndex.end()) {
        return it->second.createdAt;
    }
    
    return std::chrono::system_clock::time_point::min();
}

std::vector<FileVersion> VersionHistoryManager::getRecentVersions(size_t limit) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    std::vector<FileVersion> allVersions;
    
    // Collect all versions
    for (const auto& filePair : fileVersions) {
        allVersions.insert(allVersions.end(), filePair.second.begin(), filePair.second.end());
    }
    
    // Sort by creation time (newest first)
    std::sort(allVersions.begin(), allVersions.end(),
             [](const FileVersion& a, const FileVersion& b) {
                 return a.createdAt > b.createdAt;
             });
    
    // Limit results
    if (allVersions.size() > limit) {
        allVersions.resize(limit);
    }
    
    return allVersions;
}

std::vector<FileVersion> VersionHistoryManager::searchVersions(const std::string& query) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    std::vector<FileVersion> results;
    
    // Search in file paths and commit messages
    for (const auto& filePair : fileVersions) {
        for (const auto& version : filePair.second) {
            if (version.filePath.find(query) != std::string::npos ||
                version.commitMessage.find(query) != std::string::npos) {
                results.push_back(version);
            }
        }
    }
    
    return results;
}

void VersionHistoryManager::deleteFileVersionRange(const std::string& filePath,
                                                   const std::chrono::system_clock::time_point& beforeDate) {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = fileVersions.find(filePath);
    if (it != fileVersions.end()) {
        auto& versions = it->second;
        
        for (auto versionIt = versions.begin(); versionIt != versions.end();) {
            if (versionIt->createdAt < beforeDate) {
                deleteFileVersion(versionIt->versionId);
                versionIt = versions.erase(versionIt);
            } else {
                ++versionIt;
            }
        }
    }
}

size_t VersionHistoryManager::getTotalVersions() const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    size_t total = 0;
    for (const auto& filePair : fileVersions) {
        total += filePair.second.size();
    }
    
    return total;
}

size_t VersionHistoryManager::getVersionsForFile(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    auto it = fileVersions.find(filePath);
    if (it != fileVersions.end()) {
        return it->second.size();
    }
    
    return 0;
}

std::map<std::string, size_t> VersionHistoryManager::getVersionStatistics() const {
    std::lock_guard<std::mutex> lock(versionsMutex);
    
    std::map<std::string, size_t> stats;
    
    // Total versions
    stats["total_versions"] = getTotalVersions();
    
    // Files with versions
    stats["files_with_versions"] = fileVersions.size();
    
    // Average versions per file
    if (!fileVersions.empty()) {
        stats["avg_versions_per_file"] = getTotalVersions() / fileVersions.size();
    }
    
    return stats;
}

void VersionHistoryManager::setVersionCreatedCallback(VersionCreatedCallback callback) {
    versionCreatedCallback = callback;
}

void VersionHistoryManager::setVersionDeletedCallback(VersionDeletedCallback callback) {
    versionDeletedCallback = callback;
}

void VersionHistoryManager::setVersionRestoredCallback(VersionRestoredCallback callback) {
    versionRestoredCallback = callback;
}

void VersionHistoryManager::cleanupLoop() {
    const auto cleanupInterval = std::chrono::hours(1);  // Run cleanup every hour
    
    while (running.load()) {
        cleanupOldVersions();
        std::this_thread::sleep_for(cleanupInterval);
    }
}

void VersionHistoryManager::enforceVersionPolicy() {
    if (policy.maxVersions > 0) {
        cleanupByVersionCount();
    }
    
    if (policy.maxAge.count() > 0) {
        cleanupByAge();
    }
    
    if (policy.compressOldVersions) {
        // Compress old versions
        auto cutoffTime = std::chrono::system_clock::now() - std::chrono::hours(24);  // Compress versions older than 1 day
        
        std::lock_guard<std::mutex> lock(versionsMutex);
        for (const auto& filePair : fileVersions) {
            for (const auto& version : filePair.second) {
                if (version.createdAt < cutoffTime && !version.compressed) {
                    compressVersion(version.versionId);
                }
            }
        }
    }
}

std::string VersionHistoryManager::generateVersionId(const std::string& filePath) const {
    // Generate unique version ID
    auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::string id = filePath + "_" + std::to_string(now);
    
    // Simple hash for shorter ID
    std::hash<std::string> hasher;
    size_t hash = hasher(id);
    
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

std::string VersionHistoryManager::getVersionStoragePath(const std::string& versionId) const {
    return versionStoragePath + "/" + versionId;
}

bool VersionHistoryManager::storeVersionFile(const FileVersion& version) const {
    std::string versionPath = getVersionStoragePath(version.versionId);
    
    try {
        // Copy original file to version storage
        std::filesystem::copy_file(version.filePath, versionPath,
                                  std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool VersionHistoryManager::retrieveVersionFile(const std::string& versionId, 
                                                const std::string& destination) const {
    std::string versionPath = getVersionStoragePath(versionId);
    std::string compressedPath = versionPath + ".gz";
    
    try {
        // Check if compressed version exists
        if (std::filesystem::exists(compressedPath)) {
            // Decompress and copy
            gzFile gzFile = gzopen(compressedPath.c_str(), "rb");
            std::ofstream outputFile(destination, std::ios::binary);
            
            if (!gzFile || !outputFile) {
                return false;
            }
            
            char buffer[8192];
            int bytesRead;
            while ((bytesRead = gzread(gzFile, buffer, sizeof(buffer))) > 0) {
                outputFile.write(buffer, bytesRead);
            }
            
            gzclose(gzFile);
            return true;
        } else if (std::filesystem::exists(versionPath)) {
            // Copy uncompressed version
            std::filesystem::copy_file(versionPath, destination,
                                      std::filesystem::copy_options::overwrite_existing);
            return true;
        }
    } catch (...) {
        return false;
    }
    
    return false;
}

void VersionHistoryManager::notifyVersionCreated(const FileVersion& version) {
    if (versionCreatedCallback) {
        versionCreatedCallback(version);
    }
}

void VersionHistoryManager::notifyVersionDeleted(const std::string& versionId) {
    if (versionDeletedCallback) {
        versionDeletedCallback(versionId);
    }
}

void VersionHistoryManager::notifyVersionRestored(const FileVersion& version, 
                                                  const std::string& restorePath) {
    if (versionRestoredCallback) {
        versionRestoredCallback(version, restorePath);
    }
}

bool VersionHistoryManager::shouldKeepVersion(const FileVersion& version) const {
    // Keep important versions regardless of policy
    if (isVersionImportant(version)) {
        return true;
    }
    
    // Check age limit
    if (policy.maxAge.count() > 0) {
        auto now = std::chrono::system_clock::now();
        auto age = now - version.createdAt;
        if (age > policy.maxAge) {
            return false;
        }
    }
    
    return true;
}

std::vector<FileVersion> VersionHistoryManager::sortVersionsByDate(
    const std::vector<FileVersion>& versions) const {
    
    auto sorted = versions;
    std::sort(sorted.begin(), sorted.end(),
             [](const FileVersion& a, const FileVersion& b) {
                 return a.createdAt < b.createdAt;
             });
    return sorted;
}

bool VersionHistoryManager::isValidVersionId(const std::string& versionId) const {
    return !versionId.empty() && versionId.length() > 10;
}

std::string VersionHistoryManager::sanitizeFileName(const std::string& fileName) const {
    std::string sanitized = fileName;
    
    // Replace invalid characters
    std::replace(sanitized.begin(), sanitized.end(), '/', '_');
    std::replace(sanitized.begin(), sanitized.end(), '\\', '_');
    std::replace(sanitized.begin(), sanitized.end(), ':', '_');
    std::replace(sanitized.begin(), sanitized.end(), '*', '_');
    std::replace(sanitized.begin(), sanitized.end(), '?', '_');
    std::replace(sanitized.begin(), sanitized.end(), '"', '_');
    std::replace(sanitized.begin(), sanitized.end(), '<', '_');
    std::replace(sanitized.begin(), sanitized.end(), '>', '_');
    std::replace(sanitized.begin(), sanitized.end(), '|', '_');
    
    return sanitized;
}