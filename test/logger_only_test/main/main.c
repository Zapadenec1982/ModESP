#include <stdio.h>
#include <inttypes.h>
#include "unity.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char* TAG = "LoggerTest";

// Declare logger test function from C++
extern void run_logger_tests(void);

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ModESP Logger Tests");
    ESP_LOGI(TAG, "Free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
    
    // Initialize NVS (required for logger storage)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Give system time to initialize
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Run logger tests only
    run_logger_tests();
    
    ESP_LOGI(TAG, "Logger tests completed");
    ESP_LOGI(TAG, "Free heap after tests: %" PRIu32 " bytes", esp_get_free_heap_size());
    
    // Keep the task running
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
} 