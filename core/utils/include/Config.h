#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

namespace SentinelFS {

    class Config {
    public:
        using Validator = std::function<bool(const std::string& key, const std::string& value)>;

        static Config& instance();

        bool loadFromFile(const std::string& path, bool overrideExisting = true);
        bool loadLayered(const std::vector<std::string>& paths, bool overrideExisting = true);
        void saveToFile(const std::string& path);

        bool hasKey(const std::string& key) const;

        std::string get(const std::string& key, const std::string& defaultValue = "") const;
        void set(const std::string& key, const std::string& value);
        
        int getInt(const std::string& key, int defaultValue = 0) const;
        void setInt(const std::string& key, int value);

        size_t getSize(const std::string& key, size_t defaultValue = 0) const;
        void setSize(const std::string& key, size_t value);

        bool getBool(const std::string& key, bool defaultValue = false) const;
        void setBool(const std::string& key, bool value);

        double getDouble(const std::string& key, double defaultValue = 0.0) const;
        void setDouble(const std::string& key, double value);

        bool validate(const std::unordered_map<std::string, Validator>& schema) const;

    private:
        Config() = default;
        std::unordered_map<std::string, std::string> settings_;
        mutable std::mutex mutex_;

        static std::string trim(const std::string& value);
        bool storeKV(const std::string& key, const std::string& value, bool overrideExisting);
    };

}
