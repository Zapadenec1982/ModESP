/**
 * @file application_config_usage.cpp
 * @brief Правильне використання ConfigManager в Application
 */

// В application.cpp:

esp_err_t Application::init() {
    ESP_LOGI(TAG, "=== PHASE 1: System Initialization ===");
    
    // 1. Базова ініціалізація
    EventBus::init();
    SharedState::init();
    ModuleManager::init();
    
    // 2. СИНХРОННЕ завантаження конфігурації - БЛОКУЄ!
    ESP_LOGI(TAG, "Loading configuration (blocking)...");
    uint64_t config_start = esp_timer_get_time();
    
    ConfigManager::init();
    esp_err_t config_result = ConfigManager::load();  // <-- БЛОКУЮЧА ОПЕРАЦІЯ!
    
    uint64_t config_time = esp_timer_get_time() - config_start;
    ESP_LOGI(TAG, "Configuration loaded in %lld ms", config_time / 1000);
    
    if (config_result != ESP_OK) {
        ESP_LOGW(TAG, "No saved config, using defaults");
        // Зберегти defaults для наступного разу
        ConfigManager::save_sync();  // Також блокуюча
    }
    
    // 3. Тепер можна конфігурувати модулі - у них є конфігурація!
    ESP_LOGI(TAG, "=== PHASE 2: Module Configuration ===");
    nlohmann::json config = ConfigManager::get_all();
    ModuleManager::configure_all(config);
    
    // 4. Ініціалізація модулів
    ESP_LOGI(TAG, "=== PHASE 3: Module Initialization ===");
    if (ModuleManager::init_all() != ESP_OK) {
        ESP_LOGE(TAG, "Critical module failed!");
        return ESP_FAIL;
    }
    
    // 5. ТЕПЕР включаємо асинхронне збереження для runtime
    ESP_LOGI(TAG, "=== PHASE 4: Enable Runtime Features ===");
    ConfigManager::enable_async_save();
    ConfigManager::enable_auto_save(true);  // Опційно
    
    ESP_LOGI(TAG, "System initialization complete!");
    m_initialized = true;
    return ESP_OK;
}

void Application::run() {
    ESP_LOGI(TAG, "Application main loop started");
    
    while (m_running) {
        uint32_t loop_start = esp_timer_get_time() / 1000;
        
        // Обробка подій
        EventBus::process(5);
        
        // Оновлення модулів
        ModuleManager::tick_all(20);
        
        // Приклад зміни конфігурації під час роботи
        static uint32_t last_config_update = 0;
        if (loop_start - last_config_update > 60000) {  // Раз на хвилину
            // Ця операція НЕ блокує!
            ConfigManager::set("system.uptime_minutes", loop_start / 60000);
            // Автоматично заплановано асинхронне збереження
            last_config_update = loop_start;
        }
        
        // Перевірка статусу збереження
        auto save_status = ConfigManager::get_save_status();
        if (save_status.pending_async_saves > 10) {
            ESP_LOGW(TAG, "Many pending saves: %d", save_status.pending_async_saves);
        }
        
        // Контроль циклу
        uint32_t loop_time = (esp_timer_get_time() / 1000) - loop_start;
        if (loop_time < MAIN_LOOP_PERIOD_MS) {
            vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_PERIOD_MS - loop_time));
        }
    }
}

void Application::stop() {
    ESP_LOGI(TAG, "=== System Shutdown ===");
    m_running = false;
    
    // 1. Зупинити модулі
    ESP_LOGI(TAG, "Stopping modules...");
    ModuleManager::shutdown_all();
    
    // 2. СИНХРОННЕ збереження всіх змін - БЛОКУЄ!
    ESP_LOGI(TAG, "Saving configuration (blocking)...");
    uint64_t save_start = esp_timer_get_time();
    
    ConfigManager::force_save_sync();  // <-- БЛОКУЮЧА ОПЕРАЦІЯ!
    
    uint64_t save_time = esp_timer_get_time() - save_start;
    ESP_LOGI(TAG, "Configuration saved in %lld ms", save_time / 1000);
    
    // 3. Очищення підсистем
    ConfigManager::deinit();
    EventBus::deinit();
    SharedState::deinit();
    
    ESP_LOGI(TAG, "Application stopped");
}

// Приклад обробки критичних змін
void Application::handle_critical_config_change(const std::string& key, 
                                               const nlohmann::json& value) {
    // Для критичних налаштувань - синхронне збереження
    ConfigManager::set(key, value);
    
    if (key.find("safety") != std::string::npos || 
        key.find("critical") != std::string::npos) {
        ESP_LOGI(TAG, "Critical config change, forcing sync save");
        ConfigManager::force_save_sync();  // Блокує, але це ОК для критичних змін
    }
    // Інакше - асинхронне збереження відбудеться автоматично
}
