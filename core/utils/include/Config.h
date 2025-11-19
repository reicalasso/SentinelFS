#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace SentinelFS {

    class Config {
    public:
        static Config& instance();

        bool loadFromFile(const std::string& path);
        void saveToFile(const std::string& path);

        std::string get(const std::string& key, const std::string& defaultValue = "");
        void set(const std::string& key, const std::string& value);
        
        int getInt(const std::string& key, int defaultValue = 0);
        void setInt(const std::string& key, int value);

    private:
        Config() = default;
        std::unordered_map<std::string, std::string> settings_;
        std::mutex mutex_;
    };

}
