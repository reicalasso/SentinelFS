#include "PluginManager.h"
#include "Logger.h"

#include <sstream>
#include <iostream>

namespace SentinelFS {

    void PluginManager::registerPlugin(const std::string& name, Descriptor descriptor) {
        registry_[name] = std::move(descriptor);
    }

    bool PluginManager::isRegistered(const std::string& name) const {
        return registry_.find(name) != registry_.end();
    }

    std::shared_ptr<IPlugin> PluginManager::load(const std::string& name,
                                                 EventBus* eventBus,
                                                 bool loadDependencies) {
        std::unordered_set<std::string> visiting;
        return loadInternal(name, eventBus, loadDependencies, visiting);
    }

    std::shared_ptr<IPlugin> PluginManager::loadInternal(const std::string& name,
                                                         EventBus* eventBus,
                                                         bool loadDependencies,
                                                         std::unordered_set<std::string>& visiting) {
        auto instanceIt = instances_.find(name);
        if (instanceIt != instances_.end()) {
            return instanceIt->second;
        }

        if (!isRegistered(name)) {
            std::cerr << "PluginManager: Plugin '" << name << "' not registered" << std::endl;
            return nullptr;
        }

        if (visiting.count(name)) {
            std::cerr << "PluginManager: Detected circular dependency involving '" << name << "'" << std::endl;
            return nullptr;
        }

        visiting.insert(name);
        const auto& descriptor = registry_.at(name);

        if (loadDependencies) {
            for (const auto& dep : descriptor.dependencies) {
                if (!isRegistered(dep)) {
                    std::cerr << "PluginManager: Dependency '" << dep << "' for plugin '" << name << "' not registered" << std::endl;
                    visiting.erase(name);
                    return nullptr;
                }
                if (!loadInternal(dep, eventBus, loadDependencies, visiting)) {
                    std::cerr << "PluginManager: Failed to load dependency '" << dep << "' for plugin '" << name << "'" << std::endl;
                    visiting.erase(name);
                    return nullptr;
                }
            }
        }

        auto plugin = loader_.loadPlugin(descriptor.path, eventBus);
        if (!plugin) {
            std::cerr << "PluginManager: Failed to load plugin '" << name << "' from " << descriptor.path << std::endl;
            visiting.erase(name);
            return nullptr;
        }

        if (!descriptor.minVersion.empty()) {
            if (!isVersionCompatible(plugin->getVersion(), descriptor.minVersion)) {
                std::cerr << "PluginManager: Plugin '" << name << "' version " << plugin->getVersion()
                          << " does not satisfy minimum " << descriptor.minVersion << std::endl;
                loader_.unloadPlugin(plugin->getName());
                visiting.erase(name);
                return nullptr;
            }
        }

        instances_[name] = plugin;
        visiting.erase(name);
        return plugin;
    }

    std::shared_ptr<IPlugin> PluginManager::get(const std::string& name) const {
        auto it = instances_.find(name);
        if (it != instances_.end()) {
            return it->second;
        }
        return nullptr;
    }

    std::vector<std::string> PluginManager::getDependencies(const std::string& name) const {
        auto it = registry_.find(name);
        if (it == registry_.end()) {
            return {};
        }
        return it->second.dependencies;
    }

    bool PluginManager::unload(const std::string& name) {
        auto instanceIt = instances_.find(name);
        if (instanceIt == instances_.end()) {
            return false;
        }

        auto pluginName = instanceIt->second->getName();
        instances_.erase(instanceIt);
        loader_.unloadPlugin(pluginName);
        return true;
    }

    void PluginManager::unloadAll() {
        // Collect plugin names first, then drop manager-held instances
        std::vector<std::string> pluginNames;
        pluginNames.reserve(instances_.size());

        for (const auto& entry : instances_) {
            if (entry.second) {
                pluginNames.push_back(entry.second->getName());
            }
        }

        // Release our shared_ptrs before asking loader_ to unload
        instances_.clear();

        for (const auto& name : pluginNames) {
            loader_.unloadPlugin(name);
        }
    }

    bool PluginManager::isVersionCompatible(const std::string& current,
                                            const std::string& required) const {
        auto currentTokens = tokenizeVersion(current);
        auto requiredTokens = tokenizeVersion(required);

        size_t maxSize = std::max(currentTokens.size(), requiredTokens.size());
        currentTokens.resize(maxSize, 0);
        requiredTokens.resize(maxSize, 0);

        for (size_t i = 0; i < maxSize; ++i) {
            if (currentTokens[i] > requiredTokens[i]) {
                return true;
            }
            if (currentTokens[i] < requiredTokens[i]) {
                return false;
            }
        }
        return true;
    }

    std::vector<int> PluginManager::tokenizeVersion(const std::string& version) {
        std::vector<int> tokens;
        std::stringstream ss(version);
        std::string segment;
        while (std::getline(ss, segment, '.')) {
            try {
                tokens.push_back(std::stoi(segment));
            } catch (...) {
                tokens.push_back(0);
            }
        }
        return tokens;
    }

} // namespace SentinelFS
