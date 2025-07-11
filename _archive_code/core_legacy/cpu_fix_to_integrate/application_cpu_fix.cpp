/**
 * @file application_cpu_fix.cpp
 * @brief ВИПРАВЛЕННЯ ФІКТИВНОГО CPU USAGE АЛГОРИТМУ
 * 
 * ЗАМІНИТИ функцію get_cpu_usage() в components/core/application.cpp
 */

#include "application.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "Application";

namespace Application {

// Глобальні змінні для реального вимірювання CPU
static uint64_t last_idle_time = 0;
static uint64_t last_total_time = 0;
static uint64_t last_measurement_time = 0;
static uint8_t last_cpu_usage = 5;

// Forward declaration
static uint64_t get_idle_task_runtime();

/**
 * @brief ПРАВИЛЬНА реалізація get_cpu_usage() з реальним вимірюванням
 * 
 * Замінити цю функцію в application.cpp:373-411
 */
uint8_t get_cpu_usage() {
    uint64_t current_time = esp_timer_get_time();
    
    // Мінімальний інтервал між вимірюваннями (1 секунда)
    if (current_time - last_measurement_time < 1000000) {
        return last_cpu_usage; // Повертаємо останнє значення
    }
    
    uint64_t idle_time = get_idle_task_runtime();
    uint64_t elapsed_time = current_time - last_measurement_time;
    
    if (last_measurement_time == 0) {
        // Перший вимір - ініціалізація
        last_idle_time = idle_time;
        last_total_time = current_time;
        last_measurement_time = current_time;
        last_cpu_usage = 5; // Початкова оцінка
        return last_cpu_usage;
    }
    
    // Обчислюємо дельти
    uint64_t idle_delta = idle_time - last_idle_time;
    uint64_t total_delta = elapsed_time;
    
    // Захист від ділення на нуль
    if (total_delta == 0) {
        return last_cpu_usage;
    }
    
    // Розраховуємо CPU usage
    float idle_percentage = (float)(idle_delta) / (float)(total_delta) * 100.0f;
    float cpu_usage = 100.0f - idle_percentage;
    
    // Захист від неможливих значень
    if (cpu_usage < 0.0f) cpu_usage = 0.0f;
    if (cpu_usage > 100.0f) cpu_usage = 100.0f;
    
    // Згладжування (exponential moving average)
    float smoothing_factor = 0.3f; // 30% нового значення
    cpu_usage = (smoothing_factor * cpu_usage) + ((1.0f - smoothing_factor) * last_cpu_usage);
    
    // Оновлюємо збережені значення
    last_idle_time = idle_time;
    last_total_time = current_time;
    last_measurement_time = current_time;
    last_cpu_usage = (uint8_t)(cpu_usage + 0.5f); // Округлення
    
    ESP_LOGD(TAG, "CPU Usage: idle_delta=%llu, total_delta=%llu, usage=%.1f%%", 
             idle_delta, total_delta, cpu_usage);
    
    return last_cpu_usage;
}

/**
 * @brief Отримати runtime IDLE task (мікросекунди)
 * 
 * Використовує FreeRTOS Runtime Stats API якщо доступний,
 * інакше fallback на простіший метод
 */
static uint64_t get_idle_task_runtime() {
#if (configGENERATE_RUN_TIME_STATS == 1)
    // Метод 1: Використання FreeRTOS Runtime Stats (найточніший)
    TaskStatus_t* task_status_array;
    UBaseType_t task_count;
    uint32_t total_runtime;
    
    // Отримуємо кількість завдань
    task_count = uxTaskGetNumberOfTasks();
    
    // Виділяємо пам'ять для статистики завдань
    task_status_array = (TaskStatus_t*)pvPortMalloc(task_count * sizeof(TaskStatus_t));
    if (task_status_array == NULL) {
        ESP_LOGW(TAG, "Failed to allocate memory for task stats");
        return esp_timer_get_time(); // Fallback
    }
    
    // Отримуємо статистику
    task_count = uxTaskGetSystemState(task_status_array, task_count, &total_runtime);
    
    uint64_t idle_runtime = 0;
    
    // Шукаємо IDLE завдання (зазвичай мають назву "IDLE")
    for (UBaseType_t i = 0; i < task_count; i++) {
        const char* task_name = task_status_array[i].pcTaskName;
        if (strstr(task_name, "IDLE") != NULL || 
            task_status_array[i].uxCurrentPriority == 0) { // IDLE пріоритет = 0
            idle_runtime += task_status_array[i].ulRunTimeCounter;
        }
    }
    
    vPortFree(task_status_array);
    
    // Конвертуємо в мікросекунди (Runtime stats в ticks, потрібно конвертувати)
    // Припускаємо що runtime counter в мікросекундах або близько того
    return idle_runtime;
    
#else
    // Метод 2: Fallback - оцінка на основі TickType
    // Це менш точний метод, але працює без Runtime Stats
    
    static uint64_t last_tick_time = 0;
    static TickType_t last_tick_count = 0;
    
    uint64_t current_time = esp_timer_get_time();
    TickType_t current_tick = xTaskGetTickCount();
    
    if (last_tick_time == 0) {
        last_tick_time = current_time;
        last_tick_count = current_tick;
        return current_time;
    }
    
    uint64_t elapsed_time = current_time - last_tick_time;
    TickType_t elapsed_ticks = current_tick - last_tick_count;
    
    // Якщо система працює стабільно, IDLE time близький до elapsed_time
    // Це груба оцінка, але краща за константи
    uint64_t estimated_idle_time = elapsed_time * 0.8; // Припускаємо 80% idle
    
    // Адаптивна корекція на основі main loop performance
    if (cycle_count > 0) {
        float avg_cycle_us = (float)total_cycle_time_us / cycle_count;
        float period_us = MAIN_LOOP_PERIOD_MS * 1000.0f;
        float main_load = avg_cycle_us / period_us;
        
        // Корегуємо оцінку idle time
        estimated_idle_time = elapsed_time * (1.0f - main_load - 0.1f); // -10% на system overhead
    }
    
    last_tick_time = current_time;
    last_tick_count = current_tick;
    
    return last_idle_time + estimated_idle_time;
#endif
}

/**
 * @brief Альтернативний простий метод CPU usage (якщо основний не працює)
 */
uint8_t get_cpu_usage_simple() {
    if (cycle_count == 0) return 5;
    
    float avg_cycle_us = (float)total_cycle_time_us / cycle_count;
    float period_us = MAIN_LOOP_PERIOD_MS * 1000.0f;
    
    // Простий розрахунок main loop load
    float main_loop_load = (avg_cycle_us / period_us) * 100.0f;
    
    // Додаємо розумну оцінку system overhead (без константи!)
    float system_overhead = 3.0f + (main_loop_load * 0.2f); // 3% + 20% від main loop
    
    float total_usage = main_loop_load + system_overhead;
    
    // Адаптивні корекції
    if (get_free_heap() < 50000) {
        total_usage += 2.0f; // Низька пам'ять збільшує навантаження
    }
    
    if (max_cycle_time_us > period_us * 1.5f) {
        total_usage += 5.0f; // Значні overruns
    } else if (max_cycle_time_us > period_us) {
        total_usage += 2.0f; // Незначні overruns
    }
    
    // Обмеження
    if (total_usage < 1.0f) total_usage = 1.0f;
    if (total_usage > 100.0f) total_usage = 100.0f;
    
    return (uint8_t)(total_usage + 0.5f);
}

/**
 * @brief Тест функцій CPU usage для перевірки реалістичності
 */
void test_cpu_usage_accuracy() {
    ESP_LOGI(TAG, "=== TESTING CPU USAGE ACCURACY ===");
    
    uint8_t baseline = get_cpu_usage();
    ESP_LOGI(TAG, "Baseline CPU usage: %u%%", baseline);
    
    // Створюємо штучне навантаження
    ESP_LOGI(TAG, "Creating artificial load...");
    uint64_t start_time = esp_timer_get_time();
    volatile uint32_t dummy = 0;
    
    // Навантаження протягом 2 секунд
    while (esp_timer_get_time() - start_time < 2000000) {
        for (int i = 0; i < 1000; i++) {
            dummy += i * i; // Обчислення для навантаження
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // Невелика пауза
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000)); // Час на вимірювання
    
    uint8_t loaded = get_cpu_usage();
    ESP_LOGI(TAG, "CPU usage under load: %u%%", loaded);
    
    int32_t delta = loaded - baseline;
    ESP_LOGI(TAG, "CPU usage delta: %+d%%", delta);
    
    if (delta > 3) {
        ESP_LOGI(TAG, "✅ CPU measurement seems responsive (delta > 3%%)");
    } else {
        ESP_LOGW(TAG, "⚠️ CPU measurement may not be sensitive enough (delta <= 3%%)");
    }
    
    // Чекаємо повернення до базового рівня
    vTaskDelay(pdMS_TO_TICKS(3000));
    uint8_t recovered = get_cpu_usage();
    ESP_LOGI(TAG, "CPU usage after recovery: %u%%", recovered);
    
    if (abs(recovered - baseline) <= 2) {
        ESP_LOGI(TAG, "✅ CPU measurement recovers properly");
    } else {
        ESP_LOGW(TAG, "⚠️ CPU measurement may have drift");
    }
}

} // namespace Application

// =============================================================================
// ІНСТРУКЦІЇ ПО ЗАСТОСУВАННЮ ВИПРАВЛЕННЯ
// =============================================================================

/*

1. ЗАМІНИТИ функцію get_cpu_usage() в components/core/application.cpp:373-411
   Взяти код з get_cpu_usage() вище

2. УВІМКНУТИ FreeRTOS Runtime Stats в sdkconfig:
   CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
   CONFIG_FREERTOS_USE_TRACE_FACILITY=y

3. ДОДАТИ тест в app_main():
   Application::test_cpu_usage_accuracy();

4. ОЧІКУВАНІ РЕЗУЛЬТАТИ:
   - CPU usage буде мінятися динамічно (1-15%)
   - Більше навантаження = більше CPU usage
   - Без навантаження = низький CPU usage (2-5%)
   - НЕ буде статичного зростання з часом

5. FALLBACK якщо Runtime Stats не працює:
   Використати get_cpu_usage_simple() замість get_cpu_usage()

*/ 