#include "IFileAPI.h"
#include "EventBus.h"
#include "InotifyWatcher.h"
#include "Win32Watcher.h"
#include "FSEventsWatcher.h"
#include "IFileWatcher.h"
#include "FileHasher.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <filesystem>

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
    FilesystemPlugin() {
#if defined(_WIN32)
        watcher_ = std::make_unique<Win32Watcher>();
#elif defined(__APPLE__)
        watcher_ = std::make_unique<FSEventsWatcher>();
#else
        watcher_ = std::make_unique<InotifyWatcher>();
#endif
    }
    
    ~FilesystemPlugin() {
        shutdown();
    }

    bool initialize(EventBus* eventBus) override {
        std::cout << "FilesystemPlugin initialized" << std::endl;
        eventBus_ = eventBus;
        
        // Set up callback for filesystem changes
        auto callback = [this](const WatchEvent& ev) {
            handleFileChange(ev);
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
        addRecursiveWatch(path);
    }

    void stopWatching(const std::string& path) override {
        watcher_->removeWatch(path);
    }

private:
    EventBus* eventBus_ = nullptr;
    std::unique_ptr<IFileWatcher> watcher_;

    /**
     * @brief Simple ignore filter for paths (e.g. .git, temporary files).
     */
    bool isIgnoredPath(const std::string& path) const {
        // Ignore .git directories and their contents
        if (path.find("/.git/") != std::string::npos || path.rfind("/.git", 0) == 0) {
            return true;
        }
        // Ignore common temp/editor swap files
        if (path.size() >= 1 && path.back() == '~') {
            return true;
        }
        if (path.size() >= 4 && path.substr(path.size() - 4) == ".swp") {
            return true;
        }
        return false;
    }

    void addRecursiveWatch(const std::string& root) {
        namespace fs = std::filesystem;
        std::error_code ec;

        fs::path rootPath(root);
        if (!fs::exists(rootPath, ec)) {
            return;
        }

        if (fs::is_directory(rootPath, ec)) {
            if (!isIgnoredPath(root)) {
                watcher_->addWatch(root);
            }
            for (auto it = fs::recursive_directory_iterator(rootPath, ec);
                 it != fs::recursive_directory_iterator(); ++it) {
                if (ec) {
                    break;
                }
                std::string itemPath = it->path().string();
                if (isIgnoredPath(itemPath)) {
                    continue;
                }
                
                if (it->is_directory(ec)) {
                    watcher_->addWatch(itemPath);
                } else if (it->is_regular_file(ec)) {
                    // Publish FILE_CREATED for existing files so ML plugin can analyze them
                    if (eventBus_) {
                        std::cout << "[FilesystemPlugin] Initial scan - Publishing FILE_CREATED: " << itemPath << std::endl;
                        eventBus_->publish("FILE_CREATED", itemPath);
                    }
                }
            }
        } else {
            // If a file path is given, watch its parent directory
            auto parent = rootPath.parent_path();
            if (!parent.empty()) {
                std::string dir = parent.string();
                if (!isIgnoredPath(dir)) {
                    watcher_->addWatch(dir);
                }
            }
            // Also publish FILE_CREATED for the file itself
            if (eventBus_ && !isIgnoredPath(root)) {
                std::cout << "[FilesystemPlugin] Initial scan - Publishing FILE_CREATED: " << root << std::endl;
                eventBus_->publish("FILE_CREATED", root);
            }
        }
    }

    /**
     * @brief Handle filesystem change events
     */
    void handleFileChange(const WatchEvent& ev) {
        if (isIgnoredPath(ev.path)) {
            return;
        }

        if (ev.type != WatchEventType::Delete && !ev.isDirectory) {
            std::string hash = FileHasher::calculateSHA256(ev.path);
            if (!hash.empty()) {
                std::cout << "Hash: " << hash << std::endl;
            }
        }
        
        if (eventBus_) {
            std::string eventType;
            switch (ev.type) {
                case WatchEventType::Create:
                    eventType = "FILE_CREATED";
                    std::cout << "[FilesystemPlugin] Publishing FILE_CREATED: " << ev.path << std::endl;
                    break;
                case WatchEventType::Modify:
                    eventType = "FILE_MODIFIED";
                    std::cout << "[FilesystemPlugin] Publishing FILE_MODIFIED: " << ev.path << std::endl;
                    break;
                case WatchEventType::Delete:
                    eventType = "FILE_DELETED";
                    std::cout << "[FilesystemPlugin] Publishing FILE_DELETED: " << ev.path << std::endl;
                    break;
                case WatchEventType::Rename:
                    eventType = "FILE_RENAMED";
                    break;
            }

            if (!eventType.empty()) {
                eventBus_->publish(eventType, ev.path);
            }
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
