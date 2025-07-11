/**
 * @file test_memory_pool.cpp
 * @brief Hardware tests for Memory Pool on ESP32
 */

#include "memory/memory_pool.h"
#include "pooled_event.h"
#include "unity.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <vector>
#include <random>

static const char* TAG = "TestMemoryPool";

// Test basic allocation and deallocation
TEST_CASE("Memory Pool Basic Allocation", "[memory_pool]") {
    // Initialize pool
    TEST_ASSERT_EQUAL(ESP_OK, MemoryPool::init());
    
    // Test tiny allocation
    void* ptr1 = MemoryPool::allocate(16);
    TEST_ASSERT_NOT_NULL(ptr1);
    
    // Test small allocation
    void* ptr2 = MemoryPool::allocate(48);
    TEST_ASSERT_NOT_NULL(ptr2);
    
    // Test deallocation
    TEST_ASSERT_EQUAL(ESP_OK, MemoryPool::deallocate(ptr1, 16));
    TEST_ASSERT_EQUAL(ESP_OK, MemoryPool::deallocate(ptr2, 48));
    
    // Verify pool is restored
    auto diag = MemoryPool::get_diagnostics();
    TEST_ASSERT_EQUAL(0, diag.current_usage);
}

// Test pool exhaustion
TEST_CASE("Memory Pool Exhaustion", "[memory_pool]") {
    std::vector<void*> allocations;
    
    // Allocate all tiny blocks
    for (int i = 0; i < CONFIG_MEMORY_POOL_TINY_COUNT; i++) {
        void* ptr = MemoryPool::allocate(16);
        if (ptr) {
            allocations.push_back(ptr);
        }
    }
    
    // Next allocation should fail
    void* ptr = MemoryPool::allocate(16);
    TEST_ASSERT_NULL(ptr);
    
    // Check diagnostics
    auto diag = MemoryPool::get_diagnostics();
    TEST_ASSERT_TRUE(diag.allocation_failures > 0);
    
    // Deallocate all
    for (auto p : allocations) {
        MemoryPool::deallocate(p, 16);
    }
}

// Test different block sizes
TEST_CASE("Memory Pool Block Size Selection", "[memory_pool]") {
    struct TestCase {
        size_t request_size;
        MemoryPool::BlockSize expected_pool;
    };
    
    TestCase cases[] = {
        {8, MemoryPool::BlockSize::TINY},
        {32, MemoryPool::BlockSize::TINY},
        {33, MemoryPool::BlockSize::SMALL},
        {64, MemoryPool::BlockSize::SMALL},
        {65, MemoryPool::BlockSize::MEDIUM},
        {128, MemoryPool::BlockSize::MEDIUM},
        {129, MemoryPool::BlockSize::LARGE},
        {256, MemoryPool::BlockSize::LARGE},
        {257, MemoryPool::BlockSize::XLARGE},
        {512, MemoryPool::BlockSize::XLARGE}
    };
    
    for (auto& tc : cases) {
        void* ptr = MemoryPool::allocate(tc.request_size);
        TEST_ASSERT_NOT_NULL(ptr);
        
        // Write pattern to verify memory
        memset(ptr, 0xAA, tc.request_size);
        
        TEST_ASSERT_EQUAL(ESP_OK, MemoryPool::deallocate(ptr, tc.request_size));
    }
}

// Stress test with multiple threads
static void allocation_task(void* param) {
    const int iterations = 1000;
    std::mt19937 gen(xTaskGetTickCount());
    std::uniform_int_distribution<> size_dist(8, 256);
    
    for (int i = 0; i < iterations; i++) {
        size_t size = size_dist(gen);
        void* ptr = MemoryPool::allocate(size);
        
        if (ptr) {
            // Simulate work
            memset(ptr, i & 0xFF, size);
            vTaskDelay(pdMS_TO_TICKS(1));
            
            MemoryPool::deallocate(ptr, size);
        }
        
        // Yield to other tasks
        taskYIELD();
    }
    
    vTaskDelete(NULL);
}

TEST_CASE("Memory Pool Thread Safety", "[memory_pool]") {
    const int num_tasks = 4;
    TaskHandle_t tasks[num_tasks];
    
    // Create allocation tasks
    for (int i = 0; i < num_tasks; i++) {
        xTaskCreate(allocation_task, "alloc_task", 4096, NULL, 5, &tasks[i]);
    }
    
    // Let tasks run
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Check pool integrity
    auto diag = MemoryPool::get_diagnostics();
    ESP_LOGI(TAG, "After stress test:");
    ESP_LOGI(TAG, "  Current usage: %lu", diag.current_usage);
    ESP_LOGI(TAG, "  Peak usage: %lu", diag.peak_usage);
    ESP_LOGI(TAG, "  Failures: %lu", diag.allocation_failures);
    
    // Should return to zero usage
    TEST_ASSERT_EQUAL(0, diag.current_usage);
}

// Performance benchmark
TEST_CASE("Memory Pool Performance", "[memory_pool][performance]") {
    const int iterations = 10000;
    int64_t pool_time = 0;
    int64_t heap_time = 0;
    
    // Benchmark pool allocation
    int64_t start = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        void* ptr = MemoryPool::allocate(64);
        TEST_ASSERT_NOT_NULL(ptr);
        MemoryPool::deallocate(ptr, 64);
    }
    pool_time = esp_timer_get_time() - start;
    
    // Benchmark heap allocation
    start = esp_timer_get_time();
    for (int i = 0; i < iterations; i++) {
        void* ptr = malloc(64);
        TEST_ASSERT_NOT_NULL(ptr);
        free(ptr);
    }
    heap_time = esp_timer_get_time() - start;
    
    ESP_LOGI(TAG, "Performance comparison (%d iterations):", iterations);
    ESP_LOGI(TAG, "  Pool: %lld us (%lld ns/op)", pool_time, (pool_time * 1000) / iterations);
    ESP_LOGI(TAG, "  Heap: %lld us (%lld ns/op)", heap_time, (heap_time * 1000) / iterations);
    ESP_LOGI(TAG, "  Pool is %.1fx faster", (float)heap_time / pool_time);
    
    // Pool should be faster
    TEST_ASSERT_LESS_THAN(heap_time, pool_time);
}

// Test pooled events integration
TEST_CASE("Pooled Events", "[memory_pool][event_bus]") {
    // Create pooled event
    auto event = EventBus::PooledEvent::create(
        "test.event",
        {{"value", 42}, {"message", "Hello from pool"}},
        EventBus::Priority::HIGH
    );
    
    TEST_ASSERT_NOT_NULL(event);
    TEST_ASSERT_TRUE(event->is_pooled());
    TEST_ASSERT_EQUAL_STRING("test.event", event->type.c_str());
    TEST_ASSERT_EQUAL(42, event->data["value"]);
    
    // Event should be automatically deallocated when out of scope
    auto diag_before = MemoryPool::get_diagnostics();
    event.reset(); // Force deallocation
    auto diag_after = MemoryPool::get_diagnostics();
    
    TEST_ASSERT_LESS_THAN(diag_before.current_usage, diag_after.current_usage + 1);
}

// Long-term stability test
TEST_CASE("Memory Pool 24h Simulation", "[memory_pool][long]") {
    const int hours_to_simulate = 24;
    const int allocations_per_hour = 3600; // One per second
    
    ESP_LOGI(TAG, "Starting %d hour simulation...", hours_to_simulate);
    
    for (int hour = 0; hour < hours_to_simulate; hour++) {
        for (int i = 0; i < allocations_per_hour; i++) {
            // Random size allocation
            size_t size = 16 + (rand() % 240);
            void* ptr = MemoryPool::allocate(size);
            
            if (ptr) {
                // Simulate data processing
                memset(ptr, hour & 0xFF, size);
                
                // Quick deallocation to simulate real usage
                MemoryPool::deallocate(ptr, size);
            }
            
            // Check for memory leaks every 1000 allocations
            if (i % 1000 == 0) {
                auto diag = MemoryPool::get_diagnostics();
                TEST_ASSERT_LESS_OR_EQUAL(10, diag.current_usage);
            }
        }
        
        ESP_LOGI(TAG, "Hour %d complete", hour + 1);
    }
    
    // Final check - should have zero leaks
    auto final_diag = MemoryPool::get_diagnostics();
    TEST_ASSERT_EQUAL(0, final_diag.current_usage);
    ESP_LOGI(TAG, "24h simulation complete. Peak usage: %lu", final_diag.peak_usage);
}

// Diagnostics JSON export test
TEST_CASE("Memory Pool Diagnostics JSON", "[memory_pool]") {
    // Make some allocations
    void* ptrs[5];
    for (int i = 0; i < 5; i++) {
        ptrs[i] = MemoryPool::allocate(32 * (i + 1));
    }
    
    // Get diagnostics
    auto diag = MemoryPool::get_diagnostics();
    auto json = diag.to_json();
    
    // Verify JSON structure
    TEST_ASSERT_TRUE(json.contains("current_usage"));
    TEST_ASSERT_TRUE(json.contains("pools"));
    TEST_ASSERT_TRUE(json["pools"].is_array());
    TEST_ASSERT_EQUAL(5, json["pools"].size());
    
    ESP_LOGI(TAG, "Diagnostics JSON: %s", json.dump(2).c_str());
    
    // Cleanup
    for (int i = 0; i < 5; i++) {
        if (ptrs[i]) {
            MemoryPool::deallocate(ptrs[i], 32 * (i + 1));
        }
    }
}