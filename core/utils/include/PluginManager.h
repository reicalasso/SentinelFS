#pragma once

#include "PluginLoader.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SentinelFS {

    /**
     * @brief Higher level plugin manager that tracks dependencies and supports
     *        compatibility validation.
     */
    class PluginManager {
    public:
        enum class PluginStatus {
            NOT_LOADED,
            LOADED,
            FAILED,
            OPTIONAL_NOT_LOADED
        };

        struct Descriptor {
            std::string path;
            std::vector<std::string> dependencies;
            std::string minVersion;
            bool optional = false;
        };

        PluginManager() = default;

        void registerPlugin(const std::string& name, Descriptor descriptor);
        bool isRegistered(const std::string& name) const;

        std::shared_ptr<IPlugin> load(const std::string& name,
                                      EventBus* eventBus,
                                      bool loadDependencies = true);

        std::shared_ptr<IPlugin> get(const std::string& name) const;
        std::vector<std::string> getDependencies(const std::string& name) const;

        bool unload(const std::string& name);
        void unloadAll();

        PluginStatus getPluginStatus(const std::string& name) const;
        std::vector<std::pair<std::string, PluginStatus>> getAllPluginStatuses() const;

    private:
        std::shared_ptr<IPlugin> loadInternal(const std::string& name,
                                              EventBus* eventBus,
                                              bool loadDependencies,
                                              std::unordered_set<std::string>& visiting);
        bool isVersionCompatible(const std::string& current,
                                 const std::string& required) const;
        static std::vector<int> tokenizeVersion(const std::string& version);
        std::vector<std::string> resolveUnloadOrder() const;

        PluginLoader loader_;
        std::unordered_map<std::string, Descriptor> registry_;
        std::unordered_map<std::string, std::shared_ptr<IPlugin>> instances_;
        mutable std::unordered_map<std::string, PluginStatus> statuses_;
    };

} // namespace SentinelFS
