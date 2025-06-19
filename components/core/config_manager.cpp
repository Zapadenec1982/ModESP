#include "config_manager.h"
#include <esp_log.h>
#include <nvs.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include "error_handling.h"

static const char* TAG = "ConfigManager";
static const char* NVS_NAMESPACE = "moduchill";
static const char* CONFIG_KEY = "config_unified";

namespace ConfigManager {

// Embedded modular configuration files (linked by CMake)
// Ці файли будуть додані через CMake як: system.json, climate.json, sensors.json і т.д.
extern const uint8_t system_config_start[] asm("_binary_system_json_start");
extern const uint8_t system_config_end[] asm("_binary_system_json_end");
extern const uint8_t climate_config_start[] asm("_binary_climate_json_start");
extern const uint8_t climate_config_end[] asm("_binary_climate_json_end");
extern const uint8_t sensors_config_start[] asm("_binary_sensors_json_start");
extern const uint8_t sensors_config_end[] asm("_binary_sensors_json_end");
extern const uint8_t actuators_config_start[] asm("_binary_actuators_json_start");
extern const uint8_t actuators_config_end[] asm("_binary_actuators_json_end");
extern const uint8_t alarms_config_start[] asm("_binary_alarms_json_start");
extern const uint8_t alarms_config_end[] asm("_binary_alarms_json_end");
extern const uint8_t network_config_start[] asm("_binary_network_json_start");
extern const uint8_t network_config_end[] asm("_binary_network_json_end");
extern const uint8_t ui_config_start[] asm("_binary_ui_json_start");
extern const uint8_t ui_config_end[] asm("_binary_ui_json_end");
extern const uint8_t logging_config_start[] asm("_binary_logging_json_start");
extern const uint8_t logging_config_end[] asm("_binary_logging_json_end");
extern const uint8_t wifi_config_start[] asm("_binary_wifi_json_start");
extern const uint8_t wifi_config_end[] asm("_binary_wifi_json_end");
extern const uint8_t rtc_config_start[] asm("_binary_rtc_json_start");
extern const uint8_t rtc_config_end[] asm("_binary_rtc_json_end");

// Internal state
static nlohmann::json config_cache;           // RAM-кеш для швидкого доступу
static nlohmann::json last_saved_config;      // Останній збережений стан (для discard)
static bool dirty_flag = false;               // Прапорець "брудного" стану
static uint32_t config_version = 1;          // Поточна версія конфігурації

// Change notification support
static std::vector<ChangeCallback> change_callbacks;

// Структура для опису модульних файлів
struct ConfigModule {
    const char* name;
    const uint8_t* start;
    const uint8_t* end;
};

// Мапа всіх доступних модульних конфігурацій
static const ConfigModule config_modules[] = {
    {"system", system_config_start, system_config_end},
    {"climate", climate_config_start, climate_config_end},
    {"sensors", sensors_config_start, sensors_config_end},
    {"actuators", actuators_config_start, actuators_config_end},
    {"alarms", alarms_config_start, alarms_config_end},
    {"network", network_config_start, network_config_end},
    {"ui", ui_config_start, ui_config_end},
    {"logging", logging_config_start, logging_config_end},
    {"wifi", wifi_config_start, wifi_config_end},
    {"rtc", rtc_config_start, rtc_config_end}
};

static constexpr size_t CONFIG_MODULES_COUNT = sizeof(config_modules) / sizeof(ConfigModule);

// Helper: завантажити один модульний файл
static nlohmann::json load_module_config(const ConfigModule& module) {
    using namespace ModESP;
    
    if (!module.start || !module.end) {
        ESP_LOGW(TAG, "Module %s not embedded", module.name);
        return nlohmann::json::object();
    }
    
    size_t size = module.end - module.start;
    
    // Use heap allocation instead of stack for large JSON strings
    char* json_buffer = static_cast<char*>(malloc(size + 1));
    if (!json_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for module %s", module.name);
        return nlohmann::json::object();
    }
    
    memcpy(json_buffer, module.start, size);
    json_buffer[size] = '\0';
    
    nlohmann::json result = nlohmann::json::object();
    
    esp_err_t parse_result = safe_execute("load_module_config", [&]() -> esp_err_t {
        auto parsed = nlohmann::json::parse(json_buffer, nullptr, false);
        if (parsed.is_discarded()) {
            ESP_LOGE(TAG, "Failed to parse module %s: JSON parse error", module.name);
            return ESP_ERR_INVALID_ARG;
        }
        result = std::move(parsed);
        return ESP_OK;
    });
    
    free(json_buffer);
    
    return (parse_result == ESP_OK) ? result : nlohmann::json::object();
}

// Helper: агрегувати всі модульні файли в єдиний об'єкт
static nlohmann::json aggregate_embedded_configs() {
    nlohmann::json aggregated = nlohmann::json::object();
    
    // Додаємо версію на верхньому рівні
    aggregated["version"] = config_version;
    
    // Агрегуємо всі модулі
    for (size_t i = 0; i < CONFIG_MODULES_COUNT; ++i) {
        const auto& module = config_modules[i];
        nlohmann::json module_config = load_module_config(module);
        
        if (!module_config.empty()) {
            aggregated[module.name] = module_config;
            ESP_LOGD(TAG, "Loaded config section: '%s'", module.name);
        } else {
            ESP_LOGW(TAG, "Failed to load config for module: %s", module.name);
        }
    }
    
    ESP_LOGI(TAG, "Aggregated %zu config modules", CONFIG_MODULES_COUNT);
    return aggregated;
}

// Helper: розпарсити шлях з крапками
static std::vector<std::string> parse_path(const std::string& path) {
    std::vector<std::string> keys;
    if (path.empty()) return keys;
    
    size_t start = 0;
    size_t dot = path.find('.');
    
    while (dot != std::string::npos) {
        if (dot > start) {
            keys.push_back(path.substr(start, dot - start));
        }
        start = dot + 1;
        dot = path.find('.', start);
    }
    
    if (start < path.length()) {
        keys.push_back(path.substr(start));
    }
    
    return keys;
}

// Helper: отримати значення за шляхом
static nlohmann::json get_value_at_path(const nlohmann::json& json, const std::vector<std::string>& keys) {
    const nlohmann::json* current = &json;
    
    for (const auto& key : keys) {
        if (current->is_object() && current->contains(key)) {
            current = &(*current)[key];
        } else {
            return nlohmann::json();
        }
    }
    
    return *current;
}

// Helper: встановити значення за шляхом
static void set_value_at_path(nlohmann::json& json, const std::vector<std::string>& keys, 
                             const nlohmann::json& value) {
    if (keys.empty()) return;
    
    nlohmann::json* current = &json;
    
    // Навігація до батьківського об'єкта
    for (size_t i = 0; i < keys.size() - 1; ++i) {
        const auto& key = keys[i];
        
        if (!current->is_object()) {
            *current = nlohmann::json::object();
        }
        
        if (!current->contains(key)) {
            (*current)[key] = nlohmann::json::object();
        }
        
        current = &(*current)[key];
    }
    
    // Встановити значення
    if (!current->is_object()) {
        *current = nlohmann::json::object();
    }
    
    (*current)[keys.back()] = value;
}

// Helper: повідомити про зміни
static void notify_change(const std::string& path, const nlohmann::json& old_value, 
                         const nlohmann::json& new_value) {
    using namespace ModESP;
    static ErrorCollector error_collector;
    
    for (const auto& callback : change_callbacks) {
        esp_err_t result = safe_execute("change_callback", [&]() -> esp_err_t {
            callback(path, old_value, new_value);
            return ESP_OK;
        });
        
        if (result != ESP_OK) {
            error_collector.add(result, "change_callback for path: " + path);
        }
    }
    
    if (error_collector.has_errors()) {
        error_collector.log_all(TAG);
        error_collector.clear();
    }
}

// Helper: виконати міграцію конфігурації
static void migrate_config(nlohmann::json& config, uint32_t from_version) {
    ESP_LOGI(TAG, "Migrating config from v%lu to v%lu", from_version, config_version);
    
    // Отримуємо еталонну конфігурацію з вбудованих файлів
    nlohmann::json reference = aggregate_embedded_configs();
    
    // Додаємо відсутні секції верхнього рівня
    for (auto& [key, value] : reference.items()) {
        if (key != "version" && !config.contains(key)) {
            ESP_LOGI(TAG, "Adding missing config section: %s", key.c_str());
            config[key] = value;
        }
    }
    
    // Оновлюємо версію
    config["version"] = config_version;
    
    ESP_LOGI(TAG, "Migration completed");
}

esp_err_t init() {
    ESP_LOGI(TAG, "Initializing ConfigManager...");
    
    // Ініціалізуємо внутрішні структури
    change_callbacks.clear();
    dirty_flag = false;
    
    // Завантажуємо початкову конфігурацію з вбудованих файлів  
    config_cache = aggregate_embedded_configs();
    last_saved_config = config_cache; // Початково кеш = збережений стан
    
    ESP_LOGI(TAG, "ConfigManager initialized with %zu modules", CONFIG_MODULES_COUNT);
    return ESP_OK;
}

esp_err_t load() {
    using namespace ModESP;
    
    ESP_LOGI(TAG, "Loading configuration...");
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, using embedded defaults");
        // Конфігурація вже завантажена в init() з вбудованих файлів
        dirty_flag = true; // Позначаємо для збереження при першому запуску
        return ESP_OK;
    }
    
    // Отримуємо розмір збереженої конфігурації
    size_t required_size = 0;
    err = nvs_get_str(handle, CONFIG_KEY, nullptr, &required_size);
    
    if (err == ESP_OK && required_size > 0) {
        // Виділяємо буфер та зчитуємо
        char* json_str = (char*)malloc(required_size);
        if (json_str == nullptr) {
            nvs_close(handle);
            return ESP_ERR_NO_MEM;
        }
        
        err = nvs_get_str(handle, CONFIG_KEY, json_str, &required_size);
        
        if (err == ESP_OK) {
            esp_err_t parse_result = safe_execute("parse_saved_config", [&]() -> esp_err_t {
                auto result = nlohmann::json::parse(json_str, nullptr, false);
                if (result.is_discarded()) {
                    ESP_LOGE(TAG, "Failed to parse saved config: JSON parse error");
                    return ESP_ERR_INVALID_ARG;
                }
                
                nlohmann::json loaded_config = result;
                
                // Перевіряємо версію та виконуємо міграцію при необхідності
                uint32_t loaded_version = loaded_config.value("version", 0);
                if (loaded_version < config_version) {
                    migrate_config(loaded_config, loaded_version);
                    dirty_flag = true; // Потрібно зберегти після міграції
                }
                
                // Оновлюємо кеш та збережений стан
                config_cache = loaded_config;
                last_saved_config = loaded_config;
                dirty_flag = false;
                
                ESP_LOGI(TAG, "Configuration loaded from NVS (%zu bytes)", required_size);
                return ESP_OK;
            });
            
            if (parse_result != ESP_OK) {
                ESP_LOGW(TAG, "Using embedded defaults due to parse error");
                // Залишаємо агреговану конфігурацію з init()
                dirty_flag = true;
            }
        } else {
            ESP_LOGW(TAG, "Failed to read config from NVS: %s", esp_err_to_name(err));
        }
        
        free(json_str);
    } else {
        ESP_LOGW(TAG, "No saved configuration found, using embedded defaults");
        dirty_flag = true; // Позначаємо для збереження
    }
    
    nvs_close(handle);
    return ESP_OK;
}

esp_err_t save() {
    if (!dirty_flag) {
        ESP_LOGD(TAG, "Configuration not changed, skipping save");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Saving configuration to NVS...");
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    // Серіалізуємо весь об'єднаний об'єкт конфігурації
    std::string json_str = config_cache.dump();
    
    // Атомарно записуємо в NVS
    err = nvs_set_str(handle, CONFIG_KEY, json_str.c_str());
    
    if (err == ESP_OK) {
        err = nvs_commit(handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Configuration saved (%zu bytes)", json_str.length());
            
            // Оновлюємо збережений стан та скидаємо прапорець
            last_saved_config = config_cache;
            dirty_flag = false;
        } else {
            ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Failed to write config to NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(handle);
    return err;
}

nlohmann::json get(const std::string& path) {
    if (path.empty()) {
        return config_cache;
    }
    
    std::vector<std::string> keys = parse_path(path);
    return get_value_at_path(config_cache, keys);
}

esp_err_t set(const std::string& path, const nlohmann::json& value) {
    if (path.empty()) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Отримуємо старе значення для порівняння
    nlohmann::json old_value = get(path);
    
    // Встановлюємо нове значення в кеші
    std::vector<std::string> keys = parse_path(path);
    set_value_at_path(config_cache, keys, value);
    
    // Позначаємо як "брудний" та логуємо
    dirty_flag = true;
    ESP_LOGD(TAG, "Set %s = %s", path.c_str(), value.dump().c_str());
    
    // Повідомляємо про зміну якщо значення відрізняється
    if (old_value != value) {
        notify_change(path, old_value, value);
    }
    
    return ESP_OK;
}

bool is_dirty() {
    return dirty_flag;
}

void discard_changes() {
    if (!dirty_flag) {
        ESP_LOGD(TAG, "No changes to discard");
        return;
    }
    
    ESP_LOGI(TAG, "Discarding unsaved changes");
    
    // Відновлюємо кеш до останнього збереженого стану
    config_cache = last_saved_config;
    dirty_flag = false;
}

nlohmann::json get_all() {
    return config_cache;
}

bool has(const std::string& path) {
    nlohmann::json value = get(path);
    return !value.is_null();
}

uint32_t get_version() {
    return config_cache.value("version", 1);
}

bool validate(const nlohmann::json& config) {
    nlohmann::json to_validate = config.empty() ? config_cache : config;
    
    // Перевіряємо обов'язкове поле версії
    if (!to_validate.contains("version") || !to_validate["version"].is_number()) {
        ESP_LOGE(TAG, "Missing or invalid version field");
        return false;
    }
    
    // Перевіряємо наявність основних секцій
    std::vector<std::string> required_sections = {"system", "climate", "sensors"};
    
    for (const auto& section : required_sections) {
        if (!to_validate.contains(section) || !to_validate[section].is_object()) {
            ESP_LOGE(TAG, "Missing or invalid section: %s", section.c_str());
            return false;
        }
    }
    
    // Валідуємо temperature ranges в climate секції
    if (to_validate.contains("climate")) {
        const auto& climate = to_validate["climate"];
        
        if (climate.contains("min_temp") && climate.contains("max_temp")) {
            float min_temp = climate["min_temp"];
            float max_temp = climate["max_temp"];
            
            if (min_temp >= max_temp) {
                ESP_LOGE(TAG, "Invalid temperature range: min_temp >= max_temp");
                return false;
            }
            
            if (climate.contains("setpoint")) {
                float setpoint = climate["setpoint"];
                if (setpoint < min_temp || setpoint > max_temp) {
                    ESP_LOGE(TAG, "Setpoint %.2f outside valid range [%.2f, %.2f]", 
                             setpoint, min_temp, max_temp);
                    return false;
                }
            }
        }
    }
    
    // Валідуємо compressor налаштування
    if (to_validate.contains("compressor")) {
        const auto& comp = to_validate["compressor"];
        
        if (comp.contains("min_off_time") && comp["min_off_time"].is_number()) {
            int min_off = comp["min_off_time"];
            if (min_off < 60) {
                ESP_LOGW(TAG, "Compressor min_off_time %ds < 60s may damage equipment", min_off);
            }
        }
        
        if (comp.contains("max_starts_per_hour") && comp["max_starts_per_hour"].is_number()) {
            int max_starts = comp["max_starts_per_hour"];
            if (max_starts > 20) {
                ESP_LOGW(TAG, "Compressor max_starts_per_hour %d > 20 may damage equipment", max_starts);
            }
        }
    }
    
    ESP_LOGD(TAG, "Configuration validation passed");
    return true;
}

std::string export_config(bool pretty) {
    return config_cache.dump(pretty ? 4 : -1);
}

esp_err_t import_config(const std::string& json_str) {
    using namespace ModESP;
    
    Result<nlohmann::json> parse_result = safe_execute("parse_imported_config", [&]() -> esp_err_t {
        auto result = nlohmann::json::parse(json_str, nullptr, false);
        if (result.is_discarded()) {
            ESP_LOGE(TAG, "Failed to parse imported JSON: JSON parse error");
            return ESP_ERR_INVALID_ARG;
        }
        return ESP_OK;
    }) == ESP_OK ? Result<nlohmann::json>(nlohmann::json::parse(json_str)) 
                  : Result<nlohmann::json>(ESP_ERR_INVALID_ARG, "JSON parse failed");

    if (!parse_result) {
        return parse_result.error();
    }
    
    nlohmann::json new_config = *parse_result;
    
    if (!validate(new_config)) {
        ESP_LOGE(TAG, "Imported configuration failed validation");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Зберігаємо старий кеш для відновлення у разі помилки
    nlohmann::json old_cache = config_cache;
    
    config_cache = new_config;
    dirty_flag = true;
    
    ESP_LOGI(TAG, "Configuration imported successfully");
    return ESP_OK;
}

void on_change(ChangeCallback callback) {
    change_callbacks.push_back(callback);
}

std::vector<std::string> get_config_modules() {
    std::vector<std::string> modules;
    modules.reserve(CONFIG_MODULES_COUNT);
    
    for (size_t i = 0; i < CONFIG_MODULES_COUNT; ++i) {
        modules.push_back(config_modules[i].name);
    }
    
    return modules;
}

esp_err_t reload_defaults() {
    using namespace ModESP;
    
    ESP_LOGI(TAG, "Reloading default configuration from embedded files");
    
    // Зберігаємо старий кеш для відновлення
    nlohmann::json old_cache = config_cache;
    
    esp_err_t result = safe_execute("reload_defaults", [&]() -> esp_err_t {
        // Агрегуємо свіжу конфігурацію з вбудованих файлів
        config_cache = aggregate_embedded_configs();
        dirty_flag = true;
        
        ESP_LOGI(TAG, "Default configuration reloaded");
        return ESP_OK;
    });
    
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reload defaults");
        // Відновлюємо старий кеш
        config_cache = old_cache;
        return ESP_ERR_INVALID_STATE;
    }
    
    return ESP_OK;
}

} // namespace ConfigManager