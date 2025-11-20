#pragma once

#include <string>
#include <functional>
#include <optional>

namespace SentinelFS {

    enum class WatchEventType {
        Create,
        Modify,
        Delete,
        Rename
    };

    struct WatchEvent {
        WatchEventType type;
        std::string path;
        std::optional<std::string> newPath; // used for rename target when available
    };

    /**
     * @brief Cross-platform filesystem watcher interface.
     */
    class IFileWatcher {
    public:
        using EventCallback = std::function<void(const WatchEvent&)>;

        virtual ~IFileWatcher() = default;

        virtual bool initialize(EventCallback callback) = 0;
        virtual void shutdown() = 0;
        virtual bool addWatch(const std::string& path) = 0;
        virtual bool removeWatch(const std::string& path) = 0;
    };

} // namespace SentinelFS
