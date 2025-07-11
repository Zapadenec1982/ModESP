/**
 * @file config_manager_async_corrected.h
 * @brief Правильна архітектура ConfigManager з асинхронним збереженням
 */

#ifndef CONFIG_MANAGER_ASYNC_CORRECTED_H
#define CONFIG_MANAGER_ASYNC_CORRECTED_H

namespace ConfigManager {

/**
 * @brief Життєвий цикл ConfigManager
 * 
 * 1. СТАРТ СИСТЕМИ (блокуючі операції):
 *    - init() - ініціалізація структур
 *    - load() - СИНХРОННЕ завантаження конфігурації
 *    - Всі модулі отримують свої налаштування
 * 
 * 2. РОБОТА СИСТЕМИ (неблокуючі операції):
 *    - set() - зміна конфігурації в RAM
 *    - save() - АСИНХРОННЕ збереження змін
 *    - Система продовжує працювати під час запису
 * 
 * 3. ЗУПИНКА СИСТЕМИ (блокуючі операції):
 *    - flush_and_save() - СИНХРОННЕ збереження всіх змін
 *    - deinit() - очищення ресурсів
 */

// === СИНХРОННІ операції (блокуючі) ===

/**
 * @brief Ініціалізація ConfigManager
 * БЛОКУЮЧА - викликається при старті
 */
esp_err_t init();

/**
 * @brief Завантажити конфігурацію з LittleFS
 * БЛОКУЮЧА - модулі чекають на завантаження
 * @return ESP_OK якщо завантажено, ESP_ERR_NOT_FOUND якщо використано defaults
 */
esp_err_t load();

/**
 * @brief Примусове синхронне збереження
 * БЛОКУЮЧА - використовується при shutdown або критичних змінах
 */
esp_err_t force_save_sync();

// === АСИНХРОННІ операції (неблокуючі) ===

/**
 * @brief Запланувати асинхронне збереження
 * НЕБЛОКУЮЧА - повертається відразу
 * @param module Зберегти тільки вказаний модуль (або всі якщо пусто)
 */
esp_err_t save_async(const std::string& module = "");

/**
 * @brief Автоматичне збереження при змінах
 * Якщо увімкнено, set() автоматично планує збереження
 */
void enable_auto_save(bool enable);

// === Правильний порядок використання ===

/**
 * Приклад правильного старту системи:
 * 
 * void app_main() {
 *     // 1. Ініціалізація файлової системи
 *     esp_vfs_littlefs_register(...);
 *     
 *     // 2. СИНХРОННЕ завантаження конфігурації
 *     ConfigManager::init();
 *     esp_err_t ret = ConfigManager::load();  // БЛОКУЄ!
 *     if (ret != ESP_OK) {
 *         ESP_LOGW(TAG, "Using default config");
 *     }
 *     
 *     // 3. Тепер можна ініціалізувати модулі
 *     ModuleManager::configure_all(ConfigManager::get_all());
 *     ModuleManager::init_all();
 *     
 *     // 4. Включити асинхронне збереження для runtime
 *     ConfigManager::enable_auto_save(true);
 *     
 *     // 5. Основний цикл
 *     while (running) {
 *         // Зміни конфігурації не блокують систему
 *         ConfigManager::set("sensor.offset", 1.5);  // Автоматично заплановано save_async()
 *         
 *         // Інша робота...
 *         vTaskDelay(10);
 *     }
 *     
 *     // 6. СИНХРОННЕ збереження перед shutdown
 *     ConfigManager::force_save_sync();  // БЛОКУЄ!
 *     ConfigManager::deinit();
 * }
 */

} // namespace ConfigManager

#endif
