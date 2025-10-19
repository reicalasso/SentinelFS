#include "resume_transfers.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <openssl/sha.h>
#include <cstring>

ResumableTransferManager::ResumableTransferManager() 
    : checkpointDirectory(".sentinelfs/checkpoints"),
      autoCleanup(true),
      maxCheckpointAge(std::chrono::hours(24*7)) {  // 1 week
    
    // Create checkpoint directory if it doesn't exist
    std::filesystem::create_directories(checkpointDirectory);
}

ResumableTransferManager::~ResumableTransferManager() {
    stop();
}

void ResumableTransferManager::start() {
    if (running.exchange(true)) {
        return;  // Already running
    }
    
    recoveryThread = std::thread(&ResumableTransferManager::recoveryLoop, this);
}

void ResumableTransferManager::stop() {
    if (running.exchange(false)) {
        if (recoveryThread.joinable()) {
            recoveryThread.join();
        }
    }
}

TransferCheckpoint ResumableTransferManager::createCheckpoint(const std::string& filePath, 
                                                              size_t fileSize,
                                                              const std::string& peerId,
                                                              bool isUpload) {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    TransferCheckpoint checkpoint(filePath, fileSize);
    checkpoint.peerId = peerId;
    checkpoint.isUpload = isUpload;
    checkpoint.chunkSize = 1024 * 1024;  // 1MB chunks by default
    checkpoint.retryAttempts = 0;
    
    // Save the checkpoint
    saveCheckpoint(checkpoint);
    
    activeCheckpoints[checkpoint.transferId] = checkpoint;
    
    return checkpoint;
}

bool ResumableTransferManager::saveCheckpoint(const TransferCheckpoint& checkpoint) {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    // Update last checkpoint time
    auto& storedCheckpoint = activeCheckpoints[checkpoint.transferId];
    storedCheckpoint = checkpoint;
    storedCheckpoint.lastCheckpoint = std::chrono::steady_clock::now();
    
    // Also save to file for persistence
    return saveCheckpointToFile(storedCheckpoint);
}

TransferCheckpoint ResumableTransferManager::loadCheckpoint(const std::string& transferId) const {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    // First check active checkpoints
    auto it = activeCheckpoints.find(transferId);
    if (it != activeCheckpoints.end()) {
        return it->second;
    }
    
    // Then check file system
    return loadCheckpointFromFile(transferId);
}

bool ResumableTransferManager::removeCheckpoint(const std::string& transferId) {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    // Remove from active checkpoints
    activeCheckpoints.erase(transferId);
    
    // Remove from completed checkpoints
    completedCheckpoints.erase(transferId);
    
    // Remove from file system
    return removeCheckpointFile(transferId);
}

bool ResumableTransferManager::hasCheckpoint(const std::string& transferId) const {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    // Check active checkpoints
    if (activeCheckpoints.find(transferId) != activeCheckpoints.end()) {
        return true;
    }
    
    // Check file system
    std::string checkpointFile = checkpointDirectory + "/" + transferId + ".ckpt";
    return std::filesystem::exists(checkpointFile);
}

bool ResumableTransferManager::resumeTransfer(const std::string& transferId) {
    auto checkpoint = loadCheckpoint(transferId);
    if (checkpoint.transferId.empty()) {
        return false;  // No checkpoint found
    }
    
    // Check if transfer is stale
    if (isTransferStale(checkpoint)) {
        notifyTransferError(transferId, "Transfer is too old to resume");
        return false;
    }
    
    // Attempt to recover the transfer
    return attemptTransferRecovery(checkpoint);
}

bool ResumableTransferManager::resumeTransferFromFile(const std::string& filePath, 
                                                      const std::string& peerId) {
    // Generate transfer ID from file path and peer
    std::string transferId = filePath + "_" + peerId;
    
    // Replace special characters to make valid filename
    std::replace(transferId.begin(), transferId.end(), '/', '_');
    std::replace(transferId.begin(), transferId.end(), '\\', '_');
    
    return resumeTransfer(transferId);
}

bool ResumableTransferManager::recoverInterruptedTransfers() {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    bool allRecovered = true;
    
    // Attempt recovery for each active checkpoint
    for (auto& pair : activeCheckpoints) {
        TransferCheckpoint& checkpoint = pair.second;
        
        if (!attemptTransferRecovery(checkpoint)) {
            allRecovered = false;
            failedTransfers++;
        } else {
            recoveredTransfers++;
        }
    }
    
    return allRecovered;
}

std::vector<TransferCheckpoint> ResumableTransferManager::getPendingTransfers() const {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    std::vector<TransferCheckpoint> pending;
    for (const auto& pair : activeCheckpoints) {
        pending.push_back(pair.second);
    }
    
    return pending;
}

std::vector<TransferCheckpoint> ResumableTransferManager::getFailedTransfers() const {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    std::vector<TransferCheckpoint> failed;
    for (const auto& pair : transferRetryCounts) {
        if (pair.second >= maxRetryAttempts.load()) {
            auto it = activeCheckpoints.find(pair.first);
            if (it != activeCheckpoints.end()) {
                failed.push_back(it->second);
            }
        }
    }
    
    return failed;
}

void ResumableTransferManager::handleDisconnection(const std::string& peerId, 
                                                   const std::string& reason) {
    std::lock_guard<std::mutex> lock(networkMutex);
    
    DisconnectionEvent event;
    event.peerId = peerId;
    event.reason = reason;
    event.recovered = false;
    
    connectionHistory[peerId].push_back(event);
}

void ResumableTransferManager::handleReconnection(const std::string& peerId) {
    std::lock_guard<std::mutex> lock(networkMutex);
    
    auto it = connectionHistory.find(peerId);
    if (it != connectionHistory.end() && !it->second.empty()) {
        // Mark the last disconnection as recovered
        auto& events = it->second;
        if (!events.back().recovered) {
            events.back().recovered = true;
            events.back().reconnectTime = std::chrono::steady_clock::now();
        }
    }
}

std::vector<DisconnectionEvent> ResumableTransferManager::getConnectionHistory(const std::string& peerId) const {
    std::lock_guard<std::mutex> lock(networkMutex);
    
    auto it = connectionHistory.find(peerId);
    if (it != connectionHistory.end()) {
        return it->second;
    }
    
    return {};
}

void ResumableTransferManager::setMaxRetryAttempts(int maxRetries) {
    maxRetryAttempts = maxRetries;
}

int ResumableTransferManager::getMaxRetryAttempts() const {
    return maxRetryAttempts.load();
}

void ResumableTransferManager::resetRetryCount(const std::string& transferId) {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    transferRetryCounts[transferId] = 0;
}

std::vector<size_t> ResumableTransferManager::getMissingChunks(const TransferCheckpoint& checkpoint) const {
    std::vector<size_t> missing;
    
    size_t totalChunks = (checkpoint.totalSize + checkpoint.chunkSize - 1) / checkpoint.chunkSize;
    
    for (size_t i = 0; i < totalChunks; ++i) {
        // Check if chunk is completed
        if (std::find(checkpoint.completedChunks.begin(), 
                     checkpoint.completedChunks.end(), i) == checkpoint.completedChunks.end()) {
            missing.push_back(i);
        }
    }
    
    return missing;
}

bool ResumableTransferManager::markChunkComplete(const std::string& transferId, size_t chunkIndex) {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    auto it = activeCheckpoints.find(transferId);
    if (it == activeCheckpoints.end()) {
        return false;
    }
    
    TransferCheckpoint& checkpoint = it->second;
    
    // Add chunk to completed chunks if not already present
    if (std::find(checkpoint.completedChunks.begin(), 
                 checkpoint.completedChunks.end(), chunkIndex) == checkpoint.completedChunks.end()) {
        checkpoint.completedChunks.push_back(chunkIndex);
        checkpoint.transferredBytes += std::min(checkpoint.chunkSize, 
                                               checkpoint.totalSize - (chunkIndex * checkpoint.chunkSize));
        
        // Save updated checkpoint
        return saveCheckpoint(checkpoint);
    }
    
    return true;
}

size_t ResumableTransferManager::getNextChunkToTransfer(const TransferCheckpoint& checkpoint) const {
    size_t totalChunks = (checkpoint.totalSize + checkpoint.chunkSize - 1) / checkpoint.chunkSize;
    
    // Find first incomplete chunk
    for (size_t i = 0; i < totalChunks; ++i) {
        if (std::find(checkpoint.completedChunks.begin(), 
                     checkpoint.completedChunks.end(), i) == checkpoint.completedChunks.end()) {
            return i;
        }
    }
    
    return totalChunks;  // All chunks completed
}

bool ResumableTransferManager::verifyTransferIntegrity(const TransferCheckpoint& checkpoint) const {
    std::string calculatedChecksum = calculateFileChecksum(checkpoint.filePath);
    return calculatedChecksum == checkpoint.checksum;
}

std::string ResumableTransferManager::calculateFileChecksum(const std::string& filePath) const {
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

double ResumableTransferManager::getTransferProgress(const std::string& transferId) const {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    auto it = activeCheckpoints.find(transferId);
    if (it != activeCheckpoints.end()) {
        const TransferCheckpoint& checkpoint = it->second;
        if (checkpoint.totalSize > 0) {
            return static_cast<double>(checkpoint.transferredBytes) / 
                   static_cast<double>(checkpoint.totalSize);
        }
    }
    
    return 0.0;
}

std::chrono::seconds ResumableTransferManager::getEstimatedTimeRemaining(const std::string& transferId) const {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    auto it = activeCheckpoints.find(transferId);
    if (it != activeCheckpoints.end()) {
        const TransferCheckpoint& checkpoint = it->second;
        
        if (checkpoint.transferredBytes > 0 && checkpoint.totalSize > 0) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - checkpoint.lastCheckpoint);
            
            double bytesPerSecond = static_cast<double>(checkpoint.transferredBytes) / 
                                  elapsed.count();
            
            if (bytesPerSecond > 0) {
                size_t remainingBytes = checkpoint.totalSize - checkpoint.transferredBytes;
                return std::chrono::seconds(static_cast<int>(remainingBytes / bytesPerSecond));
            }
        }
    }
    
    return std::chrono::seconds(0);
}

void ResumableTransferManager::cleanupOldCheckpoints(std::chrono::hours maxAge) {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = activeCheckpoints.begin(); it != activeCheckpoints.end();) {
        const TransferCheckpoint& checkpoint = it->second;
        auto age = std::chrono::duration_cast<std::chrono::hours>(
            now - checkpoint.lastCheckpoint);
        
        if (age > maxAge) {
            it = activeCheckpoints.erase(it);
        } else {
            ++it;
        }
    }
    
    // Also cleanup from file system
    for (const auto& entry : std::filesystem::directory_iterator(checkpointDirectory)) {
        if (entry.path().extension() == ".ckpt") {
            auto fileTime = std::filesystem::last_write_time(entry);
            auto fileAge = std::chrono::duration_cast<std::chrono::hours>(
                std::chrono::system_clock::now() - 
                std::chrono::time_point_cast<std::chrono::system_clock::duration>(fileTime));
            
            if (fileAge > maxAge) {
                std::filesystem::remove(entry.path());
            }
        }
    }
}

void ResumableTransferManager::cleanupCompletedTransfers() {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    
    // Move completed transfers from active to completed
    for (auto it = activeCheckpoints.begin(); it != activeCheckpoints.end();) {
        const TransferCheckpoint& checkpoint = it->second;
        
        if (checkpoint.transferredBytes >= checkpoint.totalSize) {
            completedCheckpoints[checkpoint.transferId] = checkpoint;
            it = activeCheckpoints.erase(it);
        } else {
            ++it;
        }
    }
}

void ResumableTransferManager::setTransferCompleteCallback(TransferCompleteCallback callback) {
    transferCompleteCallback = callback;
}

void ResumableTransferManager::setTransferProgressCallback(TransferProgressCallback callback) {
    transferProgressCallback = callback;
}

void ResumableTransferManager::setTransferErrorCallback(TransferErrorCallback callback) {
    transferErrorCallback = callback;
}

size_t ResumableTransferManager::getTotalPendingTransfers() const {
    std::lock_guard<std::mutex> lock(checkpointsMutex);
    return activeCheckpoints.size();
}

size_t ResumableTransferManager::getTotalRecoveredTransfers() const {
    return recoveredTransfers.load();
}

double ResumableTransferManager::getRecoverySuccessRate() const {
    size_t total = recoveredTransfers.load() + failedTransfers.load();
    return total > 0 ? static_cast<double>(recoveredTransfers.load()) / total : 0.0;
}

void ResumableTransferManager::recoveryLoop() {
    const auto checkInterval = std::chrono::seconds(30);
    
    while (running.load()) {
        recoverInterruptedTransfers();
        std::this_thread::sleep_for(checkInterval);
    }
}

bool ResumableTransferManager::attemptTransferRecovery(TransferCheckpoint& checkpoint) {
    // Increment retry count
    {
        std::lock_guard<std::mutex> lock(checkpointsMutex);
        transferRetryCounts[checkpoint.transferId]++;
        
        if (transferRetryCounts[checkpoint.transferId] > maxRetryAttempts.load()) {
            notifyTransferError(checkpoint.transferId, "Max retry attempts exceeded");
            return false;
        }
    }
    
    // Verify file integrity
    if (!checkpoint.checksum.empty()) {
        std::string currentChecksum = calculateFileChecksum(checkpoint.filePath);
        if (!currentChecksum.empty() && currentChecksum != checkpoint.checksum) {
            notifyTransferError(checkpoint.transferId, "File integrity check failed");
            return false;
        }
    }
    
    // Recovery successful
    notifyTransferComplete(checkpoint.transferId, true);
    
    // Move to completed
    {
        std::lock_guard<std::mutex> lock(checkpointsMutex);
        completedCheckpoints[checkpoint.transferId] = checkpoint;
        activeCheckpoints.erase(checkpoint.transferId);
    }
    
    return true;
}

void ResumableTransferManager::notifyTransferComplete(const std::string& transferId, bool success) {
    updateTransferStats(transferId, success);
    
    if (transferCompleteCallback) {
        transferCompleteCallback(transferId, success);
    }
}

void ResumableTransferManager::notifyTransferProgress(const std::string& transferId, double progress) {
    if (transferProgressCallback) {
        transferProgressCallback(transferId, progress);
    }
}

void ResumableTransferManager::notifyTransferError(const std::string& transferId, const std::string& error) {
    updateTransferStats(transferId, false);
    
    if (transferErrorCallback) {
        transferErrorCallback(transferId, error);
    }
}

bool ResumableTransferManager::saveCheckpointToFile(const TransferCheckpoint& checkpoint) const {
    std::string filename = checkpointDirectory + "/" + checkpoint.transferId + ".ckpt";
    std::ofstream file(filename, std::ios::binary);
    
    if (!file) {
        return false;
    }
    
    // Write checkpoint data
    file << checkpoint.filePath << "\n";
    file << checkpoint.transferId << "\n";
    file << checkpoint.totalSize << "\n";
    file << checkpoint.transferredBytes << "\n";
    file << checkpoint.chunkSize << "\n";
    file << checkpoint.checksum << "\n";
    file << checkpoint.retryAttempts << "\n";
    file << checkpoint.peerId << "\n";
    file << (checkpoint.isUpload ? "1" : "0") << "\n";
    
    // Write completed chunks
    file << checkpoint.completedChunks.size() << "\n";
    for (size_t chunk : checkpoint.completedChunks) {
        file << chunk << " ";
    }
    file << "\n";
    
    file.close();
    return file.good();
}

TransferCheckpoint ResumableTransferManager::loadCheckpointFromFile(const std::string& transferId) const {
    std::string filename = checkpointDirectory + "/" + transferId + ".ckpt";
    std::ifstream file(filename, std::ios::binary);
    
    if (!file) {
        return TransferCheckpoint();
    }
    
    TransferCheckpoint checkpoint;
    
    // Read checkpoint data
    std::getline(file, checkpoint.filePath);
    std::getline(file, checkpoint.transferId);
    file >> checkpoint.totalSize;
    file.ignore();  // Ignore newline
    file >> checkpoint.transferredBytes;
    file.ignore();  // Ignore newline
    file >> checkpoint.chunkSize;
    file.ignore();  // Ignore newline
    std::getline(file, checkpoint.checksum);
    file >> checkpoint.retryAttempts;
    file.ignore();  // Ignore newline
    std::getline(file, checkpoint.peerId);
    
    int isUploadInt;
    file >> isUploadInt;
    checkpoint.isUpload = (isUploadInt == 1);
    file.ignore();  // Ignore newline
    
    // Read completed chunks
    size_t chunkCount;
    file >> chunkCount;
    file.ignore();  // Ignore newline
    
    checkpoint.completedChunks.reserve(chunkCount);
    for (size_t i = 0; i < chunkCount; ++i) {
        size_t chunk;
        file >> chunk;
        checkpoint.completedChunks.push_back(chunk);
    }
    
    file.close();
    return checkpoint;
}

bool ResumableTransferManager::removeCheckpointFile(const std::string& transferId) const {
    std::string filename = checkpointDirectory + "/" + transferId + ".ckpt";
    return std::filesystem::remove(filename);
}

std::string TransferCheckpoint::generateTransferId() const {
    // Generate a unique ID based on file path and timestamp
    auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    std::string id = filePath + "_" + std::to_string(now);
    
    // Simple hash to make shorter ID
    std::hash<std::string> hasher;
    size_t hash = hasher(id);
    
    std::stringstream ss;
    ss << std::hex << hash;
    return ss.str();
}

void ResumableTransferManager::updateTransferStats(const std::string& transferId, bool success) {
    if (success) {
        recoveredTransfers++;
    } else {
        failedTransfers++;
    }
}

bool ResumableTransferManager::isTransferStale(const TransferCheckpoint& checkpoint) const {
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::hours>(
        now - checkpoint.lastCheckpoint);
    
    return age > maxCheckpointAge;
}