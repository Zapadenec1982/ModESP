/**
 * @file test_config_lifecycle.cpp
 * @brief Тести правильного життєвого циклу ConfigManager
 */

#include "unity.h"
#include "config_manager.h"
#include "config_manager_async.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "TestConfigLifecycle";

// Test correct startup sequence
void test_config_sync_load_at_startup() {
    // 1. Init повинен бути синхронним
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::init());
    
    // 2. Load МУСИТЬ бути синхронним і блокуючим
    uint64_t start = esp_timer_get_time();
    esp_err_t result = ConfigManager::load();
    uint64_t load_time = esp_timer_get_time() - start;
    
    // Перевіряємо що load дійсно зайняв час (не async)
    ESP_LOGI(TAG, "Config load took %lld us", load_time);
    TEST_ASSERT_GREATER_THAN(0, load_time);
    
    // 3. Після load конфігурація доступна одразу
    auto config = ConfigManager::get_all();
    TEST_ASSERT_FALSE(config.empty());
    
    // 4. Async save НЕ повинен бути включений автоматично
    auto status = ConfigManager::get_save_status();
    TEST_ASSERT_EQUAL(0, status.pending_async_saves);
}

// Test modules can access config after load
void test_config_available_for_modules() {
    // Симулюємо модуль, який потребує конфігурацію
    ConfigManager::set("test_module.enabled", true);
    ConfigManager::set("test_module.param1", 42);
    ConfigManager::save_sync(); // Синхронне збереження
    
    // Перезавантаження
    ConfigManager::init();
    ConfigManager::load(); // Блокуюче завантаження
    
    // Модуль може одразу отримати свою конфігурацію
    auto module_config = ConfigManager::get("test_module");
    TEST_ASSERT_TRUE(module_config["enabled"].get<bool>());
    TEST_ASSERT_EQUAL(42, module_config["param1"].get<int>());
}

// Test async save during runtime
void test_config_async_save_runtime() {
    // Спочатку система стартує синхронно
    ConfigManager::init();
    ConfigManager::load();
    
    // Потім включаємо async для runtime
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::enable_async_save());
    ConfigManager::enable_auto_save(true);
    
    // Тепер зміни не блокують
    uint64_t start = esp_timer_get_time();
    for (int i = 0; i < 10; i++) {
        ConfigManager::set("runtime.value" + std::to_string(i), i);
    }
    uint64_t set_time = esp_timer_get_time() - start;
    
    // set() повинен бути швидким (тільки RAM)
    ESP_LOGI(TAG, "10 sets took %lld us", set_time);
    TEST_ASSERT_LESS_THAN(1000, set_time); // < 1ms
    
    // Перевіряємо що saves заплановані
    auto status = ConfigManager::get_save_status();
    TEST_ASSERT_GREATER_THAN(0, status.pending_async_saves);
}

// Test sync save at shutdown
void test_config_sync_save_shutdown() {
    // Runtime зміни
    ConfigManager::set("shutdown.test", "final_value");
    
    // Shutdown - знову синхронний
    uint64_t start = esp_timer_get_time();
    esp_err_t result = ConfigManager::force_save_sync();
    uint64_t save_time = esp_timer_get_time() - start;
    
    TEST_ASSERT_EQUAL(ESP_OK, result);
    ESP_LOGI(TAG, "Sync save took %lld us", save_time);
    
    // Перевіряємо що всі зміни збережені
    auto status = ConfigManager::get_save_status();
    TEST_ASSERT_FALSE(status.has_unsaved_changes);
    TEST_ASSERT_EQUAL(0, status.pending_async_saves);
}

// Test critical config changes
void test_config_critical_changes() {
    ConfigManager::enable_async_save();
    
    // Звичайна зміна - async
    ConfigManager::set("normal.param", 100);
    
    // Критична зміна - повинна бути sync
    ConfigManager::set("safety.limit", 85.0);
    ConfigManager::force_save_sync(); // Блокуюче для критичних
    
    // Перевіряємо що критичні зміни збережені одразу
    ConfigManager::init();
    ConfigManager::load();
    
    TEST_ASSERT_EQUAL(85.0, ConfigManager::get("safety.limit").get<double>());
}

// Test group runner
void test_config_lifecycle_group(void) {
    RUN_TEST(test_config_sync_load_at_startup);
    RUN_TEST(test_config_available_for_modules);
    RUN_TEST(test_config_async_save_runtime);
    RUN_TEST(test_config_sync_save_shutdown);
    RUN_TEST(test_config_critical_changes);
}
