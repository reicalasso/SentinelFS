#ifndef SFS_EVENT_BUS_H
#define SFS_EVENT_BUS_H

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <any>

namespace sfs {
namespace core {

/**
 * @brief Event structure
 * 
 * Generic event container that can hold any type of data.
 * Events are identified by their type string.
 */
struct Event {
    std::string type;           // Event type (e.g., "fs.file_changed")
    std::any data;              // Event payload (type-erased)
    uint64_t timestamp;         // Event timestamp (milliseconds since epoch)
    std::string source;         // Source plugin name
    
    Event(const std::string& t, std::any d, const std::string& src = "")
        : type(t), data(std::move(d)), source(src) {
        timestamp = get_current_time_ms();
    }
    
private:
    static uint64_t get_current_time_ms();
};

/**
 * @brief Event handler callback type
 */
using EventHandler = std::function<void(const Event&)>;

/**
 * @brief EventBus - Core communication mechanism
 * 
 * Provides publish-subscribe pattern for plugin communication.
 * Plugins can subscribe to specific event types and publish events
 * without knowing about each other.
 * 
 * Thread-safe implementation.
 */
class EventBus {
public:
    EventBus();
    ~EventBus();
    
    // Prevent copying
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
    /**
     * @brief Subscribe to an event type
     * 
     * @param event_type Event type to subscribe to (e.g., "fs.file_changed")
     * @param handler Callback function to invoke when event occurs
     * @return Subscription ID (used for unsubscribing)
     */
    uint64_t subscribe(const std::string& event_type, EventHandler handler);
    
    /**
     * @brief Unsubscribe from an event
     * 
     * @param subscription_id ID returned by subscribe()
     */
    void unsubscribe(uint64_t subscription_id);
    
    /**
     * @brief Publish an event
     * 
     * Synchronously calls all registered handlers for this event type.
     * Handlers are called in the order they were registered.
     * 
     * @param event Event to publish
     */
    void publish(const Event& event);
    
    /**
     * @brief Publish an event asynchronously
     * 
     * Queues the event for processing on a background thread.
     * 
     * @param event Event to publish
     */
    void publish_async(const Event& event);
    
    /**
     * @brief Clear all subscriptions
     */
    void clear();
    
    /**
     * @brief Get subscription count for an event type
     * 
     * @param event_type Event type
     * @return Number of active subscriptions
     */
    size_t subscription_count(const std::string& event_type) const;

private:
    struct Subscription {
        uint64_t id;
        std::string event_type;
        EventHandler handler;
    };
    
    mutable std::mutex mutex_;
    std::map<std::string, std::vector<Subscription>> subscriptions_;
    uint64_t next_subscription_id_;
    
    // Async event queue (for future implementation)
    bool running_;
};

} // namespace core
} // namespace sfs

#endif // SFS_EVENT_BUS_H
