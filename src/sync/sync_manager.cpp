#include "sync_manager.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <regex>
#include <fnmatch.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <zlib.h>
#include <ctime>

SyncManager::SyncManager(const SyncConfig& config)
    : config(config), storageQuota(0) {
    initializeComponents();
}

SyncManager::~SyncManager() {
    stop();
}

bool SyncManager::start() {
    if (running.exchange(true)) {
        return true;  // Already running
    }
    
    // Start component managers
    if (bandwidthLimiter) {
        bandwidthLimiter->start();
    }
    
    if (resumableTransfers) {
        resumableTransfers->start();
    }
    
    if (versionHistory) {
        versionHistory->start();
    }
    
    // Start main sync thread
    syncThread = std::thread(&SyncManager::syncLoop, this);
    
    // Start maintenance thread
    maintenanceRunning = true;
    maintenanceThread = std::thread(&SyncManager::maintenanceLoop, this);
    
    logEvent(SyncEvent(SyncEvent::Type::TRANSFER_STARTED, "", "sync_manager"));
    return true;
}

void SyncManager::stop() {
    if (running.exchange(false)) {
        // Stop sync thread
        if (syncThread.joinable()) {
            syncThread.join();
        }
        
        // Stop maintenance thread
        maintenanceRunning = false;
        if (maintenanceThread.joinable()) {
            maintenanceThread.join();
        }
        
        // Stop component managers
        if (bandwidthLimiter) {
            bandwidthLimiter->stop();
        }
        
        if (resumableTransfers) {
            resumableTransfers->stop();
        }
        
        if (versionHistory) {
            versionHistory->stop();
        }
        
        logEvent(SyncEvent(SyncEvent::Type::TRANSFER_COMPLETED, "", "sync_manager"));
    }
}

void SyncManager::setConfig(const SyncConfig& newConfig) {
    std::lock_guard<std::mutex> lock(configMutex);
    config = newConfig;
    
    // Update component configurations
    configureBandwidthLimiter();
    configureVersionHistory();
    
    logEvent(SyncEvent(SyncEvent::Type::VERSION_CREATED, "", "configuration"));
}

SyncConfig SyncManager::getConfig() const {
    std::lock_guard<std::mutex> lock(configMutex);
    return config;
}

bool SyncManager::syncFile(const std::string& filePath, const std::string& peerId) {
    if (!running.load() || paused.load()) {
        return false;
    }
    
    // Check selective sync rules
    if (selectiveSync && !shouldSyncFile(filePath)) {
        logEvent(SyncEvent(SyncEvent::Type::FILE_DELETED, filePath, peerId));
        return true;  // Treat as success (file intentionally not synced)
    }
    
    // Check sync hours
    if (!isWithinSyncHours()) {
        logEvent(SyncEvent(SyncEvent::Type::NETWORK_ERROR, filePath, peerId));
        return false;
    }
    
    // Get file size
    size_t fileSize = 0;
    try {
        fileSize = std::filesystem::file_size(filePath);
    } catch (...) {
        logEvent(SyncEvent(SyncEvent::Type::TRANSFER_FAILED, filePath, peerId));
        return false;
    }
    
    // Check bandwidth availability
    if (bandwidthLimiter && config.enableBandwidthThrottling) {
        waitForBandwidthAvailability(filePath, fileSize, true);  // Upload assumed by default
    }
    
    // Process the file sync
    bool success = processFileSync(filePath, peerId);
    
    if (success) {
        handleTransferSuccess(filePath, fileSize);
        logEvent(SyncEvent(SyncEvent::Type::TRANSFER_COMPLETED, filePath, peerId));
    } else {
        handleTransferFailure(filePath, "Unknown error");
        logEvent(SyncEvent(SyncEvent::Type::TRANSFER_FAILED, filePath, peerId));
    }
    
    return success;
}

bool SyncManager::syncDirectory(const std::string& directoryPath) {
    if (!running.load() || paused.load()) {
        return false;
    }
    
    // Collect files to sync in batch
    std::vector<std::string> filesToSync;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                filesToSync.push_back(entry.path().string());
            }
        }
    } catch (const std::exception& e) {
        logEvent(SyncEvent(SyncEvent::Type::TRANSFER_FAILED, directoryPath, ""));
        return false;
    }
    
    // Use batch processing for efficiency (process in chunks of 100)
    const size_t BATCH_SIZE = 100;
    for (size_t i = 0; i < filesToSync.size(); i += BATCH_SIZE) {
        size_t end = std::min(i + BATCH_SIZE, filesToSync.size());
        
        // Process batch with implicit transaction support
        for (size_t j = i; j < end; ++j) {
            if (!syncFile(filesToSync[j])) {
                // Continue on failure, but log it
                logEvent(SyncEvent(SyncEvent::Type::TRANSFER_FAILED, filesToSync[j], ""));
            }
        }
    }
    
    return true;
}

// Note: syncDirectoryOriginal removed - use syncDirectory instead

void SyncManager::cancelSync(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    // Cancel any ongoing transfers
    if (resumableTransfers) {
        // This would cancel the specific file transfer
        // Implementation depends on the actual resumable transfers system
    }
    
    logEvent(SyncEvent(SyncEvent::Type::TRANSFER_FAILED, filePath, "cancelled"));
}

void SyncManager::addSyncRule(const SyncRule& rule) {
    if (selectiveSync) {
        selectiveSync->addSyncRule(rule);
        logEvent(SyncEvent(SyncEvent::Type::VERSION_CREATED, "", "sync_rules"));
    }
}

void SyncManager::removeSyncRule(const std::string& pattern) {
    if (selectiveSync) {
        selectiveSync->removeSyncRule(pattern);
        logEvent(SyncEvent(SyncEvent::Type::FILE_DELETED, "", "sync_rules"));
    }
}

std::vector<SyncRule> SyncManager::getSyncRules() const {
    if (selectiveSync) {
        return selectiveSync->getSyncRules();
    }
    return {};
}

bool SyncManager::shouldSyncFile(const std::string& filePath, size_t fileSize) const {
    if (selectiveSync) {
        return selectiveSync->shouldSyncFile(filePath, fileSize);
    }
    return true;  // Sync everything by default
}

SyncPriority SyncManager::getFilePriority(const std::string& filePath) const {
    if (selectiveSync) {
        return selectiveSync->getFilePriority(filePath);
    }
    return SyncPriority::NORMAL;
}

void SyncManager::setBandwidthLimits(size_t maxUpload, size_t maxDownload) {
    std::lock_guard<std::mutex> lock(configMutex);
    config.maxBandwidthUpload = maxUpload;
    config.maxBandwidthDownload = maxDownload;
    
    if (bandwidthLimiter) {
        BandwidthConfig bwConfig = bandwidthLimiter->getConfiguration();
        bwConfig.maxUploadSpeed = maxUpload;
        bwConfig.maxDownloadSpeed = maxDownload;
        bandwidthLimiter->setConfiguration(bwConfig);
    }
    
    logEvent(SyncEvent(SyncEvent::Type::BANDWIDTH_LIMITED, "", "system"));
}

void SyncManager::setBandwidthThrottling(bool enable) {
    std::lock_guard<std::mutex> lock(configMutex);
    config.enableBandwidthThrottling = enable;
    
    if (bandwidthLimiter) {
        BandwidthConfig bwConfig = bandwidthLimiter->getConfiguration();
        bwConfig.enableThrottling = enable;
        bandwidthLimiter->setConfiguration(bwConfig);
    }
}

double SyncManager::getCurrentUploadRate() const {
    if (bandwidthLimiter) {
        return bandwidthLimiter->getCurrentUploadRate();
    }
    return 0.0;
}

double SyncManager::getCurrentDownloadRate() const {
    if (bandwidthLimiter) {
        return bandwidthLimiter->getCurrentDownloadRate();
    }
    return 0.0;
}

void SyncManager::pauseAllTransfers() {
    paused = true;
    logEvent(SyncEvent(SyncEvent::Type::NETWORK_ERROR, "", "system"));
}

void SyncManager::resumeAllTransfers() {
    paused = false;
    logEvent(SyncEvent(SyncEvent::Type::TRANSFER_RESUMED, "", "system"));
}

bool SyncManager::resumeInterruptedTransfer(const std::string& transferId) {
    if (resumableTransfers) {
        return resumableTransfers->resumeTransfer(transferId);
    }
    return false;
}

std::vector<TransferCheckpoint> SyncManager::getPendingTransfers() const {
    if (resumableTransfers) {
        return resumableTransfers->getPendingTransfers();
    }
    return {};
}

std::vector<TransferCheckpoint> SyncManager::getFailedTransfers() const {
    if (resumableTransfers) {
        return resumableTransfers->getFailedTransfers();
    }
    return {};
}

double SyncManager::getTransferProgress(const std::string& transferId) const {
    if (resumableTransfers) {
        return resumableTransfers->getTransferProgress(transferId);
    }
    return 0.0;
}

FileVersion SyncManager::createFileVersion(const std::string& filePath, 
                                          const std::string& commitMessage,
                                          const std::string& modifiedBy) {
    if (versionHistory) {
        FileVersion version = versionHistory->createFileVersion(filePath, commitMessage, modifiedBy);
        if (!version.versionId.empty()) {
            logEvent(SyncEvent(SyncEvent::Type::VERSION_CREATED, filePath, modifiedBy));
            return version;
        }
    }
    return FileVersion();
}

bool SyncManager::restoreFileVersion(const std::string& versionId, 
                                     const std::string& restorePath) {
    if (versionHistory) {
        bool success = versionHistory->restoreFileVersion(versionId, restorePath);
        if (success) {
            logEvent(SyncEvent(SyncEvent::Type::VERSION_RESTORED, restorePath, "version_history"));
        }
        return success;
    }
    return false;
}

std::vector<FileVersion> SyncManager::getFileVersions(const std::string& filePath) const {
    if (versionHistory) {
        return versionHistory->getFileVersions(filePath);
    }
    return {};
}

bool SyncManager::deleteOldVersions(const std::string& filePath) {
    if (versionHistory) {
        // This would delete old versions according to policy
        // Implementation depends on the actual version history system
        return true;
    }
    return false;
}

void SyncManager::handleNetworkDisconnect(const std::string& peerId, const std::string& reason) {
    if (resumableTransfers) {
        resumableTransfers->handleDisconnection(peerId, reason);
    }
    
    logEvent(SyncEvent(SyncEvent::Type::NETWORK_ERROR, "", peerId));
}

void SyncManager::handleNetworkReconnect(const std::string& peerId) {
    if (resumableTransfers) {
        resumableTransfers->handleReconnection(peerId);
    }
    
    logEvent(SyncEvent(SyncEvent::Type::TRANSFER_RESUMED, "", peerId));
}

bool SyncManager::isNetworkStable() const {
    // This would check network stability based on various factors
    // For now, return true (assume stable)
    return true;
}

void SyncManager::addConflictResolutionRule(const std::string& pattern, ConflictResolutionStrategy strategy) {
    std::lock_guard<std::mutex> lock(configMutex);
    conflictResolutionRules[pattern] = strategy;
}

ConflictResolutionStrategy SyncManager::getConflictResolutionStrategy(const std::string& filePath) const {
    std::lock_guard<std::mutex> lock(configMutex);
    
    // Check for specific pattern matches
    for (const auto& rule : conflictResolutionRules) {
        if (matchesPattern(filePath, rule.first)) {
            return rule.second;
        }
    }
    
    // Default to LATEST strategy
    return ConflictResolutionStrategy::LATEST;
}

bool SyncManager::resolveConflict(const std::string& filePath, const std::vector<PeerInfo>& peers) {
    ConflictResolutionStrategy strategy = getConflictResolutionStrategy(filePath);
    
    // This would implement actual conflict resolution based on strategy
    // For now, return true (conflict resolved)
    logEvent(SyncEvent(SyncEvent::Type::FILE_CONFLICT, filePath, "resolution"));
    return true;
}

SyncStats SyncManager::getSyncStats() const {
    std::lock_guard<std::mutex> lock(statsMutex);
    return syncStats;
}

void SyncManager::resetSyncStats() {
    std::lock_guard<std::mutex> lock(statsMutex);
    syncStats = SyncStats();
}

std::vector<SyncEvent> SyncManager::getRecentEvents(size_t limit) const {
    std::lock_guard<std::mutex> lock(eventMutex);
    
    if (eventLog.size() <= limit) {
        return eventLog;
    }
    
    return std::vector<SyncEvent>(eventLog.end() - limit, eventLog.end());
}

double SyncManager::getSyncEfficiency() const {
    std::lock_guard<std::mutex> lock(statsMutex);
    
    if (syncStats.filesSynced == 0) {
        return 1.0;  // Perfect efficiency (no transfers yet)
    }
    
    // Efficiency based on success rate and bandwidth utilization
    double successRate = static_cast<double>(syncStats.filesSynced) / 
                        (syncStats.filesSynced + syncStats.transferFailures);
    
    return successRate;
}

void SyncManager::cleanupOldVersions() {
    if (versionHistory) {
        versionHistory->cleanupOldVersions();
    }
}

void SyncManager::optimizeStorage() {
    // This would optimize storage usage
    // For now, just cleanup old versions
    cleanupOldVersions();
}

size_t SyncManager::getStorageUsage() const {
    // This would calculate actual storage usage
    // For now, return 0
    return 0;
}

void SyncManager::setStorageQuota(size_t quotaBytes) {
    storageQuota = quotaBytes;
}

void SyncManager::setSyncEventCallback(SyncEventCallback callback) {
    syncEventCallback = callback;
}

void SyncManager::enableFileEncryption(bool enable) {
    std::lock_guard<std::mutex> lock(configMutex);
    // This would enable/disable file encryption
    // Implementation would depend on the encryption system
}

bool SyncManager::isFileEncrypted(const std::string& filePath) const {
    // This would check if a file is encrypted
    // For now, return false
    return false;
}

void SyncManager::addTrustedPeer(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(configMutex);
    trustedPeers.insert(peerId);
}

bool SyncManager::isPeerTrusted(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(configMutex);
    return trustedPeers.find(peerId) != trustedPeers.end();
}

void SyncManager::runMaintenance() {
    // Run all maintenance tasks
    cleanupOldVersions();
    storageCleanup();
    adaptiveBandwidthAdjustment();
}

bool SyncManager::verifyIntegrity() const {
    // This would verify the integrity of synced files
    // For now, return true
    return true;
}

void SyncManager::rebuildIndices() {
    // This would rebuild internal indices for better performance
    // Implementation would depend on the actual indexing system
}

void SyncManager::initializeComponents() {
    // Initialize selective sync manager
    selectiveSync = std::make_unique<SelectiveSyncManager>();
    
    // Initialize bandwidth limiter
    if (config.enableBandwidthThrottling) {
        BandwidthConfig bwConfig;
        bwConfig.maxUploadSpeed = config.maxBandwidthUpload;
        bwConfig.maxDownloadSpeed = config.maxBandwidthDownload;
        bwConfig.enableThrottling = true;
        bwConfig.adaptiveThrottling = config.adaptiveBandwidth;
        
        bandwidthLimiter = std::make_unique<BandwidthLimiter>(bwConfig);
    }
    
    // Initialize resumable transfers
    if (config.enableResumeTransfers) {
        resumableTransfers = std::make_unique<ResumableTransferManager>();
    }
    
    // Initialize version history
    if (config.enableVersionHistory) {
        VersionPolicy vp;
        vp.enableVersioning = true;
        vp.maxVersions = config.maxVersionsPerFile;
        vp.maxAge = config.maxVersionAge;
        vp.compressOldVersions = config.enableCompression;
        
        versionHistory = std::make_unique<VersionHistoryManager>(vp);
    }
}

void SyncManager::configureBandwidthLimiter() {
    if (bandwidthLimiter) {
        BandwidthConfig bwConfig;
        bwConfig.maxUploadSpeed = config.maxBandwidthUpload;
        bwConfig.maxDownloadSpeed = config.maxBandwidthDownload;
        bwConfig.enableThrottling = config.enableBandwidthThrottling;
        bwConfig.adaptiveThrottling = config.adaptiveBandwidth;
        
        bandwidthLimiter->setConfiguration(bwConfig);
    }
}

void SyncManager::configureVersionHistory() {
    if (versionHistory) {
        VersionPolicy vp;
        vp.enableVersioning = config.enableVersionHistory;
        vp.maxVersions = config.maxVersionsPerFile;
        vp.maxAge = config.maxVersionAge;
        vp.compressOldVersions = config.enableCompression;
        
        versionHistory->setVersionPolicy(vp);
    }
}

void SyncManager::syncLoop() {
    const auto syncInterval = std::chrono::milliseconds(100);
    
    while (running.load() && !paused.load()) {
        // This would process pending sync operations
        // Implementation would depend on the actual sync system
        
        std::this_thread::sleep_for(syncInterval);
    }
}

bool SyncManager::processFileSync(const std::string& filePath, const std::string& peerId) {
    // This would process the actual file sync
    // Implementation would depend on the actual sync system
    
    // For now, simulate successful sync
    std::lock_guard<std::mutex> lock(statsMutex);
    syncStats.filesSynced++;
    
    return true;
}

void SyncManager::logEvent(const SyncEvent& event) {
    std::lock_guard<std::mutex> lock(eventMutex);
    
    eventLog.push_back(event);
    
    // Keep only last 1000 events
    if (eventLog.size() > 1000) {
        eventLog.erase(eventLog.begin(), eventLog.begin() + (eventLog.size() - 1000));
    }
    
    // Call callback if registered
    if (syncEventCallback) {
        syncEventCallback(event);
    }
}

void SyncManager::updateStats(const SyncEvent& event) {
    std::lock_guard<std::mutex> lock(statsMutex);
    
    switch (event.type) {
        case SyncEvent::Type::TRANSFER_COMPLETED:
            syncStats.filesSynced++;
            syncStats.lastSync = std::chrono::system_clock::now();
            break;
        case SyncEvent::Type::TRANSFER_FAILED:
            syncStats.transferFailures++;
            break;
        case SyncEvent::Type::FILE_CONFLICT:
            syncStats.conflictsResolved++;
            break;
        case SyncEvent::Type::VERSION_CREATED:
            syncStats.versionsCreated++;
            break;
        default:
            break;
    }
}

bool SyncManager::checkBandwidthAvailability(const std::string& filePath, size_t fileSize, bool isUpload) {
    if (!bandwidthLimiter || !config.enableBandwidthThrottling) {
        return true;
    }
    
    return bandwidthLimiter->isBandwidthAvailable(isUpload, fileSize);
}

void SyncManager::handleTransferFailure(const std::string& filePath, const std::string& error) {
    std::lock_guard<std::mutex> lock(statsMutex);
    syncStats.transferFailures++;
    
    // Log the failure
    logEvent(SyncEvent(SyncEvent::Type::TRANSFER_FAILED, filePath, error));
}

void SyncManager::handleTransferSuccess(const std::string& filePath, size_t bytesTransferred) {
    std::lock_guard<std::mutex> lock(statsMutex);
    syncStats.bytesTransferred += bytesTransferred;
    
    // Update bandwidth stats
    if (bandwidthLimiter) {
        // This would update the internal bandwidth tracking
    }
}

std::string SyncManager::getFileType(const std::string& filePath) const {
    size_t lastDot = filePath.find_last_of('.');
    if (lastDot != std::string::npos) {
        return filePath.substr(lastDot + 1);
    }
    return "unknown";
}

bool SyncManager::matchesPattern(const std::string& filePath, const std::string& pattern) const {
    // Handle glob patterns
    if (pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos) {
        return fnmatch(pattern.c_str(), filePath.c_str(), 0) == 0;
    }
    
    // Handle regex patterns
    if (pattern.size() > 2 && pattern.front() == '/' && pattern.back() == '/') {
        try {
            std::regex re(pattern.substr(1, pattern.size() - 2));
            return std::regex_search(filePath, re);
        } catch (...) {
            return false;
        }
    }
    
    // Literal pattern match
    return filePath.find(pattern) != std::string::npos;
}

bool SyncManager::isWithinSyncHours() const {
    std::lock_guard<std::mutex> lock(configMutex);
    
    if (config.allowedSyncHours.empty()) {
        return true;  // No time restrictions
    }
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    tm* timeinfo = localtime(&time_t);
    
    int currentHour = timeinfo->tm_hour;
    
    return std::find(config.allowedSyncHours.begin(), 
                    config.allowedSyncHours.end(), currentHour) != config.allowedSyncHours.end();
}

void SyncManager::waitForBandwidthAvailability(const std::string& filePath, size_t fileSize, bool isUpload) {
    if (bandwidthLimiter && config.enableBandwidthThrottling) {
        bandwidthLimiter->waitForBandwidth(isUpload, fileSize);
    }
}

void SyncManager::maintenanceLoop() {
    const auto maintenanceInterval = std::chrono::minutes(5);
    
    while (maintenanceRunning.load()) {
        runMaintenance();
        std::this_thread::sleep_for(maintenanceInterval);
    }
}

void SyncManager::adaptiveBandwidthAdjustment() {
    if (bandwidthLimiter && config.adaptiveBandwidth) {
        bandwidthLimiter->updateNetworkConditions();
    }
}

void SyncManager::storageCleanup() {
    if (versionHistory) {
        versionHistory->cleanupOldVersions();
    }
    
    if (resumableTransfers) {
        resumableTransfers->cleanupOldCheckpoints();
    }
}