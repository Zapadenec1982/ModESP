/**
 * @file test_runner.cpp
 * @brief Main test runner for ModESP core components
 */

#include "unity.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>

// Declare test groups
extern void test_event_bus_group(void);
extern void test_config_manager_group(void);
extern void test_config_async_group(void);
extern void test_module_manager_group(void);
extern void test_shared_state_group(void);
extern void test_json_validator_group(void);
extern void test_logger_group(void);

static const char* TAG = "TestRunner";

/**
 * @brief Run all EventBus tests
 */
extern "C" void run_eventbus_tests(void) {
    ESP_LOGI(TAG, "Starting EventBus tests...");
    UNITY_BEGIN();
    test_event_bus_group();
    UNITY_END();
}

/**
 * @brief Run all core component tests
 */
extern "C" void run_all_tests(void) {
    ESP_LOGI(TAG, "Starting all ModESP core tests...");
    
    // Initialize Unity
    UNITY_BEGIN();
    
    // Run test groups
    ESP_LOGI(TAG, "Running EventBus tests...");
    test_event_bus_group();
    
    ESP_LOGI(TAG, "Running ConfigManager tests...");
    test_config_manager_group();
    
    ESP_LOGI(TAG, "Running ConfigManager Async tests...");
    test_config_async_group();
    
    ESP_LOGI(TAG, "Running ModuleManager tests...");
    test_module_manager_group();
    
    ESP_LOGI(TAG, "Running SharedState tests...");
    test_shared_state_group();
    
    ESP_LOGI(TAG, "Running JsonValidator tests...");
    test_json_validator_group();
    
    ESP_LOGI(TAG, "Running Logger tests...");
    test_logger_group();
    
    // End Unity tests
    UNITY_END();
    
    ESP_LOGI(TAG, "All tests completed!");
}

/**
 * @brief Run specific test group
 */
extern "C" void run_test_group(const char* group_name) {
    ESP_LOGI(TAG, "Running test group: %s", group_name);
    
    UNITY_BEGIN();
    
    if (strcmp(group_name, "eventbus") == 0) {
        test_event_bus_group();
    } else if (strcmp(group_name, "config") == 0) {
        test_config_manager_group();
    } else if (strcmp(group_name, "module") == 0) {
        test_module_manager_group();
    } else if (strcmp(group_name, "state") == 0) {
        test_shared_state_group();
    } else if (strcmp(group_name, "json") == 0) {
        test_json_validator_group();
    } else if (strcmp(group_name, "logger") == 0) {
        test_logger_group();
    } else {
        ESP_LOGW(TAG, "Unknown test group: %s", group_name);
    }
    
    UNITY_END();
}
