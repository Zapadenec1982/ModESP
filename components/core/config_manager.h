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
#include "json_validator.h"

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
 * @brief Save configuration to NVS
 * 
 * Атомарно зберігає весь об'єднаний об'єкт конфігурації в NVS.
 * Використовує двофазний коміт для надійності.
 * Скидає прапорець is_dirty після успішного збереження.
 * 
 * @return ESP_OK on success
 */
esp_err_t save();

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
 * @brief Discard unsaved changes
 * 
 * Скасовує всі зміни в RAM-кеші, повертаючи до останнього збереженого стану.
 * Скидає прапорець is_dirty.
 */
void discard_changes();

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