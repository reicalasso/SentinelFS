#include "PluginLoader.h"
#include "IStorageAPI.h"
#include "EventBus.h"
#include <iostream>
#include <cassert>
#include <vector>
#include <memory>

int main() {
    SentinelFS::EventBus eventBus;
    SentinelFS::PluginLoader loader;
    
    // Path to the storage plugin shared library
    // Assuming running from build root
    std::string pluginPath = "plugins/storage/libstorage_plugin.so"; 

    std::cout << "Loading plugin from: " << pluginPath << std::endl;
    auto plugin = loader.loadPlugin(pluginPath, &eventBus);

    if (!plugin) {
        std::cerr << "Failed to load plugin" << std::endl;
        return 1;
    }

    std::cout << "Plugin loaded: " << plugin->getName() << " v" << plugin->getVersion() << std::endl;

    // Cast to IStorageAPI
    auto storagePlugin = std::dynamic_pointer_cast<SentinelFS::IStorageAPI>(plugin);
    if (storagePlugin) {
        std::cout << "Plugin implements IStorageAPI" << std::endl;
        
        // Test addFile
        std::string path = "/tmp/testfile.txt";
        std::string hash = "abcdef123456";
        long long timestamp = 1234567890;
        long long size = 1024;

        if (storagePlugin->addFile(path, hash, timestamp, size)) {
            std::cout << "Successfully added file metadata" << std::endl;
        } else {
            std::cerr << "Failed to add file metadata" << std::endl;
            return 1;
        }

        // Test getFile
        auto meta = storagePlugin->getFile(path);
        if (meta) {
            std::cout << "Retrieved metadata: " << meta->path << ", " << meta->hash << std::endl;
            if (meta->path == path && meta->hash == hash && meta->timestamp == timestamp && meta->size == size) {
                std::cout << "Metadata matches!" << std::endl;
            } else {
                std::cerr << "Metadata mismatch!" << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Failed to retrieve file metadata" << std::endl;
            return 1;
        }

        // Test Peer Storage
        SentinelFS::PeerInfo peer;
        peer.id = "peer_123";
        peer.ip = "192.168.1.50";
        peer.port = 8888;
        peer.lastSeen = 1000;
        peer.status = "active";

        if (storagePlugin->addPeer(peer)) {
             std::cout << "Successfully added peer" << std::endl;
        } else {
             std::cerr << "Failed to add peer" << std::endl;
             return 1;
        }

        auto peers = storagePlugin->getAllPeers();
        std::cout << "Retrieved " << peers.size() << " peers" << std::endl;
        bool found = false;
        for (const auto& p : peers) {
            if (p.id == peer.id) {
                std::cout << "Found peer: " << p.id << " at " << p.ip << std::endl;
                found = true;
                break;
            }
        }
        
        if (!found) {
            std::cerr << "Failed to retrieve added peer" << std::endl;
            return 1;
        }

        // Test removeFile
        if (storagePlugin->removeFile(path)) {
            std::cout << "Successfully removed file metadata" << std::endl;
        } else {
            std::cerr << "Failed to remove file metadata" << std::endl;
            return 1;
        }

        // Verify removal
        auto metaAfter = storagePlugin->getFile(path);
        if (!metaAfter) {
            std::cout << "Verification successful: File not found after removal" << std::endl;
        } else {
            std::cerr << "Verification failed: File still exists after removal" << std::endl;
            return 1;
        }

    } else {
        std::cerr << "Plugin does not implement IStorageAPI" << std::endl;
        return 1;
    }

    return 0;
}
