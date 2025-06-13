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
    if (event_queue != nullptr) {
        ESP_LOGW(TAG, "EventBus already initialized");
        return ESP_OK;
    }
    
    max_queue_size = queue_size;
    event_queue = xQueueCreate(queue_size, sizeof(Event*));
    
    if (event_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_ERR_NO_MEM;
    }
    
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
    
    // Apply filter if set
    if (event_filter && !event_filter(*event)) {
        delete event;
        return ESP_OK;
    }
    
    stats.total_published++;
    
    // If called from main task and not paused, process immediately
    if (xTaskGetCurrentTaskHandle() == xTaskGetHandle("main") && !processing_paused) {
        // Process immediately
        for (const auto& sub : subscriptions) {
            if (matches_pattern(sub->pattern, event->type)) {
                // Безпечне виконання handler без винятків
                // Припускаємо, що handler може викликати помилки,
                // але не повертає код помилки
                ModESP::safe_execute("event_handler", [&]() -> esp_err_t {
                    sub->handler(*event);
                    sub->call_count++;
                    return ESP_OK;
                });
            }
        }
        stats.total_processed++;
        delete event;
        return ESP_OK;
    }
    
    // Queue for later processing
    Event* event_ptr = event;
    if (xQueueSend(event_queue, &event_ptr, 0) != pdTRUE) {
        stats.total_dropped++;
        delete event;
        ESP_LOGW(TAG, "Event queue full, dropping: %s", type.c_str());
        return ESP_ERR_NO_MEM;
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
    
    subscriptions.push_back(std::move(sub));
    stats.total_subscriptions = subscriptions.size();
    
    ESP_LOGD(TAG, "Subscribed to %s (handle: %lu)", pattern.c_str(), sub->handle);
    
    return sub->handle;
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
        return 0;
    }
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    size_t processed = 0;
    
    Event* event = nullptr;
    while ((esp_timer_get_time() / 1000 - start_time) < max_ms) {
        if (xQueueReceive(event_queue, &event, 0) != pdTRUE) {
            break;
        }
        
        uint32_t event_start = esp_timer_get_time();
        
        // Dispatch to all matching subscribers
        for (const auto& sub : subscriptions) {
            if (matches_pattern(sub->pattern, event->type)) {
                // Безпечне виконання handler без винятків
                esp_err_t result = ModESP::safe_execute("event_handler", [&]() -> esp_err_t {
                    sub->handler(*event);
                    sub->call_count++;
                    return ESP_OK;
                });
                
                if (result != ESP_OK) {
                    error_collector.add(result, std::string("handler_") + event->type);
                }
                
                if (result != ESP_OK) {
                    error_collector.add(result, std::string("handler_") + event->type);
                }
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
    return error_collector.has_errors();
}

size_t get_error_count() {
    return error_collector.error_count();
}

void clear_errors() {
    error_collector.clear();
}

void log_errors(const char* tag) {
    error_collector.log_all(tag);
}

void set_filter(EventFilter filter) {
    event_filter = filter;
}

void clear_filter() {
    event_filter = nullptr;
}

} // namespace EventBus