/**
 * @file main.cpp
 * @brief Integration Test Suite for ModESP Core System
 * 
 * Tests the entire ModESP system as a whole - not individual components,
 * but their integration and real-world behavior.
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "unity.h"

// Test modules
extern "C" {
    void run_system_lifecycle_tests(void);
    void run_system_stress_tests(void);
    void run_error_scenario_tests(void);
    void run_multicore_tests(void);
    void run_real_hardware_tests(void);
}

static const char* TAG = "IntegrationTest";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== ModESP Core Integration Test Suite ===");
    ESP_LOGI(TAG, "Testing entire system integration, not individual components");
    ESP_LOGI(TAG, "Free heap at start: %lu bytes", esp_get_free_heap_size());
    
    // Wait for system to stabilize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    UNITY_BEGIN();
    
    ESP_LOGI(TAG, "🧪 Running System Lifecycle Tests...");
    run_system_lifecycle_tests();
    
    ESP_LOGI(TAG, "🔥 Running System Stress Tests...");
    run_system_stress_tests();
    
    ESP_LOGI(TAG, "⚠️  Running Error Scenario Tests...");
    run_error_scenario_tests();
    
    ESP_LOGI(TAG, "🔄 Running Multicore Tests...");
    run_multicore_tests();
    
    ESP_LOGI(TAG, "🔌 Running Real Hardware Tests...");
    run_real_hardware_tests();
    
    UNITY_END();
    
    ESP_LOGI(TAG, "=== Integration Test Suite Complete ===");
    ESP_LOGI(TAG, "Free heap at end: %lu bytes", esp_get_free_heap_size());
    
    // Keep running to observe system behavior
    while (1) {
        ESP_LOGI(TAG, "System running normally. Free heap: %lu bytes", 
                 esp_get_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
} 