#include "event_bus.h"
#include <esp_log.h>
#include <algorithm>

namespace ModuChill {

static const char* TAG = "EventBus";

EventBus* EventBus::instance_ = nullptr;

EventBus::EventBus() 
    : next_handle_(1)
    , events_published_(0)
    , events_processed_(0)
    , events_dropped_(0) {
    queue_mutex_ = xSemaphoreCreateMutex();
    configASSERT(queue_mutex_);
}

EventBus::~EventBus() {
    if (queue_mutex_) {
        vSemaphoreDelete(queue_mutex_);
    }
}

EventBus* EventBus::get_instance() {
    if (!instance_) {
        instance_ = new EventBus();
    }
    return instance_;
}

void EventBus::init() {
    ESP_LOGI(TAG, "EventBus initialized");
}

void EventBus::deinit() {
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}

void EventBus::publish(const std::string& type, const json& data, EventPriority priority) {
    auto* bus = get_instance();
    
    Event event(type, data, priority);
    
    // If called from main task, dispatch immediately
    if (xTaskGetCurrentTaskHandle() == xTaskGetHandle("main")) {
        bus->dispatch_event(event);
    } else {
        // Queue for later processing
        if (xSemaphoreTake(bus->queue_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (bus->event_queue_.size() < MAX_QUEUE_SIZE) {
                bus->event_queue_.push({event, xTaskGetCurrentTaskHandle()});
                bus->events_published_++;
            } else {
                bus->events_dropped_++;
                ESP_LOGW(TAG, "Event queue full, dropping: %s", type.c_str());
            }
            xSemaphoreGive(bus->queue_mutex_);
        } else {
            ESP_LOGE(TAG, "Failed to acquire queue mutex");
        }
    }
}

bool EventBus::matches_pattern(const std::string& pattern, const std::string& type) const {
    if (pattern.empty() || pattern == "*") {
        return true;
    }
    
    // Exact match
    if (pattern == type) {
        return true;
    }
    
    // Prefix match with wildcard
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.length() - 1);
        return type.substr(0, prefix.length()) == prefix;
    }
    
    return false;
}
void EventBus::dispatch_event(const Event& event) {
    // Find all matching subscriptions
    for (const auto& [prefix, subs] : subscriptions_) {
        for (const auto& sub : subs) {
            if (matches_pattern(sub->pattern, event.type)) {
                try {
                    sub->handler(event);
                    sub->call_count++;
                } catch (const std::exception& e) {
                    ESP_LOGE(TAG, "Exception in handler for %s: %s", 
                             event.type.c_str(), e.what());
                } catch (...) {
                    ESP_LOGE(TAG, "Unknown exception in handler for %s", 
                             event.type.c_str());
                }
            }
        }
    }
    events_processed_++;
}

void EventBus::process(uint32_t max_ms) {
    auto* bus = get_instance();
    bus->process_queued_events(max_ms);
}

void EventBus::process_queued_events(uint32_t max_ms) {
    uint32_t start_time = esp_timer_get_time() / 1000;
    
    while ((esp_timer_get_time() / 1000 - start_time) < max_ms) {
        QueuedEvent queued_event;
        bool has_event = false;
        
        // Get next event
        if (xSemaphoreTake(queue_mutex_, 0) == pdTRUE) {
            if (!event_queue_.empty()) {
                queued_event = event_queue_.front();
                event_queue_.pop();
                has_event = true;
            }
            xSemaphoreGive(queue_mutex_);
        }
        
        if (!has_event) {
            break;
        }
        
        // Dispatch event
        dispatch_event(queued_event.event);
    }
}

SubscriptionHandle EventBus::subscribe(const std::string& pattern, EventHandler handler) {
    auto* bus = get_instance();
    
    if (xTaskGetCurrentTaskHandle() != xTaskGetHandle("main")) {
        ESP_LOGE(TAG, "Subscribe must be called from main task");
        return 0;
    }
    
    auto sub = std::make_unique<Subscription>();
    sub->handle = bus->next_handle_++;
    sub->pattern = pattern;
    sub->handler = handler;
    sub->call_count = 0;
    
    // Store by prefix for efficient lookup
    std::string prefix = bus->get_pattern_prefix(pattern);
    bus->subscriptions_[prefix].push_back(std::move(sub));
    
    ESP_LOGD(TAG, "Subscribed to %s (handle: %lu)", pattern.c_str(), sub->handle);
    
    return sub->handle;
}

void EventBus::unsubscribe(SubscriptionHandle handle) {
    auto* bus = get_instance();
    
    for (auto& [prefix, subs] : bus->subscriptions_) {
        auto it = std::remove_if(subs.begin(), subs.end(),
            [handle](const auto& sub) { return sub->handle == handle; });
        
        if (it != subs.end()) {
            subs.erase(it, subs.end());
            ESP_LOGD(TAG, "Unsubscribed handle: %lu", handle);
            return;
        }
    }
}

void EventBus::unsubscribe_all(const std::string& pattern) {
    auto* bus = get_instance();
    std::string prefix = bus->get_pattern_prefix(pattern);
    
    bus->subscriptions_.erase(prefix);
    ESP_LOGD(TAG, "Unsubscribed all from %s", pattern.c_str());
}

std::string EventBus::get_pattern_prefix(const std::string& pattern) const {
    size_t dot_pos = pattern.find('.');
    if (dot_pos != std::string::npos) {
        return pattern.substr(0, dot_pos);
    }
    return pattern;
}

uint32_t EventBus::get_queue_size() {
    auto* bus = get_instance();
    uint32_t size = 0;
    
    if (xSemaphoreTake(bus->queue_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
        size = bus->event_queue_.size();
        xSemaphoreGive(bus->queue_mutex_);
    }
    
    return size;
}

void EventBus::clear() {
    auto* bus = get_instance();
    
    if (xSemaphoreTake(bus->queue_mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
        std::queue<QueuedEvent> empty;
        std::swap(bus->event_queue_, empty);
        xSemaphoreGive(bus->queue_mutex_);
    }
}

bool EventBus::has_subscribers(const std::string& pattern) {
    auto* bus = get_instance();
    std::string prefix = bus->get_pattern_prefix(pattern);
    
    return bus->subscriptions_.find(prefix) != bus->subscriptions_.end() &&
           !bus->subscriptions_[prefix].empty();
}

} // namespace ModuChill