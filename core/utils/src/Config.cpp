#include "Config.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace SentinelFS {

    Config& Config::instance() {
        static Config instance;
        return instance;
    }

    bool Config::loadFromFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;

            size_t delimiterPos = line.find('=');
            if (delimiterPos != std::string::npos) {
                std::string key = line.substr(0, delimiterPos);
                std::string value = line.substr(delimiterPos + 1);
                settings_[key] = value;
            }
        }
        return true;
    }

    void Config::saveToFile(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ofstream file(path);
        if (file.is_open()) {
            for (const auto& pair : settings_) {
                file << pair.first << "=" << pair.second << std::endl;
            }
        }
    }

    std::string Config::get(const std::string& key, const std::string& defaultValue) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (settings_.find(key) != settings_.end()) {
            return settings_[key];
        }
        return defaultValue;
    }

    void Config::set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_[key] = value;
    }

    int Config::getInt(const std::string& key, int defaultValue) {
        std::string val = get(key, "");
        if (val.empty()) return defaultValue;
        try {
            return std::stoi(val);
        } catch (...) {
            return defaultValue;
        }
    }

    void Config::setInt(const std::string& key, int value) {
        set(key, std::to_string(value));
    }

}
