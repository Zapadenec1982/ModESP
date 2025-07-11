/**
 * @file config_manager_async.h
 * @brief Асинхронний менеджер конфігурації з підтримкою watchdog
 */

#ifndef CONFIG_MANAGER_ASYNC_H
#define CONFIG_MANAGER_ASYNC_H

#include <string>
#include <queue>
#include <mutex>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nlohmann/json.hpp"

namespace ConfigManagerAsync {

/**
 * @brief Тип операції збереження
 */
enum class SaveOperation {
    SAVE_MODULE,      // Зберегти конкретний модуль
    SAVE_ALL,         // Зберегти всю конфігурацію
    SAVE_INCREMENTAL  // Зберегти тільки зміни
};

/**
 * @brief Запит на збереження
 */
struct SaveRequest {
    SaveOperation operation;
    std::string module_name;
    nlohmann::json data;
    uint32_t timestamp;
};

/**
 * @brief Налаштування асинхронного менеджера
 */
struct AsyncConfig {
    size_t write_queue_size = 10;        // Розмір черги запису
    size_t writer_stack_size = 4096;     // Розмір стеку задачі
    uint8_t writer_priority = 5;         // Пріоритет задачі
    uint32_t batch_delay_ms = 100;       // Затримка для групування змін
    uint32_t watchdog_feed_interval = 50; // Інтервал годування watchdog (ms)
    bool enable_compression = false;      // Стиснення JSON перед записом
};

/**
 * @brief Ініціалізація асинхронного менеджера
 */
esp_err_t init_async(const AsyncConfig& config = {});

/**
 * @brief Запланувати збереження модуля
 * 
 * Неблокуюча операція - додає в чергу для асинхронного запису
 */
esp_err_t schedule_save(const std::string& module_name);

/**
 * @brief Запланувати збереження всієї конфігурації
 */
esp_err_t schedule_save_all();

/**
 * @brief Примусове збереження з очікуванням
 * 
 * Блокує виконання до завершення всіх операцій запису
 */
esp_err_t flush_pending_saves(uint32_t timeout_ms = 5000);

/**
 * @brief Отримати статистику асинхронних операцій
 */
struct AsyncStats {
    uint32_t pending_saves;
    uint32_t completed_saves;
    uint32_t failed_saves;
    uint32_t total_write_time_ms;
    uint32_t max_write_time_ms;
    size_t total_bytes_written;
};

AsyncStats get_async_stats();

/**
 * @brief Зупинити асинхронний менеджер
 */
esp_err_t stop_async();

} // namespace ConfigManagerAsync

#endif // CONFIG_MANAGER_ASYNC_H
