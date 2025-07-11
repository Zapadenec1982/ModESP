/**
 * @file config_manager_lifecycle.cpp
 * @brief Оновлена реалізація з правильним життєвим циклом
 */

// Модифікації для config_manager.cpp:

// === 1. ЗАВАНТАЖЕННЯ - завжди СИНХРОННЕ ===

esp_err_t load() {
    using namespace ModESP;
    
    ESP_LOGI(TAG, "Loading configuration (SYNC)...");
    
    // Завантажуємо конфігурацію СИНХРОННО
    // Це критичний момент - система має чекати!
    
    uint64_t start_time = esp_timer_get_time();
    
    nlohmann::json loaded_config = nlohmann::json::object();
    bool any_section_loaded = false;
    
    // Завантажуємо версію
    char version_path[128];
    snprintf(version_path, sizeof(version_path), "%s/version.dat", CONFIG_DIR);
    
    FILE* version_file = fopen(version_path, "rb");
    if (version_file) {
        uint32_t saved_version;
        if (fread(&saved_version, sizeof(saved_version), 1, version_file) == 1) {
            loaded_config["version"] = saved_version;
            ESP_LOGI(TAG, "Config version: %lu", saved_version);
        }
        fclose(version_file);
    }
    
    // Завантажуємо кожен модуль СИНХРОННО
    size_t total_loaded = 0;
    for (size_t i = 0; i < CONFIG_MODULES_COUNT; i++) {
        const ConfigModule& module = config_modules[i];
        
        char module_path[128];
        snprintf(module_path, sizeof(module_path), "%s/%s.json", CONFIG_DIR, module.name);
        
        FILE* module_file = fopen(module_path, "r");
        if (module_file) {
            // Отримуємо розмір файлу
            fseek(module_file, 0, SEEK_END);
            long file_size = ftell(module_file);
            rewind(module_file);
            
            if (file_size > 0) {
                // Читаємо файл з періодичним годуванням watchdog
                std::string buffer(file_size, '\0');
                size_t read = 0;
                const size_t chunk_size = 4096; // 4KB chunks
                
                while (read < file_size) {
                    size_t to_read = std::min(chunk_size, file_size - read);
                    size_t result = fread(&buffer[read], 1, to_read, module_file);
                    if (result == 0) break;
                    
                    read += result;
                    
                    // Годуємо watchdog кожні 4KB
                    esp_task_wdt_reset();
                }
                
                fclose(module_file);
                
                // Парсимо JSON
                try {
                    loaded_config[module.name] = nlohmann::json::parse(buffer);
                    any_section_loaded = true;
                    total_loaded += read;
                    ESP_LOGD(TAG, "Loaded %s (%zu bytes)", module.name, read);
                } catch (const std::exception& e) {
                    ESP_LOGE(TAG, "Failed to parse %s: %s", module.name, e.what());
                }
            } else {
                fclose(module_file);
            }
        }
    }
    
    uint64_t load_time = esp_timer_get_time() - start_time;
    ESP_LOGI(TAG, "Configuration loaded in %lld ms (%zu bytes)", 
             load_time / 1000, total_loaded);
    
    if (any_section_loaded) {
        // Використовуємо завантажену конфігурацію
        config_cache = loaded_config;
        last_saved_config = config_cache;
        dirty_flag = false;
        return ESP_OK;
    } else {
        // Використовуємо defaults
        ESP_LOGW(TAG, "No saved config found, using embedded defaults");
        config_cache = aggregate_embedded_configs();
        last_saved_config = config_cache;
        dirty_flag = true; // Потрібно зберегти
        return ESP_ERR_NOT_FOUND;
    }
}
// === 2. ЗБЕРЕЖЕННЯ - може бути АСИНХРОННИМ під час роботи ===

static bool auto_save_enabled = false;
static bool async_save_available = false;

esp_err_t save() {
    if (!dirty_flag) {
        ESP_LOGD(TAG, "No changes to save");
        return ESP_OK;
    }
    
    #ifdef CONFIG_USE_ASYNC_SAVE
    if (async_save_available) {
        // Під час роботи системи - використовуємо асинхронне збереження
        ESP_LOGI(TAG, "Scheduling async save...");
        return ConfigManagerAsync::schedule_save_all();
    }
    #endif
    
    // Fallback на синхронне збереження
    return save_sync();
}

esp_err_t save_sync() {
    ESP_LOGI(TAG, "Saving configuration (SYNC)...");
    
    uint64_t start_time = esp_timer_get_time();
    size_t total_saved = 0;
    bool all_saved = true;
    
    // Створюємо директорію якщо потрібно
    struct stat st;
    if (stat(CONFIG_DIR, &st) != 0) {
        mkdir(CONFIG_DIR, 0755);
    }
    
    // Зберігаємо версію
    char version_path[128];
    snprintf(version_path, sizeof(version_path), "%s/version.dat", CONFIG_DIR);
    
    FILE* version_file = fopen(version_path, "wb");
    if (version_file) {
        uint32_t version = config_cache.value("version", config_version);
        fwrite(&version, sizeof(version), 1, version_file);
        fclose(version_file);
    }
    
    // Зберігаємо кожен модуль з watchdog контролем
    for (size_t i = 0; i < CONFIG_MODULES_COUNT; i++) {
        const ConfigModule& module = config_modules[i];
        
        if (config_cache.contains(module.name)) {
            std::string module_json = config_cache[module.name].dump(-1, ' ', false);
            
            char module_path[128];
            snprintf(module_path, sizeof(module_path), "%s/%s.json", CONFIG_DIR, module.name);
            
            FILE* file = fopen(module_path, "w");
            if (file) {
                // Записуємо chunks з годуванням watchdog
                size_t written = 0;
                const size_t chunk_size = 4096;
                
                while (written < module_json.length()) {
                    size_t to_write = std::min(chunk_size, module_json.length() - written);
                    size_t result = fwrite(module_json.c_str() + written, 1, to_write, file);
                    
                    if (result != to_write) {
                        all_saved = false;
                        break;
                    }
                    
                    written += result;
                    total_saved += result;
                    
                    // Годуємо watchdog кожні 4KB
                    esp_task_wdt_reset();
                }
                
                fclose(file);
                ESP_LOGD(TAG, "Saved %s (%zu bytes)", module.name, written);
            }
        }
    }
    
    uint64_t save_time = esp_timer_get_time() - start_time;
    
    if (all_saved) {
        ESP_LOGI(TAG, "Config saved in %lld ms (%zu bytes)", 
                 save_time / 1000, total_saved);
        last_saved_config = config_cache;
        dirty_flag = false;
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to save some modules");
        return ESP_FAIL;
    }
}

// === 3. ПРАВИЛЬНА ІНІЦІАЛІЗАЦІЯ ===

esp_err_t init() {
    ESP_LOGI(TAG, "Initializing ConfigManager...");
    
    // ... монтування LittleFS ...
    
    // НЕ включаємо асинхронне збереження одразу!
    // Воно буде включено ПІСЛЯ завантаження модулів
    async_save_available = false;
    
    return ESP_OK;
}
// === 4. УПРАВЛІННЯ АСИНХРОННИМ ЗБЕРЕЖЕННЯМ ===

esp_err_t enable_async_save() {
    #ifdef CONFIG_USE_ASYNC_SAVE
    if (!async_save_available) {
        ConfigManagerAsync::AsyncConfig config;
        config.write_queue_size = CONFIG_ASYNC_SAVE_QUEUE_SIZE;
        config.batch_delay_ms = CONFIG_ASYNC_SAVE_BATCH_DELAY_MS;
        config.watchdog_feed_interval = CONFIG_ASYNC_SAVE_WATCHDOG_FEED_MS;
        
        esp_err_t ret = ConfigManagerAsync::init_async(config);
        if (ret == ESP_OK) {
            async_save_available = true;
            ESP_LOGI(TAG, "Async save enabled for runtime operations");
        } else {
            ESP_LOGW(TAG, "Failed to enable async save");
        }
        return ret;
    }
    #endif
    return ESP_OK;
}

void enable_auto_save(bool enable) {
    auto_save_enabled = enable && async_save_available;
    ESP_LOGI(TAG, "Auto-save %s", auto_save_enabled ? "enabled" : "disabled");
}

// Модифікуємо set() для автоматичного збереження
esp_err_t set(const std::string& path, const nlohmann::json& value) {
    // ... існуючий код встановлення значення ...
    
    if (old_value != value) {
        notify_change(path, old_value, value);
        
        #ifdef CONFIG_USE_ASYNC_SAVE
        if (auto_save_enabled && async_save_available) {
            // Планувати асинхронне збереження тільки змінених модулів
            std::string module_name = path.substr(0, path.find('.'));
            ConfigManagerAsync::schedule_save(module_name);
        }
        #endif
    }
    
    return ESP_OK;
}

// === 5. SHUTDOWN - знову СИНХРОННИЙ ===

esp_err_t deinit() {
    ESP_LOGI(TAG, "Deinitializing ConfigManager...");
    
    #ifdef CONFIG_USE_ASYNC_SAVE
    if (async_save_available) {
        // Чекаємо завершення всіх асинхронних операцій
        ESP_LOGI(TAG, "Flushing async saves...");
        ConfigManagerAsync::flush_pending_saves(5000);
        ConfigManagerAsync::stop_async();
        async_save_available = false;
    }
    #endif
    
    // Синхронне збереження останніх змін
    if (dirty_flag) {
        ESP_LOGI(TAG, "Saving final changes...");
        save_sync(); // Блокуюче!
    }
    
    // Очищення ресурсів
    change_callbacks.clear();
    config_cache.clear();
    last_saved_config.clear();
    
    // Unmount LittleFS
    esp_vfs_littlefs_unregister("storage");
    
    ESP_LOGI(TAG, "ConfigManager deinitialized");
    return ESP_OK;
}

// === 6. НОВІ API ФУНКЦІЇ ===

// Примусове синхронне збереження (для критичних моментів)
esp_err_t force_save_sync() {
    ESP_LOGI(TAG, "Force sync save requested");
    
    #ifdef CONFIG_USE_ASYNC_SAVE
    if (async_save_available) {
        // Спочатку flush асинхронні операції
        ConfigManagerAsync::flush_pending_saves(3000);
    }
    #endif
    
    return save_sync();
}

// Отримати статус збереження
struct SaveStatus {
    bool has_unsaved_changes;
    uint32_t pending_async_saves;
    uint64_t last_save_timestamp;
    size_t config_size_bytes;
};

SaveStatus get_save_status() {
    SaveStatus status = {
        .has_unsaved_changes = dirty_flag,
        .pending_async_saves = 0,
        .last_save_timestamp = 0, // TODO: track timestamp
        .config_size_bytes = config_cache.dump().length()
    };
    
    #ifdef CONFIG_USE_ASYNC_SAVE
    if (async_save_available) {
        auto async_stats = ConfigManagerAsync::get_async_stats();
        status.pending_async_saves = async_stats.pending_saves;
    }
    #endif
    
    return status;
}
