#include <stdio.h>
#include "unity.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "TestMain";

// Declare test runner function from C++
extern void run_all_tests(void);

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ModESP Core Component Tests");
    ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
    
    // Give system time to initialize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Run all tests
    run_all_tests();
    
    ESP_LOGI(TAG, "All tests completed");
    ESP_LOGI(TAG, "Free heap after tests: %d bytes", esp_get_free_heap_size());
    
    // Keep the task running
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
} 