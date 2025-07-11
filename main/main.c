#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "unity.h"

// Настройка для переключения между приложением и тестами
#define RUN_TESTS 0  // Установить 0 для запуска приложения, 1 для тестов
#define TEST_HYBRID_API 0  // Установить 1 для тестирования Hybrid API System

static const char* TAG = "Main";

// Объявляем внешнюю функцию из C++ кода  
void launch_application();

// Объявляем функции тестов из test_simple.cpp
void run_eventbus_tests(void);
void run_all_tests(void);

// Объявляем функции тестов TODO-006 Hybrid API
void test_hybrid_api_system(void);
void test_configuration_workflow(void);

void app_main(void) {
    ESP_LOGI(TAG, "ModuChill starting...");
    
#if RUN_TESTS
    // Запускаем Unity тесты
    ESP_LOGI(TAG, "Running Unity tests...");
    
    // Дожидаемся готовности системы
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Запускаем все тесты ядра
    run_all_tests();
    
    ESP_LOGI(TAG, "Unity tests completed!");
#elif TEST_HYBRID_API
    // Тестируем Hybrid API System (TODO-006)
    ESP_LOGI(TAG, "Testing Hybrid API System...");
    
    // Дожидаемся готовности системы
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Запускаем тесты Hybrid API
    test_hybrid_api_system();
    test_configuration_workflow();
    
    ESP_LOGI(TAG, "Hybrid API tests completed!");
#else
    // Запускаем обычное C++ приложение
    launch_application();
    
    ESP_LOGI(TAG, "C++ application launched. Main task can now exit or do other things.");
    // Основная задача app_main может завершиться, поскольку
    // наше приложение теперь работает в собственной задаче (app_task).
#endif
}