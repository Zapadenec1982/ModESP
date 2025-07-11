/**
 * @file pooled_event.h
 * @brief Memory pool integration for EventBus
 * 
 * Provides pooled allocation for Event objects to eliminate
 * heap fragmentation in the event system.
 */

#ifndef POOLED_EVENT_H
#define POOLED_EVENT_H

#include "event_bus.h"
#include "memory_pool.h"
#include <memory>

namespace ModESP {

/**
 * @brief Event with pooled memory allocation
 * 
 * Extends EventBus::Event with automatic memory pool management.
 * Automatically returns memory to pool on destruction.
 */
class PooledEvent : public EventBus::Event {
private:
    Memory::MemoryPoolManager* pool_manager_;
    void* allocated_block_;
    size_t block_size_;
    
    /**
     * @brief Private constructor - use create() factory method
     */
    PooledEvent(Memory::MemoryPoolManager* manager, void* block, size_t size)
        : pool_manager_(manager), allocated_block_(block), block_size_(size) {}

public:
    /**
     * @brief Destructor returns memory to pool
     */
    ~PooledEvent() {
        // Event data will be destroyed by base class
        // We just need to return the memory block
    }
    
    /**
     * @brief Factory method for creating pooled events
     * 
     * @param type Event type string
     * @param data Event payload
     * @param priority Event priority
     * @return Shared pointer to pooled event or nullptr if allocation failed
     */
    static std::shared_ptr<PooledEvent> create(
        const std::string& type,
        const nlohmann::json& data = nlohmann::json{},
        EventBus::Priority priority = EventBus::Priority::NORMAL) {
        
        // Calculate required size
        size_t event_size = sizeof(PooledEvent);
        size_t type_size = type.length() + 1;
        size_t json_size = data.dump().length() + 1;
        size_t total_size = event_size + type_size + json_size;
        
        // Get pool manager
        auto& pool_manager = Memory::get_pool_manager();
        
        // Allocate from pool
        void* block = pool_manager.allocate(total_size);
        if (!block) {
            ESP_LOGE("PooledEvent", "Failed to allocate %zu bytes for event", total_size);
            return nullptr;
        }
        
        // Construct event in-place
        PooledEvent* event = new(block) PooledEvent(&pool_manager, block, total_size);
        
        // Initialize base class
        event->type = type;
        event->data = data;
        event->priority = priority;
        event->timestamp = esp_timer_get_time();
        
        // Custom deleter that returns memory to pool
        auto deleter = [&pool_manager, block, total_size](PooledEvent* evt) {
            evt->~PooledEvent(); // Call destructor
            pool_manager.deallocate(block, total_size);
        };
        
        return std::shared_ptr<PooledEvent>(event, deleter);
    }
    
    /**
     * @brief Create event with pre-serialized JSON
     * 
     * More efficient when JSON is already serialized
     */
    static std::shared_ptr<PooledEvent> create_from_string(
        const std::string& type,
        const std::string& json_data,
        EventBus::Priority priority = EventBus::Priority::NORMAL) {
        
        nlohmann::json data = nlohmann::json::parse(json_data);
        return create(type, data, priority);
    }
};

/**
 * @brief Enhanced EventBus with memory pool support
 */
namespace PooledEventBus {
    
    /**
     * @brief Publish pooled event
     * 
     * Creates event using memory pool and publishes it.
     * Falls back to regular publish if pool allocation fails.
     * 
     * @param type Event type
     * @param data Event data
     * @param priority Event priority
     * @return ESP_OK on success
     */
    inline esp_err_t publish_pooled(
        const std::string& type,
        const nlohmann::json& data = nlohmann::json{},
        EventBus::Priority priority = EventBus::Priority::NORMAL) {
        
        // Try pooled allocation first
        auto pooled_event = PooledEvent::create(type, data, priority);
        
        if (pooled_event) {
            // TODO: Modify EventBus to accept shared_ptr<Event>
            // For now, use regular publish
            return EventBus::publish(type, data, priority);
        }
        
        // Fallback to regular publish if pool exhausted
        ESP_LOGW("PooledEventBus", "Pool exhausted, using heap allocation");
        return EventBus::publish(type, data, priority);
    }
    
    /**
     * @brief Get memory pool statistics for events
     */
    inline Memory::PoolStats get_event_pool_stats() {
        auto& manager = Memory::get_pool_manager();
        
        // Events typically use SMALL (64B) or MEDIUM (128B) pools
        auto small_stats = manager.get_small_pool().get_stats();
        auto medium_stats = manager.get_medium_pool().get_stats();
        
        // Combine stats
        Memory::PoolStats combined;
        combined.allocated_count = small_stats.allocated_count + medium_stats.allocated_count;
        combined.peak_usage = small_stats.peak_usage + medium_stats.peak_usage;
        combined.total_allocations = small_stats.total_allocations + medium_stats.total_allocations;
        combined.allocation_failures = small_stats.allocation_failures + medium_stats.allocation_failures;
        combined.total_bytes_served = small_stats.total_bytes_served + medium_stats.total_bytes_served;
        
        return combined;
    }
    
    /**
     * @brief Monitor event memory pressure
     * 
     * @param callback Function called when memory pressure detected
     */
    inline void set_memory_pressure_callback(
        std::function<void(uint8_t utilization)> callback) {
        
        // Register with memory diagnostics
        // TODO: Implement in MemoryDiagnostics
    }
}

} // namespace ModESP

#endif // POOLED_EVENT_H
