/**
 * @file event_bus.h
 * @brief Thread-safe publish-subscribe event system for ModuChill
 * 
 * Provides asynchronous communication between modules with pattern matching
 * and priority-based event delivery.
 */

#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include "esp_err.h"
#include "nlohmann/json.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "error_handling.h"  // Додаємо підтримку обробки помилок

namespace EventBus {

// Error collector для відстеження помилок в обробниках
static ModESP::ErrorCollector error_collector;

/**
 * @brief Event priority levels
 */
enum class Priority : uint8_t {
    CRITICAL = 0,  // Highest priority - safety events
    HIGH = 1,      // Important operational events
    NORMAL = 2,    // Regular events (default)
    LOW = 3        // Background events
};

/**
 * @brief Event structure
 */
struct Event {
    std::string type;           // Event type/identifier
    nlohmann::json data;        // Event payload
    uint64_t timestamp;         // esp_timer_get_time() when published
    Priority priority;          // Execution priority
    
    Event() : timestamp(0), priority(Priority::NORMAL) {}
    Event(const std::string& t, const nlohmann::json& d, Priority p = Priority::NORMAL)
        : type(t), data(d), timestamp(esp_timer_get_time()), priority(p) {}
};

/**
 * @brief Event handler callback
 * @param event The event to handle
 */
using EventHandler = std::function<void(const Event& event)>;

/**
 * @brief Subscription handle for unsubscribing
 */
using SubscriptionHandle = uint32_t;

/**
 * @brief Initialize EventBus
 * 
 * Must be called once during system initialization.
 * Creates event queue and processing task.
 * 
 * @param queue_size Maximum events in queue (default 32)
 * @return ESP_OK on success
 */
esp_err_t init(size_t queue_size = 32);

/**
 * @brief Publish event
 * 
 * Thread-safe. Can be called from any task or ISR.
 * If called from non-main task, event is queued for processing.
 * If called from main task, may process immediately.
 * 
 * @param type Event type (e.g., "sensor.temperature.updated")
 * @param data Event data as JSON
 * @param priority Event priority
 * @return ESP_OK on success, ESP_ERR_NO_MEM if queue full
 */
esp_err_t publish(const std::string& type, 
                 const nlohmann::json& data = nlohmann::json{},
                 Priority priority = Priority::NORMAL);

/**
 * @brief Publish event from ISR
 * 
 * Special version for interrupt service routines.
 * 
 * @param type Event type
 * @param data Event data
 * @param priority Event priority
 * @param higher_priority_task_woken Set to true if context switch needed
 * @return ESP_OK on success
 */
esp_err_t publish_from_isr(const std::string& type,
                           const nlohmann::json& data,
                           Priority priority,
                           BaseType_t* higher_priority_task_woken);

/**
 * @brief Subscribe to event pattern
 * 
 * Pattern matching:
 * - "" matches all events
 * - "sensor.*" matches "sensor.temp", "sensor.humidity", etc.
 * - "sensor.*.updated" matches "sensor.temp.updated", etc.
 * - Exact type matches only that event
 * 
 * Must be called from main task only.
 * 
 * @param pattern Event type pattern
 * @param handler Callback function
 * @return Subscription handle
 */
SubscriptionHandle subscribe(const std::string& pattern, EventHandler handler);

/**
 * @brief Unsubscribe from events
 * 
 * Must be called from main task only.
 * 
 * @param handle Subscription handle from subscribe()
 * @return ESP_OK if unsubscribed
 */
esp_err_t unsubscribe(SubscriptionHandle handle);

/**
 * @brief Process event queue
 * 
 * Processes queued events for specified time budget.
 * Called automatically from main loop.
 * 
 * @param max_ms Maximum milliseconds to process
 * @return Number of events processed
 */
size_t process(uint32_t max_ms = 2);

/**
 * @brief Get current queue size
 * @return Number of events waiting in queue
 */
size_t get_queue_size();

/**
 * @brief Check if queue is full
 * @return true if queue is full
 */
bool is_queue_full();

/**
 * @brief Clear all pending events
 */
void clear();

/**
 * @brief Pause event processing
 * 
 * Useful during critical operations.
 */
void pause();

/**
 * @brief Resume event processing
 */
void resume();

/**
 * @brief Check if processing is paused
 * @return true if paused
 */
bool is_paused();

/**
 * @brief Get event statistics
 */
struct Stats {
    size_t total_published;     // Total events published
    size_t total_processed;     // Total events processed
    size_t total_dropped;       // Events dropped (queue full)
    size_t queue_size;          // Current queue size
    size_t max_queue_size;      // Maximum queue size
    size_t total_subscriptions; // Active subscriptions
    uint32_t avg_process_time_us; // Average event processing time
};

/**
 * @brief Get current statistics
 * @return Event statistics
 */
Stats get_stats();

/**
 * @brief Reset statistics counters
 */
void reset_stats();

/**
 * @brief Get error information
 * @return True if there are errors
 */
bool has_errors();

/**
 * @brief Get total error count
 * @return Number of errors since init or last clear
 */
size_t get_error_count();

/**
 * @brief Clear all error records
 */
void clear_errors();

/**
 * @brief Log all errors
 * @param tag Tag for logging
 */
void log_errors(const char* tag = "EventBus");

/**
 * @brief Set event filter
 * 
 * Allows filtering events before they enter the queue.
 * Useful for rate limiting or security.
 * 
 * @param filter Function returning true to allow event
 */
using EventFilter = std::function<bool(const Event&)>;
void set_filter(EventFilter filter);

/**
 * @brief Clear event filter
 */
void clear_filter();

} // namespace EventBus

#endif // EVENT_BUS_H
