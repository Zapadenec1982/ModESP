/**
 * @file config_manager_integration.cpp
 * @brief Integration example for async ConfigManager
 */

// Додати в config_manager.cpp:

// В початок файлу
#include "config_manager_async.h"

// Додати статичну змінну
static bool async_save_enabled = false;

// В функцію init()
esp_err_t init() {
    // ... існуючий код ...
    
    #ifdef CONFIG_USE_ASYNC_SAVE
    // Ініціалізувати асинхронний менеджер
    ConfigManagerAsync::AsyncConfig async_config;
    async_config.write_queue_size = 20;
    async_config.batch_delay_ms = 200;
    async_config.watchdog_feed_interval = 30;
    
    if (ConfigManagerAsync::init_async(async_config) == ESP_OK) {
        async_save_enabled = true;
        ESP_LOGI(TAG, "Async save enabled");
    } else {
        ESP_LOGW(TAG, "Failed to init async save, using sync mode");
    }    #endif
    
    return ESP_OK;
}

// Модифікувати функцію save()
esp_err_t save() {
    if (!dirty_flag) {
        ESP_LOGD(TAG, "No changes to save");
        return ESP_OK;
    }
    
    #ifdef CONFIG_USE_ASYNC_SAVE
    if (async_save_enabled) {
        // Використовувати асинхронне збереження
        ESP_LOGI(TAG, "Scheduling async save of all modules");
        return ConfigManagerAsync::schedule_save_all();
    }
    #endif
    
    // Існуючий синхронний код
    ESP_LOGI(TAG, "Saving configuration (sync mode)...");
    // ... існуючий код збереження ...
}

// Модифікувати функцію set()
esp_err_t set(const std::string& path, const nlohmann::json& value) {
    // ... існуючий код ...    
    // Повідомляємо про зміну
    if (old_value != value) {
        notify_change(path, old_value, value);
        
        #ifdef CONFIG_USE_ASYNC_SAVE
        if (async_save_enabled && CONFIG_AUTO_SAVE_ENABLED) {
            // Автоматично планувати збереження змін
            std::string module_name = path.substr(0, path.find('.'));
            ConfigManagerAsync::schedule_save(module_name);
        }
        #endif
    }
    
    return ESP_OK;
}

// Додати нову функцію для примусового збереження
esp_err_t force_save_sync() {
    #ifdef CONFIG_USE_ASYNC_SAVE
    if (async_save_enabled) {
        // Зачекати завершення всіх асинхронних операцій
        ESP_LOGI(TAG, "Flushing pending async saves...");
        esp_err_t ret = ConfigManagerAsync::flush_pending_saves(5000);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Async flush timeout, forcing sync save");
        } else {
            return ESP_OK;
        }
    }    #endif
    
    // Виконати синхронне збереження
    return save_sync_internal();
}

// В функцію deinit()
esp_err_t deinit() {
    ESP_LOGI(TAG, "Deinitializing ConfigManager...");
    
    #ifdef CONFIG_USE_ASYNC_SAVE
    if (async_save_enabled) {
        // Зберегти всі незбережені зміни
        ConfigManagerAsync::flush_pending_saves(5000);
        ConfigManagerAsync::stop_async();
        async_save_enabled = false;
    }
    #endif
    
    // ... існуючий код ...
}

// Додати в Kconfig.projbuild:
/*
config USE_ASYNC_SAVE
    bool "Enable asynchronous configuration save"
    default y
    help
        Use async task for saving configuration to avoid watchdog issues

config AUTO_SAVE_ENABLED  
    bool "Auto-save configuration changes"
    depends on USE_ASYNC_SAVE
    default n
    help
        Automatically save configuration changes without explicit save() call
*/
