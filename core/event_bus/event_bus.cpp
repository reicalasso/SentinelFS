#include "event_bus.h"
#include <chrono>
#include <algorithm>

namespace sfs {
namespace core {

uint64_t Event::get_current_time_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

EventBus::EventBus()
    : next_subscription_id_(1), running_(true) {
}

EventBus::~EventBus() {
    running_ = false;
    clear();
}

uint64_t EventBus::subscribe(const std::string& event_type, EventHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t id = next_subscription_id_++;
    Subscription sub{id, event_type, std::move(handler)};
    
    subscriptions_[event_type].push_back(std::move(sub));
    
    return id;
}

void EventBus::unsubscribe(uint64_t subscription_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [event_type, subs] : subscriptions_) {
        auto it = std::remove_if(subs.begin(), subs.end(),
            [subscription_id](const Subscription& sub) {
                return sub.id == subscription_id;
            });
        
        if (it != subs.end()) {
            subs.erase(it, subs.end());
            return;
        }
    }
}

void EventBus::publish(const Event& event) {
    std::vector<EventHandler> handlers;
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = subscriptions_.find(event.type);
        if (it != subscriptions_.end()) {
            for (const auto& sub : it->second) {
                handlers.push_back(sub.handler);
            }
        }
    }
    
    // Call handlers outside of lock to prevent deadlocks
    for (const auto& handler : handlers) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            // Log error (will integrate with Logger later)
            // For now, silently catch to prevent one handler from breaking others
        }
    }
}

void EventBus::publish_async(const Event& event) {
    // TODO: Implement async queue with worker thread
    // For now, just call synchronously
    publish(event);
}

void EventBus::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    subscriptions_.clear();
}

size_t EventBus::subscription_count(const std::string& event_type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = subscriptions_.find(event_type);
    return (it != subscriptions_.end()) ? it->second.size() : 0;
}

} // namespace core
} // namespace sfs
