/**
 * @file test_memory_pool.cpp
 * @brief Unit tests for Memory Pool system
 */

#include "unity.h"
#include "memory_pool.h"
#include "memory_diagnostics.h"
#include <vector>
#include <thread>

using namespace ModESP::Memory;

// Test basic allocation and deallocation
TEST_CASE("Memory Pool Basic Allocation", "[memory_pool]") {
    auto& pool_manager = get_pool_manager();
    
    // Test tiny allocation
    void* ptr1 = pool_manager.allocate(16);
    TEST_ASSERT_NOT_NULL(ptr1);
    
    // Test small allocation
    void* ptr2 = pool_manager.allocate(48);
    TEST_ASSERT_NOT_NULL(ptr2);
    
    // Test medium allocation
    void* ptr3 = pool_manager.allocate(100);
    TEST_ASSERT_NOT_NULL(ptr3);
    
    // Deallocate
    TEST_ASSERT_TRUE(pool_manager.deallocate(ptr1, 16));
    TEST_ASSERT_TRUE(pool_manager.deallocate(ptr2, 48));
    TEST_ASSERT_TRUE(pool_manager.deallocate(ptr3, 100));
}

// Test pool exhaustion
TEST_CASE("Memory Pool Exhaustion", "[memory_pool]") {
    auto& pool_manager = get_pool_manager();
    std::vector<void*> allocations;
    
    // Allocate until exhausted (tiny pool has CONFIG_MEMORY_POOL_TINY_COUNT blocks)
    for (int i = 0; i < CONFIG_MEMORY_POOL_TINY_COUNT + 10; i++) {
        void* ptr = pool_manager.allocate(16);
        if (ptr) {
            allocations.push_back(ptr);
        }
    }
    
    // Should have exactly CONFIG_MEMORY_POOL_TINY_COUNT successful allocations
    TEST_ASSERT_EQUAL(CONFIG_MEMORY_POOL_TINY_COUNT, allocations.size());
    
    // Next allocation should fail
    void* ptr = pool_manager.allocate(16);
    TEST_ASSERT_NULL(ptr);
    
    // Deallocate all
    for (void* p : allocations) {
        TEST_ASSERT_TRUE(pool_manager.deallocate(p, 16));
    }
}

// Test different size allocations
TEST_CASE("Memory Pool Size Selection", "[memory_pool]") {
    auto& pool_manager = get_pool_manager();
    
    // Each size should go to appropriate pool
    void* tiny = pool_manager.allocate(32);
    void* small = pool_manager.allocate(64);
    void* medium = pool_manager.allocate(128);
    void* large = pool_manager.allocate(256);
    void* xlarge = pool_manager.allocate(512);
    
    TEST_ASSERT_NOT_NULL(tiny);
    TEST_ASSERT_NOT_NULL(small);
    TEST_ASSERT_NOT_NULL(medium);
    TEST_ASSERT_NOT_NULL(large);
    TEST_ASSERT_NOT_NULL(xlarge);
    
    // Too large should fail
    void* too_large = pool_manager.allocate(1024);
    TEST_ASSERT_NULL(too_large);
    
    // Cleanup
    pool_manager.deallocate(tiny, 32);
    pool_manager.deallocate(small, 64);
    pool_manager.deallocate(medium, 128);
    pool_manager.deallocate(large, 256);
    pool_manager.deallocate(xlarge, 512);
}

// Test PooledPtr RAII wrapper
TEST_CASE("PooledPtr RAII", "[memory_pool]") {
    auto& pool_manager = get_pool_manager();
    
    {
        // Create pooled object
        auto ptr = pool_manager.allocate_object<int>(42);
        TEST_ASSERT_TRUE(ptr);
        TEST_ASSERT_EQUAL(42, *ptr);
        
        // Modify value
        *ptr = 100;
        TEST_ASSERT_EQUAL(100, *ptr);
        
        // Should auto-deallocate on scope exit
    }
    
    // Verify memory was returned (by allocating again)
    void* verify = pool_manager.allocate(sizeof(int));
    TEST_ASSERT_NOT_NULL(verify);
    pool_manager.deallocate(verify, sizeof(int));
}

// Test statistics tracking
TEST_CASE("Memory Pool Statistics", "[memory_pool]") {
    auto& pool_manager = get_pool_manager();
    pool_manager.tiny_pool_.reset_stats();
    
    auto initial_stats = pool_manager.tiny_pool_.get_stats();
    TEST_ASSERT_EQUAL(0, initial_stats.total_allocations);
    
    // Perform allocations
    void* ptr1 = pool_manager.allocate(16);
    void* ptr2 = pool_manager.allocate(16);
    
    auto stats = pool_manager.tiny_pool_.get_stats();
    TEST_ASSERT_EQUAL(2, stats.allocated_count);
    TEST_ASSERT_EQUAL(2, stats.total_allocations);
    TEST_ASSERT_EQUAL(2, stats.peak_usage);
    
    // Deallocate one
    pool_manager.deallocate(ptr1, 16);
    
    stats = pool_manager.tiny_pool_.get_stats();
    TEST_ASSERT_EQUAL(1, stats.allocated_count);
    TEST_ASSERT_EQUAL(2, stats.peak_usage); // Peak should remain
    
    pool_manager.deallocate(ptr2, 16);
}

// Test memory diagnostics
TEST_CASE("Memory Diagnostics", "[memory_pool]") {
    auto& pool_manager = get_pool_manager();
    MemoryDiagnostics diagnostics(pool_manager);
    
    // Get initial diagnostics
    auto diag = diagnostics.get_diagnostics();
    
    TEST_ASSERT_TRUE(diag.total_capacity > 0);
    TEST_ASSERT_TRUE(diag.overall_utilization >= 0);
    TEST_ASSERT_TRUE(diag.overall_utilization <= 100);
    
    // Allocate some memory
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; i++) {
        ptrs.push_back(pool_manager.allocate(64));
    }
    
    // Check utilization increased
    auto diag2 = diagnostics.get_diagnostics();
    TEST_ASSERT_TRUE(diag2.overall_utilization > diag.overall_utilization);
    
    // Cleanup
    for (void* p : ptrs) {
        pool_manager.deallocate(p, 64);
    }
}

// Test thread safety (simplified for embedded)
TEST_CASE("Memory Pool Thread Safety", "[memory_pool]") {
    auto& pool_manager = get_pool_manager();
    
    const int NUM_ITERATIONS = 100;
    std::atomic<int> success_count(0);
    
    auto thread_func = [&]() {
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            void* ptr = pool_manager.allocate(32);
            if (ptr) {
                success_count++;
                vTaskDelay(1); // Yield
                pool_manager.deallocate(ptr, 32);
            }
        }
    };
    
    // Create tasks
    TaskHandle_t task1, task2;
    xTaskCreate([](void* param) {
        auto* func = static_cast<std::function<void()>*>(param);
        (*func)();
        vTaskDelete(NULL);
    }, "test1", 4096, &thread_func, tskIDLE_PRIORITY + 1, &task1);
    
    xTaskCreate([](void* param) {
        auto* func = static_cast<std::function<void()>*>(param);
        (*func)();
        vTaskDelete(NULL);
    }, "test2", 4096, &thread_func, tskIDLE_PRIORITY + 1, &task2);
    
    // Wait for completion
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Both threads should have made progress
    TEST_ASSERT_TRUE(success_count > NUM_ITERATIONS);
}

// Test benchmark functionality
TEST_CASE("Memory Pool Benchmark", "[memory_pool]") {
    auto result = MemoryPoolBenchmark::benchmark_allocation(32, 1000);
    
    TEST_ASSERT_EQUAL_STRING("Allocation/Deallocation", result.test_name);
    TEST_ASSERT_EQUAL(1000, result.iterations);
    TEST_ASSERT_TRUE(result.avg_time_ns > 0);
    TEST_ASSERT_TRUE(result.min_time_ns <= result.avg_time_ns);
    TEST_ASSERT_TRUE(result.max_time_ns >= result.avg_time_ns);
}
