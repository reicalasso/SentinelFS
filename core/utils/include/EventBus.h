#pragma once
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <any>
#include <utility>
#include <mutex>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <atomic>

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

        struct Metrics {
            std::atomic<size_t> published{0};
            std::atomic<size_t> filtered{0};
            std::atomic<size_t> failed{0};
            
            // Copy constructor for returning metrics
            Metrics() = default;
            Metrics(const Metrics& other) 
                : published(other.published.load())
                , filtered(other.filtered.load())
                , failed(other.failed.load()) {}
            Metrics& operator=(const Metrics& other) {
                published = other.published.load();
                filtered = other.filtered.load();
                failed = other.failed.load();
                return *this;
            }
        };

        using MetricsCallback = std::function<void(const std::string&, const Metrics&)>;

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

        void setMetricsCallback(MetricsCallback callback);
        std::optional<Metrics> getMetrics(const std::string& eventName) const;

    private:
        std::unordered_map<std::string, std::shared_ptr<std::vector<Subscription>>> subscribers_;
        mutable std::shared_mutex mutex_;
        mutable std::mutex metricsMutex_;
        std::unordered_map<std::string, Metrics> metrics_;
        MetricsCallback metricsCallback_;
    };

}
