#include "PluginManager.h"
#include "Logger.h"

#include <sstream>
#include <functional>

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
            Logger::instance().error("PluginManager: Plugin '" + name + "' not registered", "PluginManager");
            return nullptr;
        }

        if (visiting.count(name)) {
            Logger::instance().error("PluginManager: Detected circular dependency involving '" + name + "'", "PluginManager");
            return nullptr;
        }

        visiting.insert(name);
        const auto& descriptor = registry_.at(name);

        if (loadDependencies) {
            for (const auto& dep : descriptor.dependencies) {
                if (!isRegistered(dep)) {
                    Logger::instance().error("PluginManager: Dependency '" + dep + "' for plugin '" + name + "' not registered", "PluginManager");
                    visiting.erase(name);
                    return nullptr;
                }
                if (!loadInternal(dep, eventBus, loadDependencies, visiting)) {
                    Logger::instance().error("PluginManager: Failed to load dependency '" + dep + "' for plugin '" + name + "'", "PluginManager");
                    visiting.erase(name);
                    return nullptr;
                }
            }
        }

        auto plugin = loader_.loadPlugin(descriptor.path, eventBus);
        if (!plugin) {
            Logger::instance().error("PluginManager: Failed to load plugin '" + name + "' from " + descriptor.path, "PluginManager");
            statuses_[name] = descriptor.optional ? PluginStatus::OPTIONAL_NOT_LOADED : PluginStatus::FAILED;
            visiting.erase(name);
            return nullptr;
        }

        if (!descriptor.minVersion.empty()) {
            if (!isVersionCompatible(plugin->getVersion(), descriptor.minVersion)) {
                Logger::instance().error("PluginManager: Plugin '" + name + "' version " + plugin->getVersion()
                                        + " does not satisfy minimum " + descriptor.minVersion, "PluginManager");
                loader_.unloadPlugin(plugin->getName());
                visiting.erase(name);
                return nullptr;
            }
        }

        instances_[name] = plugin;
        statuses_[name] = PluginStatus::LOADED;
        visiting.erase(name);
        return plugin;
    }

    PluginManager::PluginStatus PluginManager::getPluginStatus(const std::string& name) const {
        auto it = statuses_.find(name);
        if (it != statuses_.end()) {
            return it->second;
        }
        return PluginStatus::NOT_LOADED;
    }

    std::vector<std::pair<std::string, PluginManager::PluginStatus>> PluginManager::getAllPluginStatuses() const {
        std::vector<std::pair<std::string, PluginStatus>> result;
        for (const auto& entry : registry_) {
            result.emplace_back(entry.first, getPluginStatus(entry.first));
        }
        return result;
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
        auto order = resolveUnloadOrder();
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            auto instIt = instances_.find(*it);
            if (instIt == instances_.end() || !instIt->second) {
                continue;
            }
            loader_.unloadPlugin(instIt->second->getName());
        }
        instances_.clear();
        statuses_.clear();
    }

    bool PluginManager::isVersionCompatible(const std::string& current,
                                            const std::string& required) const {
        auto currentTokens = tokenizeVersion(current);
        auto requiredTokens = tokenizeVersion(required);

        size_t maxSize = std::max(currentTokens.size(), requiredTokens.size());
        currentTokens.resize(maxSize, 0);
        requiredTokens.resize(maxSize, 0);

        for (size_t i = 0; i < maxSize; ++i) {
            if (currentTokens[i] < requiredTokens[i]) {
                return false;
            }
            if (currentTokens[i] > requiredTokens[i]) {
                return true;
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
            } catch (const std::exception& e) {
                Logger::instance().warn("PluginManager: Non-numeric version segment '" + segment + "' in '" + version + "'", "PluginManager");
                tokens.push_back(0);
            }
        }
        return tokens;
    }

    std::vector<std::string> PluginManager::resolveUnloadOrder() const {
        std::vector<std::string> order;
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> visiting;

        auto visit = [&](auto&& self, const std::string& node) -> bool {
            if (visited.count(node)) {
                return true;
            }
            if (visiting.count(node)) {
                Logger::instance().warn("PluginManager: Cycle detected while resolving unload order for '" + node + "'", "PluginManager");
                return false;
            }
            visiting.insert(node);
            auto it = registry_.find(node);
            if (it != registry_.end()) {
                for (const auto& dep : it->second.dependencies) {
                    if (!self(self, dep)) {
                        return false;
                    }
                }
            }
            visiting.erase(node);
            visited.insert(node);
            order.push_back(node);
            return true;
        };

        for (const auto& entry : registry_) {
            visit(visit, entry.first);
        }
        return order;
    }

} // namespace SentinelFS
