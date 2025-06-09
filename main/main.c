#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Core includes
#include "application.h"

static const char* TAG = "Main";

void app_main(void) {
    ESP_LOGI(TAG, "ModuChill starting...");
    
    // Initialize application
    esp_err_t ret = Application::init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize application: %s", esp_err_to_name(ret));
        return;
    }
    
    // Run main loop (this function never returns)
    Application::run();
}