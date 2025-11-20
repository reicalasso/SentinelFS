#include "EventBus.h"

#include <utility>

namespace SentinelFS {

    void EventBus::subscribe(const std::string& eventName,
                             EventCallback callback,
                             int priority,
                             EventFilter filter) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& subs = subscribers_[eventName];

        Subscription subscription{std::move(callback), priority, std::move(filter)};
        auto insertPos = subs.begin();
        for (; insertPos != subs.end(); ++insertPos) {
            if (priority > insertPos->priority) {
                break;
            }
        }
        subs.insert(insertPos, std::move(subscription));
    }

    void EventBus::publish(const std::string& eventName, const std::any& data) {
        std::vector<Subscription> targets;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = subscribers_.find(eventName);
            if (it == subscribers_.end()) {
                return;
            }
            targets = it->second;
        }

        for (auto& sub : targets) {
            if (sub.filter && !sub.filter(data)) {
                continue;
            }
            sub.callback(data);
        }
    }

    void EventBus::publishBatch(const std::vector<std::pair<std::string, std::any>>& events) {
        for (const auto& event : events) {
            publish(event.first, event.second);
        }
    }

}
