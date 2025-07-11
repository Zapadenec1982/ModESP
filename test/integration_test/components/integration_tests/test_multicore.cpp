/**
 * @file test_multicore.cpp
 * @brief Multicore integration tests for ESP32-S3 dual-core operation
 * 
 * Tests system behavior across both CPU cores:
 * - Core 0: Main application loop, module management
 * - Core 1: Sensor task, high-frequency operations
 * - Inter-core communication and synchronization
 * - Load balancing and performance
 */

#include "integration_test_common.h"
#include "application.h"
#include "event_bus.h"
#include "shared_state.h"
#include "module_manager.h"

static const char* TAG = "MulticoreTest";

// Shared data structures for multicore testing
static volatile uint32_t core0_counter = 0;
static volatile uint32_t core1_counter = 0;
static volatile bool multicore_test_running = false;
static SemaphoreHandle_t sync_semaphore = nullptr;
static QueueHandle_t inter_core_queue = nullptr;

/**
 * @brief Test basic multicore operation
 * 
 * Verifies that both cores are operational and can run tasks:
 * - Core identification
 * - Task affinity
 * - Basic inter-core communication
 */
void test_basic_multicore_operation() {
    ESP_LOGI(TAG, "=== Testing Basic Multicore Operation ===");
    
    TEST_ASSERT_TRUE(Application::is_running());
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Create synchronization primitives
    sync_semaphore = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(sync_semaphore);
    
    inter_core_queue = xQueueCreate(10, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(inter_core_queue);
    
    multicore_test_running = true;
    core0_counter = 0;
    core1_counter = 0;
    
    // Task for Core 0
    auto core0_task = [](void* param) {
        ESP_LOGI(TAG, "Core 0 task started on core %d", xPortGetCoreID());
        TEST_ASSERT_EQUAL(0, xPortGetCoreID());
        
        while (multicore_test_running) {
            core0_counter = core0_counter + 1;
            
            // Send message to Core 1
            uint32_t message = core0_counter;
            xQueueSend(inter_core_queue, &message, pdMS_TO_TICKS(10));
            
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        
        ESP_LOGI(TAG, "Core 0 task completed %lu iterations", core0_counter);
        xSemaphoreGive(sync_semaphore);
        vTaskDelete(nullptr);
    };
    
    // Task for Core 1
    auto core1_task = [](void* param) {
        ESP_LOGI(TAG, "Core 1 task started on core %d", xPortGetCoreID());
        TEST_ASSERT_EQUAL(1, xPortGetCoreID());
        
        while (multicore_test_running) {
            core1_counter = core1_counter + 1;
            
            // Receive message from Core 0
            uint32_t received_message;
            if (xQueueReceive(inter_core_queue, &received_message, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Verify message integrity
                TEST_ASSERT_GREATER_THAN(0, received_message);
            }
            
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        
        ESP_LOGI(TAG, "Core 1 task completed %lu iterations", core1_counter);
        xSemaphoreGive(sync_semaphore);
        vTaskDelete(nullptr);
    };
    
    // Create tasks pinned to specific cores
    TaskHandle_t core0_task_handle, core1_task_handle;
    
    xTaskCreatePinnedToCore(
        (TaskFunction_t)core0_task, "test_core0", 4096, nullptr, 3, &core0_task_handle, 0
    );
    
    xTaskCreatePinnedToCore(
        (TaskFunction_t)core1_task, "test_core1", 4096, nullptr, 3, &core1_task_handle, 1
    );
    
    // Let tasks run for a while
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Stop tasks
    multicore_test_running = false;
    
    // Wait for both tasks to complete
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(sync_semaphore, pdMS_TO_TICKS(2000)));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(sync_semaphore, pdMS_TO_TICKS(2000)));
    
    // Verify both cores were active
    TEST_ASSERT_GREATER_THAN(50, core0_counter);
    TEST_ASSERT_GREATER_THAN(50, core1_counter);
    
    ESP_LOGI(TAG, "Multicore test results: Core0=%lu, Core1=%lu iterations", 
             core0_counter, core1_counter);
    
    // Cleanup
    vSemaphoreDelete(sync_semaphore);
    vQueueDelete(inter_core_queue);
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Basic Multicore Operation", &metrics);
    
    ESP_LOGI(TAG, "✅ Basic multicore operation test PASSED");
}

/**
 * @brief Test inter-core synchronization
 * 
 * Tests various synchronization mechanisms between cores:
 * - Semaphores
 * - Mutexes
 * - Queues
 * - Shared memory access
 */
void test_inter_core_synchronization() {
    ESP_LOGI(TAG, "=== Testing Inter-Core Synchronization ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Shared data structure for synchronization testing
    struct shared_data_t {
        volatile uint32_t counter;
        volatile uint32_t core0_writes;
        volatile uint32_t core1_writes;
        SemaphoreHandle_t mutex;
    };
    
    shared_data_t shared_data = {0, 0, 0, nullptr};
    shared_data.mutex = xSemaphoreCreateMutex();
    TEST_ASSERT_NOT_NULL(shared_data.mutex);
    
    const uint32_t test_iterations = 1000;
    multicore_test_running = true;
    
    // Core 0 synchronization task
    auto sync_task_core0 = [](void* param) {
        shared_data_t* data = (shared_data_t*)param;
        
        for (uint32_t i = 0; i < test_iterations && multicore_test_running; i++) {
            // Acquire mutex
            if (xSemaphoreTake(data->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Critical section
                uint32_t old_value = data->counter;
                vTaskDelay(1); // Simulate some work
                data->counter = old_value + 1;
                data->core0_writes = data->core0_writes + 1;
                
                // Release mutex
                xSemaphoreGive(data->mutex);
            }
            
            vTaskDelay(1);
        }
        
        ESP_LOGI(TAG, "Core 0 sync task completed %lu writes", data->core0_writes);
        vTaskDelete(nullptr);
    };
    
    // Core 1 synchronization task
    auto sync_task_core1 = [](void* param) {
        shared_data_t* data = (shared_data_t*)param;
        
        for (uint32_t i = 0; i < test_iterations && multicore_test_running; i++) {
            // Acquire mutex
            if (xSemaphoreTake(data->mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Critical section
                uint32_t old_value = data->counter;
                vTaskDelay(1); // Simulate some work
                data->counter = old_value + 1;
                data->core1_writes = data->core1_writes + 1;
                
                // Release mutex
                xSemaphoreGive(data->mutex);
            }
            
            vTaskDelay(1);
        }
        
        ESP_LOGI(TAG, "Core 1 sync task completed %lu writes", data->core1_writes);
        vTaskDelete(nullptr);
    };
    
    // Create synchronization tasks
    TaskHandle_t sync_task0_handle, sync_task1_handle;
    
    xTaskCreatePinnedToCore(
        (TaskFunction_t)sync_task_core0, "sync_core0", 4096, &shared_data, 4, &sync_task0_handle, 0
    );
    
    xTaskCreatePinnedToCore(
        (TaskFunction_t)sync_task_core1, "sync_core1", 4096, &shared_data, 4, &sync_task1_handle, 1
    );
    
    // Wait for tasks to complete
    vTaskDelay(pdMS_TO_TICKS(15000)); // Give plenty of time
    
    multicore_test_running = false;
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Verify synchronization worked correctly
    uint32_t expected_counter = shared_data.core0_writes + shared_data.core1_writes;
    
    ESP_LOGI(TAG, "Synchronization results:");
    ESP_LOGI(TAG, "  Counter: %lu (expected: %lu)", shared_data.counter, expected_counter);
    ESP_LOGI(TAG, "  Core 0 writes: %lu", shared_data.core0_writes);
    ESP_LOGI(TAG, "  Core 1 writes: %lu", shared_data.core1_writes);
    
    // Counter should match total writes (no race conditions)
    TEST_ASSERT_EQUAL(expected_counter, shared_data.counter);
    
    // Both cores should have done significant work
    TEST_ASSERT_GREATER_THAN(100, shared_data.core0_writes);
    TEST_ASSERT_GREATER_THAN(100, shared_data.core1_writes);
    
    // Cleanup
    vSemaphoreDelete(shared_data.mutex);
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Inter-Core Synchronization", &metrics);
    
    ESP_LOGI(TAG, "✅ Inter-core synchronization test PASSED");
}

/**
 * @brief Test multicore performance and load balancing
 * 
 * Measures performance characteristics of multicore operation:
 * - Load distribution between cores
 * - Performance scaling
 * - Core utilization
 * - Memory access patterns
 */
void test_multicore_performance() {
    ESP_LOGI(TAG, "=== Testing Multicore Performance ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Performance measurement structure
    struct core_performance_t {
        volatile uint32_t iterations;
        volatile uint32_t total_time_us;
        volatile uint32_t min_time_us;
        volatile uint32_t max_time_us;
        uint8_t core_id;
    };
    
    core_performance_t core0_perf = {0, 0, UINT32_MAX, 0, 0};
    core_performance_t core1_perf = {0, 0, UINT32_MAX, 0, 1};
    
    const uint32_t test_duration_ms = 10000;
    multicore_test_running = true;
    
    // Performance measurement task
    auto perf_task = [](void* param) {
        core_performance_t* perf = (core_performance_t*)param;
        perf->core_id = xPortGetCoreID();
        
        ESP_LOGI(TAG, "Performance task started on core %d", perf->core_id);
        
        while (multicore_test_running) {
            uint32_t start_time = esp_timer_get_time();
            
            // Simulate computational work
            volatile float result = 0;
            for (int i = 0; i < 1000; i++) {
                result += sqrt(i * 3.14159f);
            }
            
            uint32_t end_time = esp_timer_get_time();
            uint32_t iteration_time = end_time - start_time;
            
            // Update performance metrics
            perf->iterations = perf->iterations + 1;
            perf->total_time_us += iteration_time;
            perf->min_time_us = (iteration_time < perf->min_time_us) ? iteration_time : perf->min_time_us;
            perf->max_time_us = (iteration_time > perf->max_time_us) ? iteration_time : perf->max_time_us;
            
            // Small delay to prevent watchdog
            vTaskDelay(1);
        }
        
        ESP_LOGI(TAG, "Core %d performance task completed %lu iterations", 
                 perf->core_id, perf->iterations);
        vTaskDelete(nullptr);
    };
    
    // Create performance tasks on both cores
    TaskHandle_t perf_task0_handle, perf_task1_handle;
    
    xTaskCreatePinnedToCore(
        (TaskFunction_t)perf_task, "perf_core0", 4096, &core0_perf, 3, &perf_task0_handle, 0
    );
    
    xTaskCreatePinnedToCore(
        (TaskFunction_t)perf_task, "perf_core1", 4096, &core1_perf, 3, &perf_task1_handle, 1
    );
    
    // Let performance tasks run
    vTaskDelay(pdMS_TO_TICKS(test_duration_ms));
    
    multicore_test_running = false;
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Calculate performance metrics
    uint32_t core0_avg_time = (core0_perf.iterations > 0) ? 
        (core0_perf.total_time_us / core0_perf.iterations) : 0;
    uint32_t core1_avg_time = (core1_perf.iterations > 0) ? 
        (core1_perf.total_time_us / core1_perf.iterations) : 0;
    
    float core0_iterations_per_sec = (float)core0_perf.iterations / (test_duration_ms / 1000.0f);
    float core1_iterations_per_sec = (float)core1_perf.iterations / (test_duration_ms / 1000.0f);
    
    ESP_LOGI(TAG, "Multicore performance results:");
    ESP_LOGI(TAG, "Core 0: %lu iterations, %.1f iter/sec, %lu/%lu/%lu μs (min/avg/max)",
             core0_perf.iterations, core0_iterations_per_sec,
             core0_perf.min_time_us, core0_avg_time, core0_perf.max_time_us);
    ESP_LOGI(TAG, "Core 1: %lu iterations, %.1f iter/sec, %lu/%lu/%lu μs (min/avg/max)",
             core1_perf.iterations, core1_iterations_per_sec,
             core1_perf.min_time_us, core1_avg_time, core1_perf.max_time_us);
    
    // Verify both cores performed significant work
    TEST_ASSERT_GREATER_THAN(100, core0_perf.iterations);
    TEST_ASSERT_GREATER_THAN(100, core1_perf.iterations);
    
    // Performance should be reasonably balanced (within 50% of each other)
    float performance_ratio = core0_iterations_per_sec / core1_iterations_per_sec;
    TEST_ASSERT_GREATER_THAN(0.5f, performance_ratio);
    TEST_ASSERT_LESS_THAN(2.0f, performance_ratio);
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Multicore Performance", &metrics);
    
    ESP_LOGI(TAG, "✅ Multicore performance test PASSED");
}

/**
 * @brief Test real application multicore behavior
 * 
 * Tests the actual ModESP application multicore setup:
 * - Main loop on Core 0
 * - Sensor task on Core 1
 * - Inter-core communication via events
 * - Real-world performance characteristics
 */
void test_application_multicore_behavior() {
    ESP_LOGI(TAG, "=== Testing Application Multicore Behavior ===");
    
    TEST_ASSERT_TRUE(Application::is_running());
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Monitor application multicore behavior
    const uint32_t monitoring_duration_ms = 10000;
    const uint32_t sample_interval_ms = 1000;
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    uint32_t last_sample = start_time;
    uint32_t samples_taken = 0;
    
    uint32_t total_cpu_usage = 0;
    size_t min_free_heap = SIZE_MAX;
    size_t max_free_heap = 0;
    
    ESP_LOGI(TAG, "Monitoring application multicore behavior for %lu ms...", 
             monitoring_duration_ms);
    
    while ((esp_timer_get_time() / 1000) - start_time < monitoring_duration_ms) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        
        if (current_time - last_sample >= sample_interval_ms) {
            samples_taken++;
            
            // Sample system metrics
            uint8_t cpu_usage = Application::get_cpu_usage();
            size_t free_heap = Application::get_free_heap();
            uint32_t uptime = Application::get_uptime_ms();
            
            total_cpu_usage += cpu_usage;
            min_free_heap = (free_heap < min_free_heap) ? free_heap : min_free_heap;
            max_free_heap = (free_heap > max_free_heap) ? free_heap : max_free_heap;
            
            // Generate some inter-core events
            EventBus::publish("multicore.test.core0", {
                {"timestamp", current_time},
                {"cpu_usage", cpu_usage},
                {"free_heap", free_heap}
            });
            
            ESP_LOGI(TAG, "Sample %lu: CPU=%u%%, heap=%zu KB, uptime=%lu ms",
                     samples_taken, cpu_usage, free_heap / 1024, uptime);
            
            // Verify system health
            TEST_ASSERT_TRUE(Application::is_running());
            TEST_ASSERT_GREATER_THAN(MIN_FREE_HEAP_BYTES, free_heap);
            
            last_sample = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Calculate average metrics
    uint8_t avg_cpu_usage = (samples_taken > 0) ? (total_cpu_usage / samples_taken) : 0;
    
    ESP_LOGI(TAG, "Application multicore behavior summary:");
    ESP_LOGI(TAG, "  Samples taken: %lu", samples_taken);
    ESP_LOGI(TAG, "  Average CPU usage: %u%%", avg_cpu_usage);
    ESP_LOGI(TAG, "  Heap range: %zu - %zu KB", min_free_heap / 1024, max_free_heap / 1024);
    
    // Verify reasonable performance
    TEST_ASSERT_GREATER_THAN(5, samples_taken);
    TEST_ASSERT_LESS_THAN(MAX_CPU_USAGE_PERCENT, avg_cpu_usage);
    TEST_ASSERT_GREATER_THAN(MIN_FREE_HEAP_BYTES, min_free_heap);
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Application Multicore Behavior", &metrics);
    
    ESP_LOGI(TAG, "✅ Application multicore behavior test PASSED");
}

// Test runner for multicore tests
extern "C" void run_multicore_tests(void) {
    ESP_LOGI(TAG, "🔄 Starting Multicore Tests");
    
    RUN_TEST(test_basic_multicore_operation);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    RUN_TEST(test_inter_core_synchronization);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    RUN_TEST(test_multicore_performance);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    RUN_TEST(test_application_multicore_behavior);
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    ESP_LOGI(TAG, "✅ Multicore Tests Complete");
} 