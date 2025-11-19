#include "IFileAPI.h"
#include "EventBus.h"
#include "DeltaEngine.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <thread>
#include <atomic>
#include <map>
#include <sys/inotify.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include <cstring>

namespace SentinelFS {

    class FilesystemPlugin : public IFileAPI {
    public:
        ~FilesystemPlugin() {
            shutdown();
        }

        bool initialize(EventBus* eventBus) override {
            std::cout << "FilesystemPlugin initialized" << std::endl;
            eventBus_ = eventBus;
            
            inotifyFd_ = inotify_init();
            if (inotifyFd_ < 0) {
                std::cerr << "Failed to initialize inotify" << std::endl;
                return false;
            }

            running_ = true;
            watcherThread_ = std::thread(&FilesystemPlugin::monitorLoop, this);
            return true;
        }

        void shutdown() override {
            std::cout << "FilesystemPlugin shutdown" << std::endl;
            running_ = false;
            if (inotifyFd_ >= 0) {
                close(inotifyFd_); // This will wake up the read in the thread
                inotifyFd_ = -1;
            }
            if (watcherThread_.joinable()) {
                watcherThread_.join();
            }
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
            std::cout << "Watching directory: " << path << std::endl;
            int wd = inotify_add_watch(inotifyFd_, path.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM);
            if (wd < 0) {
                std::cerr << "Failed to add watch for " << path << std::endl;
            } else {
                watchDescriptors_[wd] = path;
            }
        }

        void stopWatching(const std::string& path) override {
            // TODO: Implement remove watch
        }

    private:
        EventBus* eventBus_ = nullptr;
        std::thread watcherThread_;
        std::atomic<bool> running_{false};
        int inotifyFd_ = -1;
        std::map<int, std::string> watchDescriptors_;

        std::string calculateSHA256(const std::string& path) {
            std::ifstream file(path, std::ios::binary);
            if (!file) return "";

            std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return DeltaEngine::calculateSHA256(buffer.data(), buffer.size());
        }

        void monitorLoop() {
            const int EVENT_SIZE = sizeof(struct inotify_event);
            const int BUF_LEN = 1024 * (EVENT_SIZE + 16);
            char buffer[BUF_LEN];

            while (running_) {
                int length = read(inotifyFd_, buffer, BUF_LEN);
                if (length < 0) {
                    if (running_) std::cerr << "inotify read error" << std::endl;
                    break;
                }

                int i = 0;
                while (i < length) {
                    struct inotify_event* event = (struct inotify_event*)&buffer[i];
                    if (event->len) {
                        if (watchDescriptors_.find(event->wd) != watchDescriptors_.end()) {
                            std::string dirPath = watchDescriptors_[event->wd];
                            std::string filename = event->name;
                            std::string fullPath = dirPath + "/" + filename;
                            
                            std::string eventType;
                            if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO) {
                                eventType = "FILE_CREATED";
                            } else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM) {
                                eventType = "FILE_DELETED";
                            } else if (event->mask & IN_MODIFY) {
                                eventType = "FILE_MODIFIED";
                            }

                            if (!eventType.empty() && eventBus_) {
                                // TODO: Define a proper event data structure
                                // For now, just publishing the path
                                std::cout << "Detected " << eventType << ": " << fullPath << std::endl;
                                
                                if (eventType != "FILE_DELETED") {
                                    std::string hash = calculateSHA256(fullPath);
                                    std::cout << "Hash: " << hash << std::endl;
                                }
                                
                                eventBus_->publish(eventType, fullPath);
                            }
                        }
                    }
                    i += EVENT_SIZE + event->len;
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

}