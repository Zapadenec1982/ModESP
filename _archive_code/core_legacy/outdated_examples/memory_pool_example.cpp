/**
 * @file memory_pool_example.cpp
 * @brief Example usage of Memory Pool in ModESP
 */

#include "memory/memory_pool.h"
#include "pooled_event.h"
#include "esp_log.h"

static const char* TAG = "MemPoolExample";

void memory_pool_example() {
    // Initialize memory pool system
    if (MemoryPool::init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize memory pool");
        return;
    }
    
    ESP_LOGI(TAG, "Memory Pool initialized successfully");
    
    // Example 1: Simple allocation
    void* buffer = MemoryPool::allocate(100);
    if (buffer) {
        ESP_LOGI(TAG, "Allocated 100 bytes from pool");
        
        // Use the buffer
        memset(buffer, 0x42, 100);
        
        // Deallocate when done
        MemoryPool::deallocate(buffer, 100);
        ESP_LOGI(TAG, "Deallocated buffer");
    }
    
    // Example 2: Using pooled events
    auto event = EventBus::PooledEvent::create(
        "sensor.temperature.updated",
        {
            {"sensor_id", "temp_01"},
            {"value", 23.5},
            {"unit", "celsius"}
        },
        EventBus::Priority::NORMAL
    );
    
    if (event) {
        ESP_LOGI(TAG, "Created pooled event: %s", event->type.c_str());
        ESP_LOGI(TAG, "Event data: %s", event->data.dump().c_str());
        
        // Event will be automatically deallocated when out of scope
    }
    
    // Example 3: Check pool diagnostics
    auto diag = MemoryPool::get_diagnostics();
    
    ESP_LOGI(TAG, "Memory Pool Diagnostics:");
    ESP_LOGI(TAG, "  Current usage: %lu blocks", diag.current_usage);
    ESP_LOGI(TAG, "  Peak usage: %lu blocks", diag.peak_usage);
    ESP_LOGI(TAG, "  Allocation failures: %lu", diag.allocation_failures);
    ESP_LOGI(TAG, "  Free percentage: %d%%", 
             MemoryPool::MemoryPoolManager::get_instance().get_free_percentage());
    
    // Example 4: Monitor individual pools
    for (int i = 0; i < 5; i++) {
        auto& pool = diag.pools[i];
        ESP_LOGI(TAG, "  %s pool: %u/%u blocks used (peak: %u)",
                 pool.name,
                 pool.used_blocks,
                 pool.total_blocks,
                 pool.peak_used_blocks);
    }
    
    // Example 5: Export diagnostics as JSON
    auto json_diag = diag.to_json();
    ESP_LOGI(TAG, "Diagnostics JSON: %s", json_diag.dump(2).c_str());
    
    // Example 6: Handle low memory conditions
    if (diag.low_memory_warning) {
        ESP_LOGW(TAG, "Low memory warning triggered!");
    }
    
    if (diag.critical_memory_alert) {
        ESP_LOGE(TAG, "CRITICAL: Memory pool critically low!");
    }
}