#pragma once

#include "IFileWatcher.h"
#include <string>

namespace SentinelFS {

    /**
     * @brief macOS implementation of IFileWatcher using FSEvents (skeleton).
     *
     * On non-macOS platforms, all methods return false and do nothing.
     */
    class FSEventsWatcher : public IFileWatcher {
    public:
        FSEventsWatcher();
        ~FSEventsWatcher() override;

        FSEventsWatcher(const FSEventsWatcher&) = delete;
        FSEventsWatcher& operator=(const FSEventsWatcher&) = delete;

        bool initialize(EventCallback callback) override;
        void shutdown() override;
        bool addWatch(const std::string& path) override;
        bool removeWatch(const std::string& path) override;

    private:
    #ifdef __APPLE__
        EventCallback callback_;
        // TODO: FSEventStreamRef and run loop integration.
    #else
        EventCallback callback_;
    #endif
    };

} // namespace SentinelFS
