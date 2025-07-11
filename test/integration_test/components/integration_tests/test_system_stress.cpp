/**
 * @file test_system_stress.cpp
 * @brief Stress tests for ModESP system under heavy load
 * 
 * Tests system behavior under various stress conditions:
 * - High event throughput
 * - Memory pressure
 * - CPU intensive operations
 * - Concurrent module activity
 */

#include "integration_test_common.h"
#include "application.h"
#include "event_bus.h"
#include "shared_state.h"
#include "config_manager.h"
#include "module_manager.h"

static const char* TAG = "StressTest";

/**
 * @brief Test system under high event load
 * 
 * Generates massive number of events and verifies:
 * - System remains responsive
 * - No memory leaks
 * - Event processing doesn't crash
 * - Performance remains acceptable
 */
void test_high_event_throughput() {
    ESP_LOGI(TAG, "=== Testing High Event Throughput ===");
    
    TEST_ASSERT_TRUE(Application::is_running());
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    const uint32_t total_events = 10000;
    const uint32_t batch_size = 100;
    const uint32_t batches = total_events / batch_size;
    
    size_t initial_heap = Application::get_free_heap();
    uint32_t events_published = 0;
    
    ESP_LOGI(TAG, "Publishing %lu events in %lu batches...", total_events, batches);
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    
    for (uint32_t batch = 0; batch < batches; batch++) {
        // Publish batch of events
        for (uint32_t i = 0; i < batch_size; i++) {
            nlohmann::json event_data = {
                {"batch", batch},
                {"event", i},
                {"timestamp", esp_timer_get_time()},
                {"test_data", "stress_test_payload"}
            };
            
            EventBus::publish("stress.test.event", event_data);
            events_published++;
        }
        
        // Give system time to process events
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Check system health every 10 batches
        if (batch % 10 == 0) {
            INTEGRATION_TEST_ASSERT_SYSTEM_HEALTHY();
            
            size_t current_heap = Application::get_free_heap();
            ESP_LOGI(TAG, "Batch %lu/%lu: %lu events, heap: %zu bytes", 
                     batch + 1, batches, events_published, current_heap);
            
            // Verify no major memory leak during event processing
            TEST_ASSERT_GREATER_THAN(initial_heap - 20000, current_heap);
        }
    }
    
    uint32_t end_time = esp_timer_get_time() / 1000;
    uint32_t duration_ms = end_time - start_time;
    
    // Allow time for all events to be processed
    ESP_LOGI(TAG, "Waiting for event processing to complete...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    integration_test_stop_metrics(&metrics);
    
    // Verify final system state
    INTEGRATION_TEST_ASSERT_SYSTEM_HEALTHY();
    
    size_t final_heap = Application::get_free_heap();
    int32_t heap_delta = (int32_t)final_heap - (int32_t)initial_heap;
    
    float events_per_second = (float)events_published / (duration_ms / 1000.0f);
    
    ESP_LOGI(TAG, "Event throughput: %.1f events/sec", events_per_second);
    ESP_LOGI(TAG, "Memory delta: %ld bytes", heap_delta);
    
    // Verify reasonable performance and no major memory leaks
    TEST_ASSERT_GREATER_THAN(100.0f, events_per_second); // At least 100 events/sec
    TEST_ASSERT_GREATER_THAN(-15000, heap_delta); // No more than 15KB leak
    
    integration_test_print_metrics("High Event Throughput", &metrics);
    
    ESP_LOGI(TAG, "✅ High event throughput test PASSED");
    ESP_LOGI(TAG, "   Published %lu events at %.1f events/sec", 
             events_published, events_per_second);
}

/**
 * @brief Test system under memory pressure
 * 
 * Allocates large amounts of memory to stress the heap:
 * - Tests memory allocation/deallocation
 * - Verifies system stability under low memory
 * - Checks memory fragmentation handling
 */
void test_memory_pressure() {
    ESP_LOGI(TAG, "=== Testing Memory Pressure ===");
    
    TEST_ASSERT_TRUE(Application::is_running());
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    size_t initial_heap = Application::get_free_heap();
    const size_t allocation_size = 1024; // 1KB chunks
    const uint32_t max_allocations = 100; // Up to 100KB
    
    std::vector<void*> allocations;
    allocations.reserve(max_allocations);
    
    ESP_LOGI(TAG, "Initial free heap: %zu bytes", initial_heap);
    ESP_LOGI(TAG, "Allocating memory in %zu byte chunks...", allocation_size);
    
    // Allocate memory until we reach pressure point
    for (uint32_t i = 0; i < max_allocations; i++) {
        void* ptr = malloc(allocation_size);
        if (ptr != nullptr) {
            allocations.push_back(ptr);
            
            // Fill with test pattern to ensure real allocation
            memset(ptr, 0xAA + (i % 16), allocation_size);
        } else {
            ESP_LOGW(TAG, "Failed to allocate chunk %lu", i);
            break;
        }
        
        // Check system health every 10 allocations
        if (i % 10 == 0) {
            size_t current_heap = Application::get_free_heap();
            ESP_LOGI(TAG, "Allocated %lu chunks, free heap: %zu bytes", 
                     i + 1, current_heap);
            
            // Ensure system remains responsive
            INTEGRATION_TEST_ASSERT_SYSTEM_HEALTHY();
            
            // Stop if we're getting too low on memory
            if (current_heap < CRITICAL_FREE_HEAP_BYTES * 2) {
                ESP_LOGI(TAG, "Stopping allocation to maintain system stability");
                break;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    size_t allocated_chunks = allocations.size();
    size_t heap_under_pressure = Application::get_free_heap();
    
    ESP_LOGI(TAG, "Allocated %zu chunks (%zu bytes total)", 
             allocated_chunks, allocated_chunks * allocation_size);
    ESP_LOGI(TAG, "Free heap under pressure: %zu bytes", heap_under_pressure);
    
    // Verify system still works under memory pressure
    INTEGRATION_TEST_ASSERT_SYSTEM_HEALTHY();
    
    // Test that we can still publish events under memory pressure
    for (int i = 0; i < 10; i++) {
        EventBus::publish("stress.memory.test", {{"iteration", i}});
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Free all allocated memory
    ESP_LOGI(TAG, "Freeing allocated memory...");
    for (void* ptr : allocations) {
        free(ptr);
    }
    allocations.clear();
    
    // Give system time to consolidate memory
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    size_t final_heap = Application::get_free_heap();
    int32_t recovery_delta = (int32_t)final_heap - (int32_t)initial_heap;
    
    ESP_LOGI(TAG, "Memory recovery: initial=%zu, final=%zu, delta=%ld", 
             initial_heap, final_heap, recovery_delta);
    
    integration_test_stop_metrics(&metrics);
    
    // Verify memory was properly recovered
    TEST_ASSERT_GREATER_THAN(initial_heap - 5000, final_heap); // Allow 5KB variance
    INTEGRATION_TEST_ASSERT_SYSTEM_HEALTHY();
    
    integration_test_print_metrics("Memory Pressure", &metrics);
    
    ESP_LOGI(TAG, "✅ Memory pressure test PASSED");
    ESP_LOGI(TAG, "   Allocated %zu chunks, recovered %ld bytes", 
             allocated_chunks, recovery_delta);
}

/**
 * @brief Test system under CPU intensive load
 * 
 * Creates CPU-intensive tasks to stress the scheduler:
 * - Multiple high-priority tasks
 * - CPU-bound calculations
 * - Verifies main loop timing maintained
 */
void test_cpu_intensive_load() {
    ESP_LOGI(TAG, "=== Testing CPU Intensive Load ===");
    
    TEST_ASSERT_TRUE(Application::is_running());
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    const uint32_t test_duration_ms = 10000; // 10 seconds
    const uint32_t num_cpu_tasks = 2;
    
    volatile bool cpu_tasks_running = true;
    TaskHandle_t cpu_task_handles[num_cpu_tasks];
    
    // CPU intensive task function
    auto cpu_intensive_task = [](void* param) {
        volatile bool* running = (volatile bool*)param;
        uint32_t iterations = 0;
        
        ESP_LOGI(TAG, "CPU intensive task started on core %d", xPortGetCoreID());
        
        while (*running) {
            // Perform CPU-intensive calculations
            volatile float result = 0;
            for (int i = 0; i < 10000; i++) {
                result += sqrt(i * 3.14159f) * sin(i);
            }
            iterations++;
            
            // Yield occasionally to prevent watchdog
            if (iterations % 100 == 0) {
                vTaskDelay(1);
            }
        }
        
        ESP_LOGI(TAG, "CPU intensive task completed %lu iterations", iterations);
        vTaskDelete(nullptr);
    };
    
    ESP_LOGI(TAG, "Starting %lu CPU intensive tasks...", num_cpu_tasks);
    
    // Create CPU intensive tasks
    for (uint32_t i = 0; i < num_cpu_tasks; i++) {
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "cpu_stress_%lu", i);
        
        xTaskCreate(
            (TaskFunction_t)cpu_intensive_task,
            task_name,
            4096,
            (void*)&cpu_tasks_running,
            3, // High priority
            &cpu_task_handles[i]
        );
    }
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    uint32_t last_check = start_time;
    uint32_t health_checks = 0;
    uint32_t health_failures = 0;
    
    // Monitor system under CPU load
    while ((esp_timer_get_time() / 1000) - start_time < test_duration_ms) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        
        if (current_time - last_check >= 1000) { // Check every second
            health_checks++;
            
            if (!Application::check_health()) {
                health_failures++;
                ESP_LOGW(TAG, "Health check failed under CPU load");
            }
            
            uint8_t cpu_usage = Application::get_cpu_usage();
            size_t free_heap = Application::get_free_heap();
            
            ESP_LOGI(TAG, "Under CPU load: cpu=%u%%, heap=%zu, health=%lu/%lu", 
                     cpu_usage, free_heap, health_checks - health_failures, health_checks);
            
            // System should remain responsive
            TEST_ASSERT_TRUE(Application::is_running());
            TEST_ASSERT_GREATER_THAN(CRITICAL_FREE_HEAP_BYTES, free_heap);
            
            last_check = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Stop CPU intensive tasks
    ESP_LOGI(TAG, "Stopping CPU intensive tasks...");
    cpu_tasks_running = false;
    
    // Wait for tasks to complete
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    integration_test_stop_metrics(&metrics);
    
    // Verify system recovered from CPU stress
    INTEGRATION_TEST_ASSERT_SYSTEM_HEALTHY();
    
    float health_success_rate = (float)(health_checks - health_failures) / health_checks;
    
    ESP_LOGI(TAG, "CPU stress results: %lu/%lu health checks passed (%.1f%%)", 
             health_checks - health_failures, health_checks, health_success_rate * 100);
    
    // Allow some health check failures under extreme CPU load
    TEST_ASSERT_GREATER_THAN(0.8f, health_success_rate); // 80% success rate
    
    integration_test_print_metrics("CPU Intensive Load", &metrics);
    
    ESP_LOGI(TAG, "✅ CPU intensive load test PASSED");
}

// Test runner for system stress tests
extern "C" void run_system_stress_tests(void) {
    ESP_LOGI(TAG, "🔥 Starting System Stress Tests");
    
    RUN_TEST(test_high_event_throughput);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    RUN_TEST(test_memory_pressure);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    RUN_TEST(test_cpu_intensive_load);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "✅ System Stress Tests Complete");
} 