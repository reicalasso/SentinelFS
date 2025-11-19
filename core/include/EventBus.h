#pragma once
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <any>

namespace SentinelFS {

    using EventCallback = std::function<void(const std::any&)>;

    class EventBus {
    public:
        /**
         * @brief Subscribe to an event.
         * @param eventName The name of the event.
         * @param callback The function to call when the event is published.
         */
        void subscribe(const std::string& eventName, EventCallback callback);

        /**
         * @brief Publish an event.
         * @param eventName The name of the event.
         * @param data The data associated with the event.
         */
        void publish(const std::string& eventName, const std::any& data);

    private:
        std::unordered_map<std::string, std::vector<EventCallback>> subscribers_;
    };

}
