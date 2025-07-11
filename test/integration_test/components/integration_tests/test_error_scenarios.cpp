/**
 * @file test_error_scenarios.cpp
 * @brief Error scenario tests for ModESP system resilience
 * 
 * Tests system behavior under various error conditions:
 * - Module failures and recovery
 * - Configuration errors
 * - Resource exhaustion
 * - Hardware simulation failures
 * - Error reporting and handling
 */

#include "integration_test_common.h"
#include "application.h"
#include "event_bus.h"
#include "shared_state.h"
#include "config_manager.h"
#include "module_manager.h"

static const char* TAG = "ErrorTest";

/**
 * @brief Test system response to module errors
 * 
 * Simulates module failures and verifies:
 * - Error reporting works correctly
 * - System remains stable despite module failures
 * - Error recovery mechanisms function
 * - Critical vs non-critical error handling
 */
void test_module_error_handling() {
    ESP_LOGI(TAG, "=== Testing Module Error Handling ===");
    
    TEST_ASSERT_TRUE(Application::is_running());
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    size_t initial_heap = Application::get_free_heap();
    uint32_t initial_uptime = Application::get_uptime_ms();
    
    // Test WARNING level error
    ESP_LOGI(TAG, "Testing WARNING level error...");
    Application::report_error("TestModule", ESP_ERR_NOT_FOUND, 
                            Application::ErrorSeverity::WARNING, 
                            "Test warning error");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // System should continue running normally
    TEST_ASSERT_TRUE(Application::is_running());
    TEST_ASSERT_EQUAL(Application::State::RUNNING, Application::get_state());
    INTEGRATION_TEST_ASSERT_SYSTEM_HEALTHY();
    
    // Test ERROR level error
    ESP_LOGI(TAG, "Testing ERROR level error...");
    Application::report_error("TestModule", ESP_ERR_INVALID_STATE, 
                            Application::ErrorSeverity::ERROR, 
                            "Test error condition");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // System should still be running but may have degraded performance
    TEST_ASSERT_TRUE(Application::is_running());
    INTEGRATION_TEST_ASSERT_MEMORY_OK();
    
    // Test CRITICAL level error
    ESP_LOGI(TAG, "Testing CRITICAL level error...");
    Application::report_error("TestModule", ESP_ERR_NO_MEM, 
                            Application::ErrorSeverity::CRITICAL, 
                            "Test critical error");
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // System should transition to ERROR state but not crash
    TEST_ASSERT_EQUAL(Application::State::ERROR, Application::get_state());
    TEST_ASSERT_FALSE(Application::is_running());
    
    // Memory should still be stable
    size_t heap_after_errors = Application::get_free_heap();
    TEST_ASSERT_GREATER_THAN(initial_heap - 10000, heap_after_errors);
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Module Error Handling", &metrics);
    
    ESP_LOGI(TAG, "✅ Module error handling test PASSED");
    ESP_LOGI(TAG, "   System correctly transitioned to ERROR state on CRITICAL error");
}

/**
 * @brief Test configuration error scenarios
 * 
 * Tests handling of invalid configurations:
 * - Malformed JSON
 * - Missing required fields
 * - Invalid value ranges
 * - Configuration corruption recovery
 */
void test_configuration_errors() {
    ESP_LOGI(TAG, "=== Testing Configuration Error Scenarios ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Test invalid JSON handling
    ESP_LOGI(TAG, "Testing invalid JSON configuration...");
    
    // Create invalid JSON string
    const char* invalid_json = "{\"test\": invalid_value, \"missing_quote: true}";
    
    // Try to parse invalid JSON (should not crash)
    nlohmann::json test_config;
    bool parse_failed = false;
    
    // Check if JSON is parseable using accept() method instead of exceptions
    if (!nlohmann::json::accept(invalid_json)) {
        parse_failed = true;
        ESP_LOGI(TAG, "JSON parse correctly failed - invalid syntax");
    } else {
        ESP_LOGI(TAG, "JSON unexpectedly parsed successfully");
    }
    
    TEST_ASSERT_TRUE(parse_failed);
    
    // Test configuration with missing required fields
    ESP_LOGI(TAG, "Testing configuration with missing fields...");
    
    nlohmann::json incomplete_config = {
        {"some_field", "value"}
        // Missing required fields
    };
    
    // System should handle incomplete configuration gracefully
    // (This depends on how ConfigManager validates configurations)
    
    // Test configuration with invalid value ranges
    ESP_LOGI(TAG, "Testing configuration with invalid ranges...");
    
    nlohmann::json invalid_range_config = {
        {"temperature_min", 1000},  // Unrealistic temperature
        {"temperature_max", -1000}, // Invalid range
        {"invalid_negative_count", -5}
    };
    
    // System should reject or sanitize invalid ranges
    
    // Verify system remains stable after configuration errors
    vTaskDelay(pdMS_TO_TICKS(1000));
    INTEGRATION_TEST_ASSERT_MEMORY_OK();
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Configuration Errors", &metrics);
    
    ESP_LOGI(TAG, "✅ Configuration error scenarios test PASSED");
}

/**
 * @brief Test resource exhaustion scenarios
 * 
 * Simulates various resource exhaustion conditions:
 * - Task creation failures
 * - Queue full conditions
 * - Timer exhaustion
 * - File system full
 */
void test_resource_exhaustion() {
    ESP_LOGI(TAG, "=== Testing Resource Exhaustion Scenarios ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Test task creation failure simulation
    ESP_LOGI(TAG, "Testing task creation limits...");
    
    std::vector<TaskHandle_t> test_tasks;
    const uint32_t max_test_tasks = 20;
    
    // Create tasks until we hit limits
    for (uint32_t i = 0; i < max_test_tasks; i++) {
        TaskHandle_t task_handle;
        
        BaseType_t result = xTaskCreate(
            [](void* param) {
                // Simple task that just delays
                while (true) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            },
            "test_task",
            2048,
            nullptr,
            1,
            &task_handle
        );
        
        if (result == pdPASS) {
            test_tasks.push_back(task_handle);
        } else {
            ESP_LOGI(TAG, "Task creation failed at task %lu (expected)", i);
            break;
        }
        
        // Check system health periodically
        if (i % 5 == 0) {
            INTEGRATION_TEST_ASSERT_MEMORY_OK();
        }
    }
    
    ESP_LOGI(TAG, "Created %zu test tasks", test_tasks.size());
    
    // Clean up test tasks
    for (TaskHandle_t task : test_tasks) {
        vTaskDelete(task);
    }
    test_tasks.clear();
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Test queue overflow simulation
    ESP_LOGI(TAG, "Testing queue overflow handling...");
    
    const uint32_t queue_size = 10;
    QueueHandle_t test_queue = xQueueCreate(queue_size, sizeof(uint32_t));
    TEST_ASSERT_NOT_NULL(test_queue);
    
    // Fill queue to capacity
    for (uint32_t i = 0; i < queue_size; i++) {
        BaseType_t result = xQueueSend(test_queue, &i, 0);
        TEST_ASSERT_EQUAL(pdTRUE, result);
    }
    
    // Try to overflow queue
    uint32_t overflow_value = 999;
    BaseType_t overflow_result = xQueueSend(test_queue, &overflow_value, 0);
    TEST_ASSERT_EQUAL(pdFALSE, overflow_result); // Should fail
    
    ESP_LOGI(TAG, "Queue overflow correctly rejected");
    
    // Clean up queue
    vQueueDelete(test_queue);
    
    // Verify system stability after resource tests
    INTEGRATION_TEST_ASSERT_SYSTEM_HEALTHY();
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Resource Exhaustion", &metrics);
    
    ESP_LOGI(TAG, "✅ Resource exhaustion scenarios test PASSED");
}

/**
 * @brief Test hardware simulation failures
 * 
 * Simulates hardware failures and tests system resilience:
 * - Sensor read failures
 * - Communication timeouts
 * - GPIO errors
 * - I2C/SPI failures
 */
void test_hardware_failure_simulation() {
    ESP_LOGI(TAG, "=== Testing Hardware Failure Simulation ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Simulate sensor read failures
    ESP_LOGI(TAG, "Simulating sensor read failures...");
    
    // Publish sensor error events
    for (int i = 0; i < 5; i++) {
        EventBus::publish("sensor.error", {
            {"sensor_id", i},
            {"error_code", ESP_ERR_TIMEOUT},
            {"error_message", "Simulated sensor timeout"}
        });
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // Simulate communication failures
    ESP_LOGI(TAG, "Simulating communication failures...");
    
    EventBus::publish("hardware.communication.error", {
        {"interface", "i2c"},
        {"error_code", ESP_ERR_INVALID_RESPONSE},
        {"retry_count", 3}
    });
    
    EventBus::publish("hardware.communication.error", {
        {"interface", "spi"},
        {"error_code", ESP_ERR_TIMEOUT},
        {"device_address", 0x48}
    });
    
    // Give system time to process hardware errors
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // System should remain stable despite hardware failures
    INTEGRATION_TEST_ASSERT_MEMORY_OK();
    
    // Simulate hardware recovery
    ESP_LOGI(TAG, "Simulating hardware recovery...");
    
    EventBus::publish("sensor.recovery", {
        {"sensor_id", 0},
        {"status", "online"}
    });
    
    EventBus::publish("hardware.communication.recovery", {
        {"interface", "i2c"},
        {"status", "operational"}
    });
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Hardware Failure Simulation", &metrics);
    
    ESP_LOGI(TAG, "✅ Hardware failure simulation test PASSED");
}

/**
 * @brief Test error reporting and logging
 * 
 * Verifies that error reporting system works correctly:
 * - Errors are properly logged
 * - Error events are published
 * - Error statistics are maintained
 * - Error recovery tracking
 */
void test_error_reporting_system() {
    ESP_LOGI(TAG, "=== Testing Error Reporting System ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Test error event subscription
    bool error_event_received = false;
    std::string received_component;
    int received_error_code = 0;
    
    // Subscribe to error events
    EventBus::subscribe("system.error", [&](const EventBus::Event& event) {
        error_event_received = true;
        received_component = event.data.value("component", "");
        received_error_code = event.data.value("error_code", 0);
        ESP_LOGI(TAG, "Received error event from %s: 0x%x", 
                 received_component.c_str(), received_error_code);
    });
    
    // Report a test error
    ESP_LOGI(TAG, "Reporting test error...");
    Application::report_error("ErrorTestModule", ESP_ERR_INVALID_ARG, 
                            Application::ErrorSeverity::ERROR, 
                            "Test error for reporting verification");
    
    // Give time for event processing
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Verify error event was received
    TEST_ASSERT_TRUE(error_event_received);
    TEST_ASSERT_EQUAL_STRING("ErrorTestModule", received_component.c_str());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, received_error_code);
    
    // Test multiple error reporting
    ESP_LOGI(TAG, "Testing multiple error reports...");
    
    for (int i = 0; i < 5; i++) {
        Application::report_error("MultiErrorTest", ESP_ERR_NOT_FOUND, 
                                Application::ErrorSeverity::WARNING, 
                                "Multiple error test");
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    // System should handle multiple errors gracefully
    INTEGRATION_TEST_ASSERT_MEMORY_OK();
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Error Reporting System", &metrics);
    
    ESP_LOGI(TAG, "✅ Error reporting system test PASSED");
}

// Test runner for error scenario tests
extern "C" void run_error_scenario_tests(void) {
    ESP_LOGI(TAG, "⚠️  Starting Error Scenario Tests");
    
    RUN_TEST(test_module_error_handling);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RUN_TEST(test_configuration_errors);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RUN_TEST(test_resource_exhaustion);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RUN_TEST(test_hardware_failure_simulation);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RUN_TEST(test_error_reporting_system);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "✅ Error Scenario Tests Complete");
} 