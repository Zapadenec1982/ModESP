#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "Main";

// Оголошуємо зовнішню функцію, яка визначена у C++ коді
void launch_application();

void app_main(void) {
    ESP_LOGI(TAG, "ModuChill starting...");
    
    // Запускаємо нашу C++ Application
    launch_application();
    
    ESP_LOGI(TAG, "C++ application launched. Main task can now exit or do other things.");
    // Основна задача app_main може завершитися, оскільки
    // наш додаток тепер працює у власній задачі (app_task).
}