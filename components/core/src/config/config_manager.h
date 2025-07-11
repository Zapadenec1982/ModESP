/**
 * @file config_manager.h
 * @brief Modular JSON-based configuration management with NVS persistence
 * 
 * ConfigManager - централізований компонент для управління персистентними налаштуваннями.
 * Ключові особливості:
 * - Модульні файли: кожен функціональний блок має окремий .json файл
 * - Агрегація в пам'яті: всі файли об'єднуються в єдиний JSON-об'єкт  
 * - Зберігання в NVS: атомарний запис об'єднаної конфігурації
 * - RAM-кеш: всі операції читання з кешу для максимальної швидкодії
 * - Версійність та міграція для безпечних оновлень
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <vector>
#include <functional>
#include "esp_err.h"
#include "nlohmann/json.hpp"

namespace ConfigManager {

/**
 * @brief Initialize ConfigManager
 * 
 * Must be called once during system initialization.
 * Sets up internal structures for modular configuration handling.
 * 
 * @return ESP_OK on success
 */
esp_err_t init();

/**
 * @brief Deinitialize ConfigManager
 * 
 * Safely unmount LittleFS and clean up resources.
 * Should be called during system shutdown.
 * 
 * @return ESP_OK on success
 */
esp_err_t deinit();

/**
 * @brief Load initial configuration (STACK-SAFE)
 * 
 * Безпечне завантаження конфігурації після ініціалізації всіх модулів.
 * ОБОВ'ЯЗКОВО викликати після init() та ініціалізації ModuleManager!
 * 
 * @return ESP_OK on success
 */
esp_err_t load_initial_config();

/**
 * @brief Load configuration from NVS or embedded defaults
 * 
 * Завантаження конфігурації:
 * 1. Спробувати завантажити з NVS
 * 2. Якщо немає - агрегувати всі вбудовані .json файли  
 * 3. Виконати міграцію при невідповідності версій
 * 4. Зберегти в RAM-кеш для швидкого доступу
 * 
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no saved config (using defaults)
 */
esp_err_t load();

/**
 * @brief Save configuration to storage
 * 
 * Автоматично вибирає асинхронне або синхронне збереження
 * залежно від налаштувань компіляції.
 * 
 * @return ESP_OK on success
 */
esp_err_t save();

/**
 * @brief Force synchronous save
 * 
 * Примусове синхронне збереження з очікуванням завершення.
 * Використовується для критичних операцій та shutdown.
 * 
 * @return ESP_OK on success
 */
esp_err_t force_save_sync();

/**
 * @brief Get configuration value by path
 * 
 * Швидке читання з RAM-кешу за шляхом з крапками.
 * Приклади: "climate.setpoint", "sensors.temperature.offset"
 * 
 * @param path Dot-separated path to value
 * @return JSON value or null if not found
 */
nlohmann::json get(const std::string& path);

/**
 * @brief Set configuration value
 * 
 * Оновлює значення в RAM-кеші та встановлює is_dirty = true.
 * Створює вкладені об'єкти при необхідності.
 * Викликає зареєстровані callback'и змін.
 * 
 * @param path Dot-separated path to value
 * @param value JSON value to set
 * @return ESP_OK on success
 */
esp_err_t set(const std::string& path, const nlohmann::json& value);

/**
 * @brief Check if there are unsaved changes
 * 
 * @return true if configuration was modified and not saved
 */
bool is_dirty();

/**
 * @brief Reset configuration to factory defaults
 * 
 * Скидає конфігурацію до заводських налаштувань.
 * Можна скинути всю конфігурацію або окрему секцію.
 * 
 * @param section Назва секції для скидання (пустий рядок = вся конфігурація)
 * @return ESP_OK on success
 */
esp_err_t reset_to_defaults(const std::string& section = "");

/**
 * @brief Get complete aggregated configuration
 * 
 * Повертає весь об'єднаний об'єкт конфігурації з RAM-кешу.
 * 
 * @return Full configuration as JSON object
 */
nlohmann::json get_all();

/**
 * @brief Check if configuration path exists
 * 
 * @param path Dot-separated path to check
 * @return true if path exists in current configuration
 */
bool has(const std::string& path);

/**
 * @brief Get current configuration version
 * 
 * @return Configuration version number
 */
uint32_t get_version();

/**
 * @brief Validate configuration against schema
 * 
 * Перевіряє цілісність конфігурації:
 * - Наявність обов'язкових полів
 * - Діапазони значень
 * - Типи даних
 * 
 * @param config Configuration to validate (current if empty)
 * @return true if valid
 */
bool validate(const nlohmann::json& config = {});

/**
 * @brief Export configuration as string
 * 
 * @param pretty Pretty-print JSON if true
 * @return Configuration as JSON string
 */
std::string export_config(bool pretty = true);

/**
 * @brief Import configuration from string
 * 
 * Валідує та завантажує конфігурацію з JSON-рядка.
 * Встановлює is_dirty = true.
 * 
 * @param json_str Configuration as JSON string
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if invalid
 */
esp_err_t import_config(const std::string& json_str);

/**
 * @brief Configuration change callback
 * @param path Path that changed  
 * @param old_value Previous value
 * @param new_value New value
 */
using ChangeCallback = std::function<void(const std::string& path, 
                                         const nlohmann::json& old_value,
                                         const nlohmann::json& new_value)>;

/**
 * @brief Register callback for configuration changes
 * 
 * Викликається при зміні значень через set().
 * 
 * @param callback Function to call on changes
 */
void on_change(ChangeCallback callback);

/**
 * @brief Get list of available modular config files
 * 
 * Повертає імена всіх вбудованих .json файлів конфігурації.
 * 
 * @return Vector of config module names (без розширення .json)
 */
std::vector<std::string> get_config_modules();

/**
 * @brief Enable asynchronous save operations
 * 
 * ВАЖЛИВО: Викликати тільки ПІСЛЯ ініціалізації модулів!
 * Включає асинхронне збереження для runtime операцій.
 * 
 * @return ESP_OK on success
 */
esp_err_t enable_async_save();

/**
 * @brief Enable automatic save on configuration changes
 * 
 * Якщо увімкнено, set() автоматично планує асинхронне збереження.
 * Потребує попереднього виклику enable_async_save().
 * 
 * @param enable true to enable auto-save
 */
void enable_auto_save(bool enable);

/**
 * @brief Get save status information
 */
struct SaveStatus {
    bool has_unsaved_changes;
    uint32_t pending_async_saves;
    uint64_t last_save_timestamp;
    size_t config_size_bytes;
};

SaveStatus get_save_status();

/**
 * @brief Simple configuration health status for industrial systems
 */
struct ConfigHealth {
    bool startup_successful;        ///< Configuration loaded successfully at startup
    bool all_modules_present;       ///< All expected config modules found
    bool validation_passed;         ///< Schema validation passed
    bool save_working;              ///< Last save operation succeeded
    uint32_t config_size_bytes;     ///< Current configuration size
    uint32_t last_error_code;       ///< Last error code (0 = no error)
};

/**
 * @brief Get simple configuration health status
 * 
 * Returns basic health information for troubleshooting.
 * Designed for industrial reliability, not performance profiling.
 * 
 * @return Current configuration health status
 */
ConfigHealth get_config_health();

/**
 * @brief Enhanced configuration diagnostics
 */
struct ConfigDiagnostics {
    bool all_modules_loaded;                ///< True if all config modules loaded successfully
    std::vector<std::string> missing_modules;   ///< List of missing configuration modules
    uint32_t config_version;                ///< Current configuration version
    bool schema_validation_passed;          ///< True if schema validation passed
    size_t total_keys_count;               ///< Total number of configuration keys
};

/**
 * @brief Get comprehensive configuration diagnostics
 * 
 * Returns detailed diagnostic information about configuration state,
 * useful for troubleshooting and health monitoring.
 * 
 * @return Current configuration diagnostics
 */
ConfigDiagnostics get_diagnostics();

/**
 * @brief Force reload from embedded files
 * 
 * Перезавантажує конфігурацію з вбудованих файлів, ігноруючи NVS.
 * Корисно для відновлення після пошкодження.
 * Встановлює is_dirty = true.
 * 
 * @return ESP_OK on success
 */
esp_err_t reload_defaults();

} // namespace ConfigManager

#endif // CONFIG_MANAGER_H