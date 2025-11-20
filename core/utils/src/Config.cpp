#include "Config.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace SentinelFS {

    Config& Config::instance() {
        static Config instance;
        return instance;
    }

    bool Config::loadFromFile(const std::string& path, bool overrideExisting) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        std::vector<std::pair<std::string, std::string>> parsed;
        std::string line;
        while (std::getline(file, line)) {
            auto trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;

            size_t delimiterPos = trimmed.find('=');
            if (delimiterPos == std::string::npos) continue;

            std::string key = trim(trimmed.substr(0, delimiterPos));
            std::string value = trim(trimmed.substr(delimiterPos + 1));
            if (!key.empty()) {
                parsed.emplace_back(key, value);
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [key, value] : parsed) {
            storeKV(key, value, overrideExisting);
        }
        return true;
    }

    bool Config::loadLayered(const std::vector<std::string>& paths, bool overrideExisting) {
        bool loaded = false;
        for (const auto& path : paths) {
            if (loadFromFile(path, overrideExisting)) {
                loaded = true;
            }
        }
        return loaded;
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

    bool Config::hasKey(const std::string& key) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return settings_.find(key) != settings_.end();
    }

    std::string Config::get(const std::string& key, const std::string& defaultValue) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = settings_.find(key);
        if (it != settings_.end()) {
            return it->second;
        }
        return defaultValue;
    }

    void Config::set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_[key] = value;
    }

    int Config::getInt(const std::string& key, int defaultValue) const {
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

    size_t Config::getSize(const std::string& key, size_t defaultValue) const {
        std::string val = get(key, "");
        if (val.empty()) return defaultValue;
        try {
            return static_cast<size_t>(std::stoull(val));
        } catch (...) {
            return defaultValue;
        }
    }

    void Config::setSize(const std::string& key, size_t value) {
        set(key, std::to_string(value));
    }

    bool Config::getBool(const std::string& key, bool defaultValue) const {
        std::string val = get(key, "");
        if (val.empty()) return defaultValue;
        std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (val == "1" || val == "true" || val == "yes" || val == "on") {
            return true;
        }
        if (val == "0" || val == "false" || val == "no" || val == "off") {
            return false;
        }
        return defaultValue;
    }

    void Config::setBool(const std::string& key, bool value) {
        set(key, value ? "true" : "false");
    }

    double Config::getDouble(const std::string& key, double defaultValue) const {
        std::string val = get(key, "");
        if (val.empty()) return defaultValue;
        try {
            return std::stod(val);
        } catch (...) {
            return defaultValue;
        }
    }

    void Config::setDouble(const std::string& key, double value) {
        set(key, std::to_string(value));
    }

    bool Config::validate(const std::unordered_map<std::string, Validator>& schema) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [key, validator] : schema) {
            auto it = settings_.find(key);
            if (it != settings_.end() && validator && !validator(key, it->second)) {
                return false;
            }
        }
        return true;
    }

    std::string Config::trim(const std::string& value) {
        auto start = value.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(start, end - start + 1);
    }

    bool Config::storeKV(const std::string& key, const std::string& value, bool overrideExisting) {
        auto it = settings_.find(key);
        if (!overrideExisting && it != settings_.end()) {
            return false;
        }
        settings_[key] = value;
        return true;
    }

}
