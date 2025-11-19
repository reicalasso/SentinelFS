#include "IFileAPI.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>

namespace SentinelFS {

    class FilesystemPlugin : public IFileAPI {
    public:
        bool initialize() override {
            std::cout << "FilesystemPlugin initialized" << std::endl;
            return true;
        }

        void shutdown() override {
            std::cout << "FilesystemPlugin shutdown" << std::endl;
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

        void watchDirectory(const std::string& path) override {
            std::cout << "Watching directory: " << path << std::endl;
            // TODO: Implement actual directory watching (inotify/FSEvents)
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