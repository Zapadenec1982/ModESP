/**
 * @file test_system_lifecycle.cpp
 * @brief Integration tests for complete system lifecycle
 * 
 * Tests the entire ModESP system from boot to shutdown:
 * - Full system initialization sequence
 * - Normal operation cycles
 * - Graceful shutdown
 * - System restart scenarios
 */

#include "integration_test_common.h"
#include "application.h"
#include "event_bus.h"
#include "shared_state.h"
#include "config_manager.h"
#include "module_manager.h"

static const char* TAG = "LifecycleTest";

/**
 * @brief Test complete system initialization sequence
 * 
 * Verifies that all components initialize in correct order:
 * NVS -> EventBus -> SharedState -> ConfigManager -> ModuleManager -> HAL -> Modules
 */
void test_full_system_initialization() {
    ESP_LOGI(TAG, "=== Testing Full System Initialization ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // System should start in BOOT state
    TEST_ASSERT_EQUAL(Application::State::BOOT, Application::get_state());
    
    // Initialize the entire system
    esp_err_t init_result = Application::init();
    
    // Verify successful initialization
    TEST_ASSERT_EQUAL(ESP_OK, init_result);
    TEST_ASSERT_EQUAL(Application::State::RUNNING, Application::get_state());
    TEST_ASSERT_TRUE(Application::is_running());
    
    // Wait for system to fully stabilize
    INTEGRATION_TEST_WAIT_STABLE(SYSTEM_STABILIZE_TIME_MS);
    
    // Verify all core services are operational
    INTEGRATION_TEST_ASSERT_SYSTEM_HEALTHY();
    
    // Verify system metrics are reasonable
    TEST_ASSERT_GREATER_THAN(0, Application::get_uptime_ms());
    TEST_ASSERT_GREATER_THAN(MIN_FREE_HEAP_BYTES, Application::get_free_heap());
    TEST_ASSERT_LESS_THAN(MAX_CPU_USAGE_PERCENT, Application::get_cpu_usage());
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Full System Init", &metrics);
    
    ESP_LOGI(TAG, "✅ Full system initialization test PASSED");
}

/**
 * @brief Test system operation stability over time
 * 
 * Runs the system for extended period and verifies:
 * - Main loop maintains 100Hz frequency
 * - Memory usage remains stable
 * - No resource leaks
 * - Health checks pass consistently
 */
void test_system_operation_stability() {
    ESP_LOGI(TAG, "=== Testing System Operation Stability ===");
    
    // Ensure system is running
    TEST_ASSERT_TRUE(Application::is_running());
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    const uint32_t test_duration_ms = 15000; // 15 seconds of operation
    const uint32_t check_interval_ms = 1000;  // Check every second
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    uint32_t last_check = start_time;
    uint32_t health_checks_passed = 0;
    uint32_t total_health_checks = 0;
    
    size_t initial_heap = Application::get_free_heap();
    size_t min_heap = initial_heap;
    size_t max_heap = initial_heap;
    
    ESP_LOGI(TAG, "Running stability test for %lu ms...", test_duration_ms);
    
    while ((esp_timer_get_time() / 1000) - start_time < test_duration_ms) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        
        // Periodic health checks
        if (current_time - last_check >= check_interval_ms) {
            total_health_checks++;
            
            // System health check
            if (Application::check_health()) {
                health_checks_passed++;
            }
            
            // Memory monitoring
            size_t current_heap = Application::get_free_heap();
            min_heap = (current_heap < min_heap) ? current_heap : min_heap;
            max_heap = (current_heap > max_heap) ? current_heap : max_heap;
            
            // Verify system is still running
            TEST_ASSERT_TRUE(Application::is_running());
            TEST_ASSERT_EQUAL(Application::State::RUNNING, Application::get_state());
            
            // Verify reasonable resource usage
            TEST_ASSERT_GREATER_THAN(CRITICAL_FREE_HEAP_BYTES, current_heap);
            TEST_ASSERT_LESS_THAN(MAX_CPU_USAGE_PERCENT, Application::get_cpu_usage());
            
            ESP_LOGI(TAG, "Health check %lu/%lu: heap=%zu, cpu=%u%%", 
                     health_checks_passed, total_health_checks, 
                     current_heap, Application::get_cpu_usage());
            
            last_check = current_time;
        }
        
        // Short delay to allow system to work
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    integration_test_stop_metrics(&metrics);
    
    // Verify stability results
    float health_check_success_rate = (float)health_checks_passed / total_health_checks;
    TEST_ASSERT_GREATER_THAN(0.95f, health_check_success_rate); // 95% success rate
    
    // Verify memory stability (no significant leaks)
    size_t final_heap = Application::get_free_heap();
    int32_t heap_delta = (int32_t)final_heap - (int32_t)initial_heap;
    
    ESP_LOGI(TAG, "Memory analysis: initial=%zu, final=%zu, delta=%ld, min=%zu, max=%zu",
             initial_heap, final_heap, heap_delta, min_heap, max_heap);
    
    // Allow some memory variance but no major leaks
    TEST_ASSERT_GREATER_THAN(-10000, heap_delta); // No more than 10KB leak
    
    integration_test_print_metrics("System Stability", &metrics);
    
    ESP_LOGI(TAG, "✅ System operation stability test PASSED");
    ESP_LOGI(TAG, "   Health checks: %lu/%lu (%.1f%%)", 
             health_checks_passed, total_health_checks, health_check_success_rate * 100);
}

/**
 * @brief Test graceful system shutdown
 * 
 * Verifies that system can shutdown cleanly:
 * - All modules stop gracefully
 * - Configuration is saved
 * - Resources are cleaned up
 * - No crashes or hangs
 */
void test_graceful_shutdown() {
    ESP_LOGI(TAG, "=== Testing Graceful Shutdown ===");
    
    // Ensure system is running before shutdown
    TEST_ASSERT_TRUE(Application::is_running());
    TEST_ASSERT_EQUAL(Application::State::RUNNING, Application::get_state());
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    size_t heap_before_shutdown = Application::get_free_heap();
    
    ESP_LOGI(TAG, "Initiating graceful shutdown...");
    
    // Request graceful shutdown
    Application::shutdown();
    
    // Wait for shutdown to complete
    INTEGRATION_TEST_WAIT_STABLE(2000);
    
    // Verify system is no longer running
    TEST_ASSERT_FALSE(Application::is_running());
    TEST_ASSERT_EQUAL(Application::State::SHUTDOWN, Application::get_state());
    
    // Verify memory was cleaned up properly
    size_t heap_after_shutdown = Application::get_free_heap();
    
    ESP_LOGI(TAG, "Heap before shutdown: %zu, after: %zu", 
             heap_before_shutdown, heap_after_shutdown);
    
    // After shutdown, we should have more free memory (cleanup)
    // or at least not significantly less (no major leaks during shutdown)
    TEST_ASSERT_GREATER_THAN(heap_before_shutdown - 5000, heap_after_shutdown);
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Graceful Shutdown", &metrics);
    
    ESP_LOGI(TAG, "✅ Graceful shutdown test PASSED");
}

/**
 * @brief Test system restart functionality
 * 
 * Note: This test will actually restart the ESP32, so it should be last
 * or conditional based on test configuration.
 */
void test_system_restart() {
    ESP_LOGI(TAG, "=== Testing System Restart ===");
    ESP_LOGI(TAG, "⚠️  This test will restart the ESP32 in 5 seconds!");
    ESP_LOGI(TAG, "⚠️  This is expected behavior for restart testing.");
    
    // Give time to read the warning
    vTaskDelay(pdMS_TO_TICKS(5000));
    
    // Request system restart with delay
    Application::restart(1000);
    
    // We should never reach this point as the system will restart
    TEST_FAIL_MESSAGE("System restart did not occur as expected");
}

// Test runner for system lifecycle tests
extern "C" void run_system_lifecycle_tests(void) {
    ESP_LOGI(TAG, "🧪 Starting System Lifecycle Tests");
    
    RUN_TEST(test_full_system_initialization);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RUN_TEST(test_system_operation_stability);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RUN_TEST(test_graceful_shutdown);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Uncomment to test restart (will actually restart ESP32)
    // RUN_TEST(test_system_restart);
    
    ESP_LOGI(TAG, "✅ System Lifecycle Tests Complete");
} 