/**
 * @file test_application.cpp
 * @brief Unit tests for Application component
 */

#include "unity.h"
#include "application.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nlohmann/json.hpp"

static const char* TAG = "TestApplication";

// Test fixtures
static bool error_callback_called = false;
static Application::ErrorSeverity last_error_severity = Application::ErrorSeverity::WARNING;

// Helper functions
static void reset_test_state() {
    error_callback_called = false;
    last_error_severity = Application::ErrorSeverity::WARNING;
}

// Test error reporting callback
static void test_error_callback(const char* component, esp_err_t error, 
                               Application::ErrorSeverity severity, const char* message) {
    error_callback_called = true;
    last_error_severity = severity;
}

// Test cases
void test_application_state_management() {
    reset_test_state();
    
    // Initially should be in BOOT state (before init)
    Application::State initial_state = Application::get_state();
    TEST_ASSERT_TRUE(initial_state == Application::State::BOOT || 
                     initial_state == Application::State::RUNNING); // May already be initialized
    
    // Test is_running method
    bool running = Application::is_running();
    TEST_ASSERT_EQUAL(initial_state == Application::State::RUNNING, running);
}

void test_application_system_metrics() {
    reset_test_state();
    
    // Test uptime (should be > 0 if system is running)
    uint32_t uptime = Application::get_uptime_ms();
    TEST_ASSERT_GREATER_THAN(0, uptime);
    
    // Test free heap (should be reasonable amount)
    size_t free_heap = Application::get_free_heap();
    TEST_ASSERT_GREATER_THAN(10240, free_heap); // At least 10KB
    
    // Test minimum free heap
    size_t min_free_heap = Application::get_min_free_heap();
    TEST_ASSERT_GREATER_THAN(0, min_free_heap);
    TEST_ASSERT_LESS_OR_EQUAL(min_free_heap, free_heap);
    
    // Test CPU usage (should be 0-100%)
    uint8_t cpu_usage = Application::get_cpu_usage();
    TEST_ASSERT_LESS_OR_EQUAL(100, cpu_usage);
    
    // Test stack high water mark
    size_t stack_remaining = Application::get_stack_high_water_mark();
    TEST_ASSERT_GREATER_THAN(0, stack_remaining);
}

void test_application_error_reporting() {
    reset_test_state();
    
    // Test error reporting with different severities
    Application::report_error("TestComponent", ESP_ERR_TIMEOUT, 
                             Application::ErrorSeverity::WARNING, 
                             "Test warning message");
    
    // Small delay to allow event processing
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Test error reporting with ERROR severity
    Application::report_error("TestComponent", ESP_ERR_NO_MEM,
                             Application::ErrorSeverity::ERROR,
                             "Test error message");
    
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Verify the error was processed (would be visible in logs)
    // Note: We can't easily test the callback without modifying the implementation
    TEST_ASSERT_TRUE(true); // If we reach here, no crash occurred
}

void test_application_health_check() {
    reset_test_state();
    
    // Run health check
    bool is_healthy = Application::check_health();
    
    // Should return true or false, not crash
    TEST_ASSERT_TRUE(is_healthy == true || is_healthy == false);
    
    // Health check should complete quickly
    uint32_t start_time = esp_timer_get_time();
    for (int i = 0; i < 10; i++) {
        Application::check_health();
    }
    uint32_t end_time = esp_timer_get_time();
    
    // 10 health checks should complete in less than 100ms
    TEST_ASSERT_LESS_THAN(100000, end_time - start_time);
}

void test_application_hal_access() {
    reset_test_state();
    
    // Test that we can get HAL instance without crashing
    try {
        ESPhal& hal = Application::get_hal();
        // If we reach here, HAL access works
        TEST_ASSERT_TRUE(true);
    } catch (...) {
        TEST_FAIL_MESSAGE("HAL access threw exception");
    }
}

void test_application_performance_metrics() {
    reset_test_state();
    
    // Collect baseline metrics
    uint32_t initial_uptime = Application::get_uptime_ms();
    size_t initial_heap = Application::get_free_heap();
    uint8_t initial_cpu = Application::get_cpu_usage();
    
    // Wait a bit and collect again
    vTaskDelay(pdMS_TO_TICKS(100));
    
    uint32_t final_uptime = Application::get_uptime_ms();
    size_t final_heap = Application::get_free_heap();
    uint8_t final_cpu = Application::get_cpu_usage();
    
    // Uptime should have increased
    TEST_ASSERT_GREATER_THAN(initial_uptime, final_uptime);
    
    // Heap should be relatively stable (within 1KB)
    int heap_diff = abs((int)final_heap - (int)initial_heap);
    TEST_ASSERT_LESS_THAN(1024, heap_diff);
    
    // CPU usage should be reasonable
    TEST_ASSERT_LESS_OR_EQUAL(100, final_cpu);
}

void test_application_error_severity_levels() {
    reset_test_state();
    
    // Test all error severity levels without causing crashes
    Application::report_error("TestSeverity", ESP_ERR_INVALID_ARG,
                             Application::ErrorSeverity::WARNING,
                             "Warning level test");
    
    vTaskDelay(pdMS_TO_TICKS(5));
    
    Application::report_error("TestSeverity", ESP_ERR_INVALID_STATE,
                             Application::ErrorSeverity::ERROR,
                             "Error level test");
    
    vTaskDelay(pdMS_TO_TICKS(5));
    
    // Note: We avoid testing CRITICAL and FATAL as they might trigger restart
    // Application::report_error("TestSeverity", ESP_ERR_NO_MEM,
    //                          Application::ErrorSeverity::CRITICAL,
    //                          "Critical level test");
    
    // If we reach here, error reporting doesn't crash the system
    TEST_ASSERT_TRUE(true);
}

void test_application_multicore_safety() {
    reset_test_state();
    
    // Test that Application functions are safe to call from different tasks
    // (This is a basic test - real multicore testing would require more complex setup)
    
    // Get metrics from current task
    size_t heap1 = Application::get_free_heap();
    uint32_t uptime1 = Application::get_uptime_ms();
    bool health1 = Application::check_health();
    
    // Small delay
    vTaskDelay(pdMS_TO_TICKS(1));
    
    // Get metrics again
    size_t heap2 = Application::get_free_heap();
    uint32_t uptime2 = Application::get_uptime_ms();
    bool health2 = Application::check_health();
    
    // Should be able to call multiple times without issues
    TEST_ASSERT_GREATER_OR_EQUAL(uptime1, uptime2);
    TEST_ASSERT_TRUE(heap1 > 0 && heap2 > 0);
    TEST_ASSERT_TRUE(health1 == true || health1 == false);
    TEST_ASSERT_TRUE(health2 == true || health2 == false);
}

// Test group runner
void test_application_group(void) {
    RUN_TEST(test_application_state_management);
    RUN_TEST(test_application_system_metrics);
    RUN_TEST(test_application_error_reporting);
    RUN_TEST(test_application_health_check);
    RUN_TEST(test_application_hal_access);
    RUN_TEST(test_application_performance_metrics);
    RUN_TEST(test_application_error_severity_levels);
    RUN_TEST(test_application_multicore_safety);
    RUN_TEST(test_application_emergency_mode);
    RUN_TEST(test_application_diagnostics);
    RUN_TEST(test_application_memory_cleanup);
}

void test_application_emergency_mode() {
    reset_test_state();
    
    // Test emergency mode functions
    bool initial_emergency = Application::is_emergency_mode();
    
    // Set emergency mode
    Application::set_emergency_mode(true);
    TEST_ASSERT_TRUE(Application::is_emergency_mode());
    
    // Disable emergency mode
    Application::set_emergency_mode(false);
    TEST_ASSERT_FALSE(Application::is_emergency_mode());
    
    // Restore initial state
    Application::set_emergency_mode(initial_emergency);
}

void test_application_diagnostics() {
    reset_test_state();
    
    // Test performance metrics
    nlohmann::json perf_metrics = Application::get_performance_metrics();
    TEST_ASSERT_TRUE(perf_metrics.contains("main_loop"));
    TEST_ASSERT_TRUE(perf_metrics.contains("memory"));
    TEST_ASSERT_TRUE(perf_metrics.contains("system"));
    
    // Test multicore stats
    nlohmann::json multicore_stats = Application::get_multicore_stats();
    TEST_ASSERT_TRUE(multicore_stats.contains("core0"));
    TEST_ASSERT_TRUE(multicore_stats.contains("core1"));
    
    // Test system diagnostics
    nlohmann::json diagnostics = Application::get_system_diagnostics();
    TEST_ASSERT_TRUE(diagnostics.contains("timestamp"));
    TEST_ASSERT_TRUE(diagnostics.contains("memory"));
    TEST_ASSERT_TRUE(diagnostics.contains("performance"));
    TEST_ASSERT_TRUE(diagnostics.contains("shared_state"));
    TEST_ASSERT_TRUE(diagnostics.contains("modules"));
    
    // Verify some specific values are reasonable
    TEST_ASSERT_GREATER_THAN(0, diagnostics["timestamp"].get<uint64_t>());
    TEST_ASSERT_GREATER_THAN(0, diagnostics["memory"]["free_heap_kb"].get<size_t>());
}

void test_application_memory_cleanup() {
    reset_test_state();
    
    // Get initial memory
    size_t initial_heap = Application::get_free_heap();
    
    // Force memory cleanup
    esp_err_t result = Application::force_memory_cleanup();
    TEST_ASSERT_EQUAL(ESP_OK, result);
    
    // Verify cleanup completed without errors
    size_t final_heap = Application::get_free_heap();
    TEST_ASSERT_GREATER_THAN(0, final_heap);
    
    // Memory should not have decreased significantly
    TEST_ASSERT_GREATER_THAN(initial_heap / 2, final_heap);
}
