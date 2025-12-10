#pragma once

#include "IFileWatcher.h"
#include <string>

namespace SentinelFS {

    /**
     * @brief Windows implementation of IFileWatcher using ReadDirectoryChangesW (skeleton).
     *
     * On non-Windows platforms, all methods return false and do nothing.
     */
    class Win32Watcher : public IFileWatcher {
    public:
        Win32Watcher();
        ~Win32Watcher() override;

        Win32Watcher(const Win32Watcher&) = delete;
        Win32Watcher& operator=(const Win32Watcher&) = delete;

        bool initialize(EventCallback callback) override;
        void shutdown() override;
        bool addWatch(const std::string& path) override;
        bool removeWatch(const std::string& path) override;

    private:
    #ifdef _WIN32
        EventCallback callback_;
        // TODO: HANDLEs and worker thread for ReadDirectoryChangesW
    #else
        EventCallback callback_;
    #endif
    };

} // namespace SentinelFS
