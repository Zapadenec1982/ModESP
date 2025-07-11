/**
 * @file config_manager_async.cpp
 * @brief Реалізація асинхронного менеджера конфігурації
 */

#include "config_manager_async.h"
#include "config_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <algorithm>
#include <cstring>

static const char* TAG = "ConfigAsync";

namespace ConfigManagerAsync {

// Внутрішній стан
static struct {
    QueueHandle_t write_queue = nullptr;
    TaskHandle_t writer_task = nullptr;
    SemaphoreHandle_t stats_mutex = nullptr;
    AsyncStats stats = {};
    AsyncConfig config = {};
    bool running = false;
} state;

// Внутрішні функції
static void writer_task_func(void* param);static esp_err_t save_module_with_watchdog(const std::string& module_name, 
                                          const nlohmann::json& data);
static void update_stats(bool success, uint32_t write_time_ms, size_t bytes);

esp_err_t init_async(const AsyncConfig& config) {
    if (state.running) {
        ESP_LOGW(TAG, "Async manager already initialized");
        return ESP_OK;
    }
    
    state.config = config;
    
    // Створюємо чергу для запитів на збереження
    state.write_queue = xQueueCreate(config.write_queue_size, sizeof(SaveRequest));
    if (!state.write_queue) {
        ESP_LOGE(TAG, "Failed to create write queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Створюємо мютекс для статистики
    state.stats_mutex = xSemaphoreCreateMutex();
    if (!state.stats_mutex) {
        vQueueDelete(state.write_queue);
        ESP_LOGE(TAG, "Failed to create stats mutex");
        return ESP_ERR_NO_MEM;
    }    
    // Створюємо задачу для асинхронного запису
    BaseType_t ret = xTaskCreate(
        writer_task_func,
        "config_writer",
        config.writer_stack_size,
        nullptr,
        config.writer_priority,
        &state.writer_task
    );
    
    if (ret != pdPASS) {
        vQueueDelete(state.write_queue);
        vSemaphoreDelete(state.stats_mutex);
        ESP_LOGE(TAG, "Failed to create writer task");
        return ESP_FAIL;
    }
    
    state.running = true;
    ESP_LOGI(TAG, "Async config manager initialized");
    return ESP_OK;
}

esp_err_t schedule_save(const std::string& module_name) {
    if (!state.running) {
        return ESP_ERR_INVALID_STATE;
    }    
    SaveRequest request = {
        .operation = SaveOperation::SAVE_MODULE,
        .module_name = module_name,
        .data = ConfigManager::get(module_name),
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000)
    };
    
    if (xQueueSend(state.write_queue, &request, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "Write queue full, dropping save request for %s", 
                 module_name.c_str());
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t schedule_save_all() {
    if (!state.running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    SaveRequest request = {
        .operation = SaveOperation::SAVE_ALL,
        .module_name = "",
        .data = ConfigManager::get_all(),
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000)
    };    
    if (xQueueSend(state.write_queue, &request, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "Write queue full, dropping save all request");
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

static void writer_task_func(void* param) {
    ESP_LOGI(TAG, "Config writer task started");
    SaveRequest request;
    std::vector<SaveRequest> batch;
    
    while (state.running) {
        // Чекаємо на запит або таймаут для групування
        if (xQueueReceive(state.write_queue, &request, 
                          pdMS_TO_TICKS(state.config.batch_delay_ms)) == pdTRUE) {
            batch.push_back(request);
            
            // Збираємо всі доступні запити в batch
            while (xQueueReceive(state.write_queue, &request, 0) == pdTRUE) {
                batch.push_back(request);
                if (batch.size() >= 10) break; // Обмежуємо розмір batch
            }
        }        
        // Обробляємо batch якщо є запити або пройшов таймаут
        if (!batch.empty()) {
            ESP_LOGD(TAG, "Processing batch of %d save requests", batch.size());
            
            uint64_t start_time = esp_timer_get_time();
            
            // Групуємо запити по модулях
            std::map<std::string, nlohmann::json> modules_to_save;
            bool save_all = false;
            
            for (const auto& req : batch) {
                if (req.operation == SaveOperation::SAVE_ALL) {
                    save_all = true;
                    break;
                } else if (req.operation == SaveOperation::SAVE_MODULE) {
                    modules_to_save[req.module_name] = req.data;
                }
            }
            
            // Виконуємо збереження
            bool success = true;
            size_t total_bytes = 0;
            
            if (save_all) {
                // Зберігаємо всю конфігурацію
                auto all_config = ConfigManager::get_all();                for (auto it = all_config.begin(); it != all_config.end(); ++it) {
                    if (it.value().is_object()) {
                        esp_err_t ret = save_module_with_watchdog(it.key(), it.value());
                        if (ret != ESP_OK) {
                            success = false;
                        }
                    }
                }
            } else {
                // Зберігаємо окремі модулі
                for (const auto& [module, data] : modules_to_save) {
                    esp_err_t ret = save_module_with_watchdog(module, data);
                    if (ret != ESP_OK) {
                        success = false;
                    }
                }
            }
            
            uint32_t write_time_ms = (esp_timer_get_time() - start_time) / 1000;
            update_stats(success, write_time_ms, total_bytes);
            
            batch.clear();
        }
        
        // Годуємо watchdog
        esp_task_wdt_reset();
    }    
    ESP_LOGI(TAG, "Config writer task stopped");
    vTaskDelete(NULL);
}

static esp_err_t save_module_with_watchdog(const std::string& module_name, 
                                          const nlohmann::json& data) {
    ESP_LOGD(TAG, "Saving module %s", module_name.c_str());
    
    // Серіалізуємо JSON порціями
    std::string json_str = data.dump(-1, ' ', false);
    
    // Формуємо шлях до файлу
    char filepath[128];
    snprintf(filepath, sizeof(filepath), "/storage/configs/%s.json", module_name.c_str());
    
    // Відкриваємо файл для запису
    FILE* file = fopen(filepath, "w");
    if (!file) {
        ESP_LOGE(TAG, "Failed to open %s for writing", filepath);
        return ESP_FAIL;
    }
    
    // Записуємо дані порціями з годуванням watchdog
    const size_t chunk_size = 1024; // 1KB chunks
    size_t written = 0;
    size_t total_size = json_str.length();    
    while (written < total_size) {
        size_t to_write = std::min(chunk_size, total_size - written);
        size_t result = fwrite(json_str.c_str() + written, 1, to_write, file);
        
        if (result != to_write) {
            ESP_LOGE(TAG, "Failed to write data to %s", filepath);
            fclose(file);
            return ESP_FAIL;
        }
        
        written += result;
        
        // Годуємо watchdog кожні N мілісекунд
        static TickType_t last_feed = 0;
        TickType_t now = xTaskGetTickCount();
        if (now - last_feed > pdMS_TO_TICKS(state.config.watchdog_feed_interval)) {
            esp_task_wdt_reset();
            last_feed = now;
        }
    }
    
    fclose(file);
    ESP_LOGD(TAG, "Saved %s (%zu bytes)", module_name.c_str(), written);
    return ESP_OK;
}

static void update_stats(bool success, uint32_t write_time_ms, size_t bytes) {    if (xSemaphoreTake(state.stats_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (success) {
            state.stats.completed_saves++;
        } else {
            state.stats.failed_saves++;
        }
        state.stats.total_write_time_ms += write_time_ms;
        state.stats.max_write_time_ms = std::max(state.stats.max_write_time_ms, write_time_ms);
        state.stats.total_bytes_written += bytes;
        state.stats.pending_saves = uxQueueMessagesWaiting(state.write_queue);
        xSemaphoreGive(state.stats_mutex);
    }
}

esp_err_t flush_pending_saves(uint32_t timeout_ms) {
    if (!state.running) {
        return ESP_ERR_INVALID_STATE;
    }
    
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    
    while (uxQueueMessagesWaiting(state.write_queue) > 0) {
        if (xTaskGetTickCount() - start > timeout) {
            ESP_LOGW(TAG, "Flush timeout, %d requests still pending", 
                     uxQueueMessagesWaiting(state.write_queue));
            return ESP_ERR_TIMEOUT;
        }        vTaskDelay(pdMS_TO_TICKS(10));
        esp_task_wdt_reset();
    }
    
    return ESP_OK;
}

AsyncStats get_async_stats() {
    AsyncStats stats = {};
    if (xSemaphoreTake(state.stats_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        stats = state.stats;
        xSemaphoreGive(state.stats_mutex);
    }
    return stats;
}

esp_err_t stop_async() {
    if (!state.running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping async config manager");
    state.running = false;
    
    // Чекаємо завершення всіх операцій
    flush_pending_saves(5000);
    
    // Видаляємо задачу
    if (state.writer_task) {
        vTaskDelete(state.writer_task);
        state.writer_task = nullptr;
    }    
    // Очищаємо ресурси
    if (state.write_queue) {
        vQueueDelete(state.write_queue);
        state.write_queue = nullptr;
    }
    
    if (state.stats_mutex) {
        vSemaphoreDelete(state.stats_mutex);
        state.stats_mutex = nullptr;
    }
    
    ESP_LOGI(TAG, "Async config manager stopped");
    return ESP_OK;
}

} // namespace ConfigManagerAsync
