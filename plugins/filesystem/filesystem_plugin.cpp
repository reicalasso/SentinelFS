#include "IFileAPI.h"
#include "EventBus.h"
#include "InotifyWatcher.h"
#include "FileHasher.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>

namespace SentinelFS {

/**
 * @brief Filesystem plugin with inotify-based file monitoring
 * 
 * Delegates responsibilities:
 * - InotifyWatcher: Filesystem change detection
 * - FileHasher: File integrity verification
 */
class FilesystemPlugin : public IFileAPI {
public:
    FilesystemPlugin() : watcher_(std::make_unique<InotifyWatcher>()) {}
    
    ~FilesystemPlugin() {
        shutdown();
    }

    bool initialize(EventBus* eventBus) override {
        std::cout << "FilesystemPlugin initialized" << std::endl;
        eventBus_ = eventBus;
        
        // Set up callback for filesystem changes
        auto callback = [this](const std::string& eventType, const std::string& path) {
            handleFileChange(eventType, path);
        };
        
        return watcher_->initialize(callback);
    }

    void shutdown() override {
        std::cout << "FilesystemPlugin shutdown" << std::endl;
        watcher_->shutdown();
    }

    std::string getName() const override {
        return "FilesystemPlugin";
    }

    std::string getVersion() const override {
        return "1.0.0";
    }

    std::vector<uint8_t> readFile(const std::string& path) override {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file: " << path << std::endl;
            return {};
        }
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());
    }

    bool writeFile(const std::string& path, const std::vector<uint8_t>& data) override {
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open file for writing: " << path << std::endl;
            return false;
        }
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        return true;
    }

    void startWatching(const std::string& path) override {
        watcher_->addWatch(path);
    }

    void stopWatching(const std::string& path) override {
        watcher_->removeWatch(path);
    }

private:
    EventBus* eventBus_ = nullptr;
    std::unique_ptr<InotifyWatcher> watcher_;

    /**
     * @brief Handle filesystem change events
     */
    void handleFileChange(const std::string& eventType, const std::string& path) {
        if (eventType != "FILE_DELETED") {
            std::string hash = FileHasher::calculateSHA256(path);
            if (!hash.empty()) {
                std::cout << "Hash: " << hash << std::endl;
            }
        }
        
        if (eventBus_) {
            eventBus_->publish(eventType, path);
        }
    }
};

extern "C" {
    IPlugin* create_plugin() {
        return new FilesystemPlugin();
    }

    void destroy_plugin(IPlugin* plugin) {
        delete plugin;
    }
}

} // namespace SentinelFS
