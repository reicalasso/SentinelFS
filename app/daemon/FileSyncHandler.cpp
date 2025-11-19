#include "FileSyncHandler.h"
#include "INetworkAPI.h"
#include "IStorageAPI.h"
#include <iostream>
#include <filesystem>

namespace SentinelFS {

FileSyncHandler::FileSyncHandler(INetworkAPI* network, IStorageAPI* storage, const std::string& watchDir)
    : network_(network), storage_(storage), watchDirectory_(watchDir) {}

void FileSyncHandler::handleFileModified(const std::string& fullPath) {
    std::string filename = std::filesystem::path(fullPath).filename().string();
    std::cout << "File modified: " << filename << std::endl;

    // Check ignore list
    if (shouldIgnore(filename)) {
        std::cout << "Ignoring update for " << filename << " (recently patched)" << std::endl;
        return;
    }

    // Check if sync is enabled
    if (!syncEnabled_) {
        std::cout << "⚠️  Sync disabled. Skipping update broadcast." << std::endl;
        return;
    }

    // Broadcast UPDATE_AVAILABLE to all peers
    auto peers = storage_->getAllPeers();
    for (const auto& peer : peers) {
        std::string updateMsg = "UPDATE_AVAILABLE|" + filename;
        std::vector<uint8_t> payload(updateMsg.begin(), updateMsg.end());
        network_->sendData(peer.id, payload);
    }
}

bool FileSyncHandler::shouldIgnore(const std::string& filename) {
    // Stub - needs mutex and timestamp map
    return false;
}

void FileSyncHandler::markAsPatched(const std::string& filename) {
    // Stub - needs mutex and timestamp map
}

} // namespace SentinelFS
