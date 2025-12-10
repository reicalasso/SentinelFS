#include "EventBus.h"
#include "Logger.h"

#include <memory>
#include <optional>
#include <utility>

namespace SentinelFS {

    void EventBus::subscribe(const std::string& eventName,
                             EventCallback callback,
                             int priority,
                             EventFilter filter) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto& subsPtr = subscribers_[eventName];
        if (!subsPtr) {
            subsPtr = std::make_shared<std::vector<Subscription>>();
        }
        auto subsCopy = std::make_shared<std::vector<Subscription>>(*subsPtr);
        Subscription subscription{std::move(callback), priority, std::move(filter)};

        auto insertPos = subsCopy->begin();
        for (; insertPos != subsCopy->end(); ++insertPos) {
            if (priority > insertPos->priority) {
                break;
            }
        }
        subsCopy->insert(insertPos, std::move(subscription));
        subsPtr.swap(subsCopy);
    }

    void EventBus::publish(const std::string& eventName, const std::any& data) {
        std::shared_ptr<std::vector<Subscription>> snapshot;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = subscribers_.find(eventName);
            if (it == subscribers_.end() || !it->second) {
                return;
            }
            snapshot = it->second;
        }

        // Get or create metrics entry (lock-free after initial creation)
        Metrics* storedMetrics = nullptr;
        {
            std::lock_guard<std::mutex> metricsLock(metricsMutex_);
            storedMetrics = &metrics_[eventName];
        }

        for (const auto& sub : *snapshot) {
            storedMetrics->published.fetch_add(1, std::memory_order_relaxed);
            if (sub.filter && !sub.filter(data)) {
                storedMetrics->filtered.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            try {
                sub.callback(data);
            } catch (const std::exception& e) {
                storedMetrics->failed.fetch_add(1, std::memory_order_relaxed);
                Logger::instance().warn("EventBus callback threw: " + std::string(e.what()), "EventBus");
            }
        }
        
        // Invoke callback outside of any lock
        MetricsCallback cb;
        {
            std::lock_guard<std::mutex> metricsLock(metricsMutex_);
            cb = metricsCallback_;
        }
        if (cb) {
            cb(eventName, *storedMetrics);
        }
    }

    void EventBus::publishBatch(const std::vector<std::pair<std::string, std::any>>& events) {
        for (const auto& event : events) {
            publish(event.first, event.second);
        }
    }

    void EventBus::setMetricsCallback(MetricsCallback callback) {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        metricsCallback_ = std::move(callback);
    }

    std::optional<EventBus::Metrics> EventBus::getMetrics(const std::string& eventName) const {
        std::lock_guard<std::mutex> lock(metricsMutex_);
        auto it = metrics_.find(eventName);
        if (it == metrics_.end()) {
            return std::nullopt;
        }
        return it->second;
    }

}
