/**
 * @file memory_pool_example.cpp
 * @brief Example usage of Memory Pool system
 * 
 * Demonstrates how to use the memory pool for efficient
 * memory management in ModESP.
 */

#include "memory_pool.h"
#include "memory_diagnostics.h"
#include "pooled_event.h"
#include "esp_log.h"

static const char* TAG = "MemPoolExample";

/**
 * @brief Example data structure using pooled allocation
 */
struct SensorData {
    float temperature;
    float humidity;
    uint32_t timestamp;
    char sensor_id[32];
};

/**
 * @brief Demonstrate basic memory pool usage
 */
void memory_pool_basic_example() {
    ESP_LOGI(TAG, "=== Basic Memory Pool Example ===");
    
    // Get pool manager instance
    auto& pool_manager = ModESP::Memory::get_pool_manager();
    
    // Allocate memory for sensor data
    void* memory = pool_manager.allocate(sizeof(SensorData));
    if (!memory) {
        ESP_LOGE(TAG, "Failed to allocate memory!");
        return;
    }
    
    // Use placement new to construct object
    SensorData* data = new(memory) SensorData{
        .temperature = 25.5f,
        .humidity = 60.0f,
        .timestamp = esp_timer_get_time() / 1000000,
        .sensor_id = "SENSOR_01"
    };
    
    ESP_LOGI(TAG, "Sensor data: T=%.1f°C, H=%.1f%%, ID=%s", 
             data->temperature, data->humidity, data->sensor_id);
    
    // Cleanup - call destructor and deallocate
    data->~SensorData();
    pool_manager.deallocate(memory, sizeof(SensorData));
    
    ESP_LOGI(TAG, "Memory successfully returned to pool");
}

/**
 * @brief Demonstrate PooledPtr RAII usage
 */
void memory_pool_raii_example() {
    ESP_LOGI(TAG, "=== RAII Memory Pool Example ===");
    
    // Create pooled object with automatic cleanup
    auto sensor_data = ModESP::make_pooled<SensorData>();
    
    if (sensor_data) {
        sensor_data->temperature = 22.3f;
        sensor_data->humidity = 55.5f;
        sensor_data->timestamp = esp_timer_get_time() / 1000000;
        strcpy(sensor_data->sensor_id, "SENSOR_02");
        
        ESP_LOGI(TAG, "Using pooled sensor data: T=%.1f°C", 
                 sensor_data->temperature);
        
        // Memory automatically returned when sensor_data goes out of scope
    } else {
        ESP_LOGE(TAG, "Failed to allocate pooled object");
    }
    
    ESP_LOGI(TAG, "Memory automatically returned via RAII");
}

/**
 * @brief Demonstrate pooled event creation
 */
void memory_pool_event_example() {
    ESP_LOGI(TAG, "=== Pooled Event Example ===");
    
    // Create JSON data for event
    nlohmann::json event_data;
    event_data["temperature"] = 24.7;
    event_data["humidity"] = 58.2;
    event_data["sensor_id"] = "TEMP_SENSOR_01";
    
    // Create pooled event
    auto event = ModESP::PooledEvent::create(
        "sensor.temperature.updated",
        event_data,
        EventBus::Priority::NORMAL
    );
    
    if (event) {
        ESP_LOGI(TAG, "Created pooled event: type=%s, size=%zu bytes",
                 event->type.c_str(), event->data.dump().length());
        
        // Event memory automatically returned when event goes out of scope
    } else {
        ESP_LOGE(TAG, "Failed to create pooled event");
    }
    
    // Alternative: Use pooled EventBus publish
    esp_err_t err = ModESP::PooledEventBus::publish_pooled(
        "system.memory.test",
        {{"test_value", 123}, {"status", "ok"}},
        EventBus::Priority::LOW
    );
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Successfully published pooled event");
    }
}

/**
 * @brief Demonstrate memory diagnostics
 */
void memory_pool_diagnostics_example() {
    ESP_LOGI(TAG, "=== Memory Diagnostics Example ===");
    
    auto& pool_manager = ModESP::Memory::get_pool_manager();
    ModESP::Memory::MemoryDiagnostics diagnostics(pool_manager);
    
    // Allocate some memory to show in diagnostics
    std::vector<void*> allocations;
    for (int i = 0; i < 5; i++) {
        void* ptr = pool_manager.allocate(64);
        if (ptr) allocations.push_back(ptr);
    }
    
    // Get and display diagnostics
    auto diag = diagnostics.get_diagnostics();
    
    ESP_LOGI(TAG, "Memory Pool Status:");
    ESP_LOGI(TAG, "  Total Capacity: %zu bytes", diag.total_capacity);
    ESP_LOGI(TAG, "  Currently Allocated: %zu bytes", diag.total_allocated);
    ESP_LOGI(TAG, "  Overall Utilization: %d%%", diag.overall_utilization);
    ESP_LOGI(TAG, "  Fragmentation Index: %d%%", diag.fragmentation_index);
    
    // Display per-pool statistics
    ESP_LOGI(TAG, "Pool Statistics:");
    for (const auto& pool : diag.pools) {
        ESP_LOGI(TAG, "  %s Pool (%zu bytes): %d/%d blocks used (%d%%)",
                 pool.name, pool.block_size,
                 pool.used_blocks, pool.total_blocks,
                 pool.utilization_percent);
    }
    
    // Check for alerts
    if (diag.critical_memory_alert) {
        ESP_LOGE(TAG, "CRITICAL MEMORY ALERT!");
    } else if (diag.low_memory_warning) {
        ESP_LOGW(TAG, "Low memory warning");
    }
    
    // Generate full report
    ESP_LOGI(TAG, "%s", diagnostics.generate_report().c_str());
    
    // Cleanup
    for (void* ptr : allocations) {
        pool_manager.deallocate(ptr, 64);
    }
}

/**
 * @brief Demonstrate memory usage tracking
 */
void memory_pool_tracking_example() {
    ESP_LOGI(TAG, "=== Memory Usage Tracking Example ===");
    
    {
        // Track memory usage for this operation
        TRACK_MEMORY_USAGE("Complex Operation");
        
        auto& pool_manager = ModESP::Memory::get_pool_manager();
        
        // Simulate complex operation with multiple allocations
        std::vector<void*> temp_allocations;
        
        for (int i = 0; i < 10; i++) {
            void* ptr = pool_manager.allocate(128);
            if (ptr) {
                temp_allocations.push_back(ptr);
                vTaskDelay(pdMS_TO_TICKS(10)); // Simulate work
            }
        }
        
        ESP_LOGI(TAG, "Allocated %zu temporary blocks", temp_allocations.size());
        
        // Cleanup
        for (void* ptr : temp_allocations) {
            pool_manager.deallocate(ptr, 128);
        }
        
        // MemoryUsageTracker destructor will log usage statistics
    }
}

/**
 * @brief Run memory pool benchmarks
 */
void memory_pool_benchmark_example() {
    ESP_LOGI(TAG, "=== Memory Pool Benchmarks ===");
    
    // Run allocation benchmark
    auto result = ModESP::Memory::MemoryPoolBenchmark::benchmark_allocation(64, 1000);
    
    ESP_LOGI(TAG, "Allocation Benchmark Results:");
    ESP_LOGI(TAG, "  Test: %s", result.test_name);
    ESP_LOGI(TAG, "  Iterations: %u", result.iterations);
    ESP_LOGI(TAG, "  Average time: %llu ns", result.avg_time_ns);
    ESP_LOGI(TAG, "  Min time: %llu ns", result.min_time_ns);
    ESP_LOGI(TAG, "  Max time: %llu ns", result.max_time_ns);
    ESP_LOGI(TAG, "  Failures: %u", result.failures);
    
    // Run all benchmarks
    ESP_LOGI(TAG, "Running complete benchmark suite...");
    auto all_results = ModESP::Memory::MemoryPoolBenchmark::run_all_benchmarks();
    
    for (const auto& res : all_results) {
        ESP_LOGI(TAG, "%s: %u iterations, avg %llu ns, %u failures",
                 res.test_name, res.iterations, res.avg_time_ns, res.failures);
    }
}

/**
 * @brief Main example function
 */
void run_memory_pool_examples() {
    ESP_LOGI(TAG, "Starting Memory Pool Examples");
    
    // Run all examples
    memory_pool_basic_example();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    memory_pool_raii_example();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    memory_pool_event_example();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    memory_pool_diagnostics_example();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    memory_pool_tracking_example();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    memory_pool_benchmark_example();
    
    ESP_LOGI(TAG, "Memory Pool Examples Complete!");
}
