#include "selective_sync.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <fnmatch.h>
#include <regex>
#include <filesystem>
#include <chrono>

SelectiveSyncManager::SelectiveSyncManager()
    : defaultPriority(SyncPriority::NORMAL), maxSyncFileSize(0),
      syncedFiles(0), totalSyncAttempts(0) {
    lastCacheClear = std::chrono::steady_clock::now();
}

SelectiveSyncManager::~SelectiveSyncManager() {
    // Destructor
}

void SelectiveSyncManager::addSyncRule(const SyncRule& rule) {
    std::lock_guard<std::mutex> lock(rulesMutex);
    
    if (!isValidRule(rule)) {
        return;
    }
    
    // Check if rule already exists
    auto it = std::find_if(syncRules.begin(), syncRules.end(),
                          [&rule](const SyncRule& r) { return r.pattern == rule.pattern; });
    
    if (it != syncRules.end()) {
        *it = rule;  // Update existing rule
    } else {
        syncRules.push_back(rule);
    }
    
    clearCacheIfExpired();
}

void SelectiveSyncManager::removeSyncRule(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(rulesMutex);
    
    syncRules.erase(
        std::remove_if(syncRules.begin(), syncRules.end(),
                      [&pattern](const SyncRule& r) { return r.pattern == pattern; }),
        syncRules.end());
    
    clearCacheIfExpired();
}

void SelectiveSyncManager::clearSyncRules() {
    std::lock_guard<std::mutex> lock(rulesMutex);
    syncRules.clear();
    clearCacheIfExpired();
}

std::vector<SyncRule> SelectiveSyncManager::getSyncRules() const {
    std::lock_guard<std::mutex> lock(rulesMutex);
    return syncRules;
}

bool SelectiveSyncManager::shouldSyncFile(const std::string& filePath, size_t fileSize) const {
    std::lock_guard<std::mutex> lock(rulesMutex);
    
    // Check cache first
    auto cacheIt = patternMatchCache.find(filePath);
    if (cacheIt != patternMatchCache.end()) {
        return cacheIt->second;
    }
    
    // Default behavior: sync everything
    bool shouldSync = true;
    SyncPriority highestPriority = defaultPriority;
    
    // Check all rules
    for (const auto& rule : syncRules) {
        if (matchesPattern(filePath, rule.pattern)) {
            // Check file size limit
            if (rule.maxSize > 0 && fileSize > rule.maxSize) {
                continue;  // Skip this rule due to size limit
            }
            
            // Check active hours
            if (!isWithinActiveHours(rule)) {
                continue;  // Skip this rule due to time restrictions
            }
            
            // Apply rule
            if (rule.include) {
                shouldSync = true;
                // Update highest priority
                if (static_cast<int>(rule.priority) > static_cast<int>(highestPriority)) {
                    highestPriority = rule.priority;
                }
            } else {
                shouldSync = false;
            }
        }
    }
    
    // Cache result
    {
        std::lock_guard<std::mutex> cacheLock(cacheMutex);
        patternMatchCache[filePath] = shouldSync;
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> statsLock(statsMutex);
        totalSyncAttempts++;
        if (shouldSync) {
            syncedFiles++;
        }
    }
    
    return shouldSync;
}

SyncPriority SelectiveSyncManager::getFilePriority(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(rulesMutex);
    
    // Check cache first
    auto cacheIt = priorityCache.find(filePath);
    if (cacheIt != priorityCache.end()) {
        return cacheIt->second;
    }
    
    // Default priority
    SyncPriority priority = defaultPriority;
    
    // Check all rules
    for (const auto& rule : syncRules) {
        if (matchesPattern(filePath, rule.pattern)) {
            if (static_cast<int>(rule.priority) > static_cast<int>(priority)) {
                priority = rule.priority;
            }
        }
    }
    
    // Cache result
    {
        std::lock_guard<std::mutex> cacheLock(cacheMutex);
        priorityCache[filePath] = priority;
    }
    
    return priority;
}

std::vector<SyncRule> SelectiveSyncManager::getMatchingRules(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(rulesMutex);
    
    std::vector<SyncRule> matchingRules;
    
    for (const auto& rule : syncRules) {
        if (matchesPattern(filePath, rule.pattern)) {
            matchingRules.push_back(rule);
        }
    }
    
    return matchingRules;
}

void SelectiveSyncManager::addConflictResolutionRule(const std::string& pattern, 
                                                    ConflictResolutionStrategy strategy) {
    std::lock_guard<std::mutex> lock(conflictMutex);
    conflictResolutionRules[pattern] = strategy;
}

ConflictResolutionStrategy SelectiveSyncManager::getConflictResolutionStrategy(
    const std::string& filePath) const {
    
    std::lock_guard<std::mutex> lock(conflictMutex);
    
    // Check for specific pattern matches
    for (const auto& rule : conflictResolutionRules) {
        if (matchesPattern(filePath, rule.first)) {
            return rule.second;
        }
    }
    
    // Default to latest wins
    return ConflictResolutionStrategy::LATEST_WINS;
}

void SelectiveSyncManager::setDefaultPriority(SyncPriority priority) {
    std::lock_guard<std::mutex> lock(rulesMutex);
    defaultPriority = priority;
    clearCacheIfExpired();
}

SyncPriority SelectiveSyncManager::getDefaultPriority() const {
    std::lock_guard<std::mutex> lock(rulesMutex);
    return defaultPriority;
}

void SelectiveSyncManager::setFileTypePriority(const std::string& fileType, SyncPriority priority) {
    std::lock_guard<std::mutex> lock(rulesMutex);
    
    // Create a pattern rule for this file type
    SyncRule rule("*." + fileType, priority);
    addSyncRule(rule);
}

SyncPriority SelectiveSyncManager::getFileTypePriority(const std::string& fileType) const {
    std::lock_guard<std::mutex> lock(rulesMutex);
    
    // Look for a rule that matches this file type
    std::string pattern = "*." + fileType;
    
    for (const auto& rule : syncRules) {
        if (rule.pattern == pattern) {
            return rule.priority;
        }
    }
    
    return defaultPriority;
}

void SelectiveSyncManager::setSyncHours(const std::vector<int>& hours) {
    std::lock_guard<std::mutex> lock(timeMutex);
    syncHours = hours;
}

std::vector<int> SelectiveSyncManager::getSyncHours() const {
    std::lock_guard<std::mutex> lock(timeMutex);
    return syncHours;
}

bool SelectiveSyncManager::isActiveHour() const {
    std::lock_guard<std::mutex> lock(timeMutex);
    
    if (syncHours.empty()) {
        return true;  // No time restrictions
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    tm* timeinfo = localtime(&time_t);
    
    int currentHour = timeinfo->tm_hour;
    
    return std::find(syncHours.begin(), syncHours.end(), currentHour) != syncHours.end();
}

void SelectiveSyncManager::setMaxSyncFileSize(size_t maxSize) {
    std::lock_guard<std::mutex> lock(sizeMutex);
    maxSyncFileSize = maxSize;
}

size_t SelectiveSyncManager::getMaxSyncFileSize() const {
    std::lock_guard<std::mutex> lock(sizeMutex);
    return maxSyncFileSize;
}

void SelectiveSyncManager::addFileTag(const std::string& filePath, const std::string& tag) {
    std::lock_guard<std::mutex> lock(tagsMutex);
    fileTags[filePath].insert(tag);
}

void SelectiveSyncManager::removeFileTag(const std::string& filePath, const std::string& tag) {
    std::lock_guard<std::mutex> lock(tagsMutex);
    auto it = fileTags.find(filePath);
    if (it != fileTags.end()) {
        it->second.erase(tag);
        if (it->second.empty()) {
            fileTags.erase(it);
        }
    }
}

bool SelectiveSyncManager::hasTag(const std::string& filePath, const std::string& tag) const {
    std::lock_guard<std::mutex> lock(tagsMutex);
    auto it = fileTags.find(filePath);
    if (it != fileTags.end()) {
        return it->second.find(tag) != it->second.end();
    }
    return false;
}

std::set<std::string> SelectiveSyncManager::getFileTags(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(tagsMutex);
    auto it = fileTags.find(filePath);
    if (it != fileTags.end()) {
        return it->second;
    }
    return {};
}

bool SelectiveSyncManager::matchesPattern(const std::string& filePath, const std::string& pattern) const {
    if (isGlobPattern(pattern)) {
        return matchesGlobPattern(filePath, pattern);
    } else if (isRegexPattern(pattern)) {
        return matchesRegexPattern(filePath, pattern);
    } else {
        // Literal pattern match
        return filePath.find(pattern) != std::string::npos;
    }
}

bool SelectiveSyncManager::isGlobPattern(const std::string& pattern) const {
    return pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos;
}

bool SelectiveSyncManager::isRegexPattern(const std::string& pattern) const {
    return pattern.size() > 2 && pattern.front() == '/' && pattern.back() == '/';
}

size_t SelectiveSyncManager::getRuleCount() const {
    std::lock_guard<std::mutex> lock(rulesMutex);
    return syncRules.size();
}

size_t SelectiveSyncManager::getSyncedFileCount() const {
    std::lock_guard<std::mutex> lock(statsMutex);
    return syncedFiles;
}

double SelectiveSyncManager::getSyncEfficiency() const {
    std::lock_guard<std::mutex> lock(statsMutex);
    
    if (totalSyncAttempts == 0) {
        return 1.0;  // Perfect efficiency (no transfers yet)
    }
    
    return static_cast<double>(syncedFiles) / static_cast<double>(totalSyncAttempts);
}

bool SelectiveSyncManager::isValidRule(const SyncRule& rule) const {
    return !rule.pattern.empty();
}

std::string SelectiveSyncManager::validatePattern(const std::string& pattern) const {
    // This would validate that the pattern is syntactically correct
    // For now, return empty string (no error)
    return "";
}

bool SelectiveSyncManager::matchesGlobPattern(const std::string& filePath, const std::string& pattern) const {
    return fnmatch(pattern.c_str(), filePath.c_str(), 0) == 0;
}

bool SelectiveSyncManager::matchesRegexPattern(const std::string& filePath, const std::string& pattern) const {
    try {
        // Remove the leading and trailing slashes
        std::string regexPattern = pattern.substr(1, pattern.length() - 2);
        std::regex re(regexPattern);
        return std::regex_search(filePath, re);
    } catch (...) {
        return false;  // Invalid regex
    }
}

SyncPriority SelectiveSyncManager::getHighestPriority(const std::vector<SyncRule>& rules) const {
    if (rules.empty()) {
        return defaultPriority;
    }
    
    return std::max_element(rules.begin(), rules.end(),
                            [](const SyncRule& a, const SyncRule& b) {
                                return static_cast<int>(a.priority) < static_cast<int>(b.priority);
                            })->priority;
}

bool SelectiveSyncManager::isWithinActiveHours(const SyncRule& rule) const {
    // If no active hours specified, always active
    if (rule.activeHours.count() == 0) {
        return true;
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    tm* timeinfo = localtime(&time_t);
    
    int currentHour = timeinfo->tm_hour;
    
    // This is a simplified check - in reality, you'd check against rule.activeHours
    return currentHour >= 0 && currentHour <= 23;  // Always true for now
}

std::string SelectiveSyncManager::getFileExtension(const std::string& filePath) const {
    size_t lastDot = filePath.find_last_of('.');
    if (lastDot != std::string::npos) {
        return filePath.substr(lastDot + 1);
    }
    return "";
}

std::string SelectiveSyncManager::getFileName(const std::string& filePath) const {
    size_t lastSlash = filePath.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        return filePath.substr(lastSlash + 1);
    }
    return filePath;
}

void SelectiveSyncManager::clearCacheIfExpired() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(now - lastCacheClear);
    
    if (elapsed.count() > 5) {  // Clear cache every 5 minutes
        patternMatchCache.clear();
        priorityCache.clear();
        lastCacheClear = now;
    }
}