#include "PluginLoader.h"
#include "IFileAPI.h"
#include "EventBus.h"
#include "MetricsCollector.h"

#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <any>

using namespace SentinelFS;

static std::atomic<bool> g_running{true};

static void handleSignal(int) {
    g_running = false;
}

int main(int argc, char** argv) {
    EventBus eventBus;
    PluginLoader loader;

    // Assuming running from build root: plugins live under ./plugins/filesystem/
    std::string pluginPath = "plugins/filesystem/libfilesystem_plugin.so";
    if (argc > 2) {
        pluginPath = argv[2];
    }

    std::cout << "Loading filesystem plugin from: " << pluginPath << std::endl;
    auto plugin = loader.loadPlugin(pluginPath, &eventBus);
    if (!plugin) {
        std::cerr << "Failed to load filesystem plugin" << std::endl;
        return 1;
    }

    auto fsPlugin = std::dynamic_pointer_cast<IFileAPI>(plugin);
    if (!fsPlugin) {
        std::cerr << "Loaded plugin does not implement IFileAPI" << std::endl;
        return 1;
    }

    std::string watchPath = (argc > 1) ? argv[1] : std::string("./watched_folder");
    std::cout << "Starting watcher on: " << watchPath << std::endl;
    fsPlugin->startWatching(watchPath);

    eventBus.subscribe("FILE_CREATED", [](const std::any& data) {
        try {
            std::string path = std::any_cast<std::string>(data);
            std::cout << "[EVENT] CREATED: " << path << std::endl;
        } catch (...) {}
    });

    eventBus.subscribe("FILE_MODIFIED", [](const std::any& data) {
        try {
            std::string path = std::any_cast<std::string>(data);
            std::cout << "[EVENT] MODIFIED: " << path << std::endl;
        } catch (...) {}
    });

    eventBus.subscribe("FILE_DELETED", [](const std::any& data) {
        try {
            std::string path = std::any_cast<std::string>(data);
            std::cout << "[EVENT] DELETED: " << path << std::endl;
        } catch (...) {}
    });

    eventBus.subscribe("FILE_RENAMED", [](const std::any& data) {
        try {
            std::string path = std::any_cast<std::string>(data);
            std::cout << "[EVENT] RENAMED: " << path << std::endl;
        } catch (...) {}
    });

    // Handle Ctrl+C
    std::signal(SIGINT, handleSignal);

    auto& metrics = MetricsCollector::instance();

    std::cout << "Watcher running. Press Ctrl+C to exit." << std::endl;

    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        auto sync = metrics.getSyncMetrics();
        std::cout << "[METRICS] filesWatched=" << sync.filesWatched
                  << " filesModified=" << sync.filesModified
                  << " filesDeleted=" << sync.filesDeleted << std::endl;
    }

    std::cout << "Watcher stopped." << std::endl;
    return 0;
}
