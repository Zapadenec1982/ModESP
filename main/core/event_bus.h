#pragma once

#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <nlohmann/json.hpp>

namespace ModuChill {

using json = nlohmann::json;

/**
 * Event priority levels
 */
enum class EventPriority : uint8_t {
    CRITICAL = 0,  // System safety events
    HIGH = 1,      // Real-time events
    NORMAL = 2,    // Standard events
    LOW = 3        // Background events
};

/**
 * Event structure
 */
struct Event {
    std::string type;         // Event identifier (e.g., "sensor.temp.updated")
    json data;                // Event payload
    uint32_t timestamp;       // When published (esp_timer_get_time())
    EventPriority priority;   // Execution priority
    
    Event(const std::string& t, const json& d, EventPriority p = EventPriority::NORMAL)
        : type(t), data(d), timestamp(esp_timer_get_time() / 1000), priority(p) {}
};

/**
 * Event handler function type
 */
using EventHandler = std::function<void(const Event&)>;

/**
 * Subscription handle for unsubscribing
 */
using SubscriptionHandle = uint32_t;

/**
 * EventBus - Thread-safe publish-subscribe system
 * 
 * Enables decoupled communication between modules using pattern matching.
 * Thread-safe for publishing from any task, subscription from main task only.
 */
class EventBus {
private:
    struct Subscription {
        SubscriptionHandle handle;
        std::string pattern;
        EventHandler handler;
        uint32_t call_count;
    };
    
    // Event queue for cross-task publishing
    struct QueuedEvent {
        Event event;
        TaskHandle_t publisher_task;
    };
    
    // Subscriptions organized by pattern prefix for fast lookup
    std::unordered_map<std::string, std::vector<std::unique_ptr<Subscription>>> subscriptions_;
    
    // Thread-safe event queue
    std::queue<QueuedEvent> event_queue_;
    SemaphoreHandle_t queue_mutex_;
    
    // Statistics
    uint32_t next_handle_;
    uint32_t events_published_;
    uint32_t events_processed_;
    uint32_t events_dropped_;
    
    // Configuration
    static constexpr size_t MAX_QUEUE_SIZE = 32;
    static constexpr size_t MAX_PATTERN_LENGTH = 64;
    
    // Singleton
    static EventBus* instance_;
    
    EventBus();
    ~EventBus();
    
    bool matches_pattern(const std::string& pattern, const std::string& type) const;
    std::string get_pattern_prefix(const std::string& pattern) const;
    void process_queued_events(uint32_t max_ms);
    void dispatch_event(const Event& event);

public:
    // Initialization
    static void init();
    static void deinit();
    
    // Publishing (thread-safe)
    static void publish(const std::string& type, const json& data = {}, 
                       EventPriority priority = EventPriority::NORMAL);
    
    // Subscription (main task only)
    static SubscriptionHandle subscribe(const std::string& pattern, EventHandler handler);
    static void unsubscribe(SubscriptionHandle handle);
    static void unsubscribe_all(const std::string& pattern);
    
    // Processing (main task only)
    static void process(uint32_t max_ms = 10);
    
    // Statistics
    static uint32_t get_queue_size();
    static uint32_t get_published_count() { return instance_->events_published_; }
    static uint32_t get_processed_count() { return instance_->events_processed_; }
    static uint32_t get_dropped_count() { return instance_->events_dropped_; }
    
    // Utilities
    static void clear();
    static bool has_subscribers(const std::string& pattern);
    
private:
    static EventBus* get_instance();
};

} // namespace ModuChill