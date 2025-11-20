#pragma once
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <any>
#include <utility>
#include <mutex>

namespace SentinelFS {

    using EventCallback = std::function<void(const std::any&)>;
    using EventFilter = std::function<bool(const std::any&)>;

    class EventBus {
    public:
        struct Subscription {
            EventCallback callback;
            int priority;
            EventFilter filter;
        };

        /**
         * @brief Subscribe to an event.
         * @param eventName The name of the event.
         * @param callback The function to call when the event is published.
         */
        void subscribe(const std::string& eventName,
                       EventCallback callback,
                       int priority = 0,
                       EventFilter filter = nullptr);

        /**
         * @brief Publish an event.
         * @param eventName The name of the event.
         * @param data The data associated with the event.
         */
        void publish(const std::string& eventName, const std::any& data);

        /**
         * @brief Publish multiple events in order.
         */
        void publishBatch(const std::vector<std::pair<std::string, std::any>>& events);

    private:
        std::unordered_map<std::string, std::vector<Subscription>> subscribers_;
        mutable std::mutex mutex_;
    };

}
