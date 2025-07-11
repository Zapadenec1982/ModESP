#include "event_bus.h"
#include <esp_log.h>
#include <algorithm>
#include <cstring>
#include "esp_mac.h"

static const char* TAG = "EventBus";

namespace EventBus {

// Internal structures
struct Subscription {
    SubscriptionHandle handle;
    std::string pattern;
    EventHandler handler;
    uint32_t call_count = 0;
};

// Internal state
static QueueHandle_t event_queue = nullptr;
static std::vector<std::unique_ptr<Subscription>> subscriptions;
static SubscriptionHandle next_handle = 1;
static bool processing_paused = false;
static EventFilter event_filter = nullptr;

// Statistics
static Stats stats = {0, 0, 0, 0, 0, 0, 0};

// Configuration
static size_t max_queue_size = 32;

// Helper functions
static bool matches_pattern(const std::string& pattern, const std::string& type) {
    if (pattern.empty() || pattern == "*") return true;
    if (pattern == type) return true;
    
    // Wildcard matching
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        return type.substr(0, prefix.length()) == prefix;
    }
    
    return false;
}

esp_err_t init(size_t queue_size) {
    // REAL FIX: Properly cleanup if already initialized
    if (event_queue != nullptr) {
        ESP_LOGW(TAG, "EventBus already initialized, cleaning up previous state");
        
        // Clear existing queue
        Event* event = nullptr;
        while (xQueueReceive(event_queue, &event, 0) == pdTRUE) {
            delete event;
        }
        
        // Delete old queue
        vQueueDelete(event_queue);
        event_queue = nullptr;
        
        // Clear subscriptions
        subscriptions.clear();
        
        // Reset stats
        stats = {0, 0, 0, 0, 0, 0, 0};
        next_handle = 1;
    }
    
    max_queue_size = queue_size;
    event_queue = xQueueCreate(queue_size, sizeof(Event*));
    
    if (event_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Reserve memory for subscriptions to avoid reallocations
    subscriptions.reserve(32);
    
    ESP_LOGI(TAG, "EventBus initialized with queue size %d", queue_size);
    return ESP_OK;
}

esp_err_t publish(const std::string& type, const nlohmann::json& data, Priority priority) {
    if (event_queue == nullptr) {
        ESP_LOGE(TAG, "EventBus not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create event on heap
    Event* event = new Event(type, data, priority);
    
    // Apply filter if set - FIXED: Apply filter before incrementing stats
    if (event_filter && !event_filter(*event)) {
        delete event;
        return ESP_OK;  // Event filtered out, not an error
    }
    
    stats.total_published++;
    
    // FIXED: Always queue events to ensure proper priority ordering
    // and to allow queue overflow testing
    Event* event_ptr = event;
    if (xQueueSend(event_queue, &event_ptr, 0) != pdTRUE) {
        stats.total_dropped++;
        delete event;
        ESP_LOGW(TAG, "Event queue full, dropping: %s", type.c_str());
        return ESP_ERR_NO_MEM;  // Real queue overflow
    }
    
    return ESP_OK;
}

esp_err_t publish_from_isr(const std::string& type, const nlohmann::json& data,
                           Priority priority, BaseType_t* higher_priority_task_woken) {
    if (event_queue == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    
    Event* event = new Event(type, data, priority);
    Event* event_ptr = event;
    
    if (xQueueSendFromISR(event_queue, &event_ptr, higher_priority_task_woken) != pdTRUE) {
        delete event;
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

SubscriptionHandle subscribe(const std::string& pattern, EventHandler handler) {
    if (xTaskGetCurrentTaskHandle() != xTaskGetHandle("main")) {
        ESP_LOGE(TAG, "Subscribe must be called from main task");
        return 0;
    }
    
    auto sub = std::make_unique<Subscription>();
    sub->handle = next_handle++;
    sub->pattern = pattern;
    sub->handler = handler;
    
    SubscriptionHandle handle = sub->handle; // Зберігаємо handle перед переміщенням
    
    subscriptions.push_back(std::move(sub));
    stats.total_subscriptions = subscriptions.size();
    
    ESP_LOGD(TAG, "Subscribed to %s (handle: %lu)", pattern.c_str(), handle);
    
    return handle;
}

esp_err_t unsubscribe(SubscriptionHandle handle) {
    auto it = std::remove_if(subscriptions.begin(), subscriptions.end(),
        [handle](const auto& sub) { return sub->handle == handle; });
    
    if (it != subscriptions.end()) {
        subscriptions.erase(it, subscriptions.end());
        stats.total_subscriptions = subscriptions.size();
        return ESP_OK;
    }
    
    return ESP_ERR_NOT_FOUND;
}

size_t process(uint32_t max_ms) {
    if (processing_paused || event_queue == nullptr) {
        return 0;  // FIXED: Properly respect pause state
    }
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    size_t processed = 0;
    
    // FIXED: Collect all available events for priority sorting
    std::vector<Event*> events_to_process;
    
    Event* event = nullptr;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE) {
        if (event != nullptr) {  // Safety check
            events_to_process.push_back(event);
        }
    }
    
    // FIXED: Sort by priority (CRITICAL=0, HIGH=1, NORMAL=2, LOW=3)
    // Lower number = higher priority, so sort ascending
    std::sort(events_to_process.begin(), events_to_process.end(),
        [](const Event* a, const Event* b) {
            return static_cast<int>(a->priority) < static_cast<int>(b->priority);
        });
    
    // Process sorted events within time budget
    for (Event* event : events_to_process) {
        if ((esp_timer_get_time() / 1000 - start_time) >= max_ms) {
            // Time budget exceeded, put remaining events back in queue
            Event* event_ptr = event;
            if (xQueueSend(event_queue, &event_ptr, 0) != pdTRUE) {
                // Queue full, have to drop the event
                ESP_LOGW(TAG, "Had to drop event due to time budget + full queue");
                delete event;
                stats.total_dropped++;
            }
            continue;
        }
        
        uint32_t event_start = esp_timer_get_time();
        
        // Dispatch to all matching subscribers
        for (const auto& sub : subscriptions) {
            if (matches_pattern(sub->pattern, event->type) && sub->handler) {
                sub->handler(*event);
                sub->call_count++;
            }
        }
        
        uint32_t event_time = esp_timer_get_time() - event_start;
        stats.avg_process_time_us = (stats.avg_process_time_us + event_time) / 2;
        
        delete event;
        processed++;
        stats.total_processed++;
    }
    
    return processed;
}

size_t get_queue_size() {
    if (event_queue == nullptr) return 0;
    return uxQueueMessagesWaiting(event_queue);
}

bool is_queue_full() {
    if (event_queue == nullptr) return false;
    return uxQueueSpacesAvailable(event_queue) == 0;
}

void clear() {
    if (event_queue == nullptr) return;
    
    Event* event = nullptr;
    while (xQueueReceive(event_queue, &event, 0) == pdTRUE) {
        delete event;
    }
}

void pause() {
    processing_paused = true;
}

void resume() {
    processing_paused = false;
}

bool is_paused() {
    return processing_paused;
}

Stats get_stats() {
    stats.queue_size = get_queue_size();
    stats.max_queue_size = max_queue_size;
    return stats;
}

void reset_stats() {
    stats.total_published = 0;
    stats.total_processed = 0;
    stats.total_dropped = 0;
    stats.avg_process_time_us = 0;
}

bool has_errors() {
    return false; // Temporarily disabled
}

size_t get_error_count() {
    return 0; // Temporarily disabled
}

void clear_errors() {
    // Temporarily disabled
}

void log_errors(const char* tag) {
    // Temporarily disabled
}

void set_filter(EventFilter filter) {
    event_filter = filter;
}

void clear_filter() {
    event_filter = nullptr;
}

} // namespace EventBus