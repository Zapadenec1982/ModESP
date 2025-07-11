/**
 * @file config_manager_api_update.h
 * @brief Оновлений API ConfigManager з підтримкою правильного lifecycle
 */

// Додати в config_manager.h:

namespace ConfigManager {

// === Існуючі функції (без змін) ===
esp_err_t init();
esp_err_t load();  // ЗАВЖДИ синхронний!
esp_err_t save();  // Може бути async якщо включено
nlohmann::json get(const std::string& path);
esp_err_t set(const std::string& path, const nlohmann::json& value);
bool is_dirty();
void discard_changes();
nlohmann::json get_all();
bool has(const std::string& path);

// === НОВІ функції для lifecycle management ===

/**
 * @brief Включити асинхронне збереження для runtime
 * 
 * Викликати ПІСЛЯ ініціалізації всіх модулів!
 * 
 * @return ESP_OK якщо успішно включено
 */
esp_err_t enable_async_save();

/**
 * @brief Включити автоматичне збереження при змінах
 * 
 * Коли увімкнено, set() автоматично планує async save
 * 
 * @param enable true для включення
 */
void enable_auto_save(bool enable);

/**
 * @brief Примусове синхронне збереження
 * 
 * Використовувати для:
 * - Критичних змін
 * - Перед shutdown
 * - На вимогу користувача
 * 
 * @return ESP_OK якщо збережено
 */
esp_err_t force_save_sync();

/**
 * @brief Отримати статус збереження
 * 
 * @return Структура з інформацією про стан
 */
struct SaveStatus {
    bool has_unsaved_changes;      // Є незбережені зміни в RAM
    uint32_t pending_async_saves;  // Кількість запланованих saves
    uint64_t last_save_timestamp;  // Час останнього збереження
    size_t config_size_bytes;      // Розмір конфігурації
};
SaveStatus get_save_status();

/**
 * @brief Зберегти конкретний модуль асинхронно
 * 
 * Більш ефективно ніж save() для великих конфігурацій
 * 
 * @param module_name Ім'я модуля для збереження
 * @return ESP_OK якщо заплановано
 */
esp_err_t save_module_async(const std::string& module_name);

// === Внутрішні функції (не для прямого використання) ===

/**
 * @brief Синхронне збереження (internal)
 * 
 * Використовується внутрішньо або через force_save_sync()
 */
esp_err_t save_sync();

} // namespace ConfigManager

// === Приклад правильного використання ===

/**
 * @code
 * // При старті (в Application::init)
 * ConfigManager::init();
 * ConfigManager::load();  // БЛОКУЄ - це правильно!
 * 
 * // ... ініціалізація модулів ...
 * 
 * ConfigManager::enable_async_save();  // Тільки після модулів!
 * ConfigManager::enable_auto_save(true);
 * 
 * // Під час роботи
 * ConfigManager::set("sensor.value", 42);  // Auto async save
 * 
 * // Критичні зміни
 * ConfigManager::set("safety.limit", 100);
 * ConfigManager::force_save_sync();  // Негайно і надійно
 * 
 * // При shutdown
 * ConfigManager::force_save_sync();  // Гарантоване збереження
 * ConfigManager::deinit();
 * @endcode
 */
