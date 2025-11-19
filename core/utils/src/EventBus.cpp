#include "EventBus.h"

namespace SentinelFS {

    void EventBus::subscribe(const std::string& eventName, EventCallback callback) {
        subscribers_[eventName].push_back(callback);
    }

    void EventBus::publish(const std::string& eventName, const std::any& data) {
        if (subscribers_.find(eventName) != subscribers_.end()) {
            for (auto& callback : subscribers_[eventName]) {
                callback(data);
            }
        }
    }

}
