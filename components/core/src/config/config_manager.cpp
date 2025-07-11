#include "config_manager.h"
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_littlefs.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdio>


#ifdef CONFIG_USE_ASYNC_SAVE
#include "config_manager_async.h"
#endif

static const char* TAG = "ConfigManager";

// LittleFS mount configuration
static const char* LITTLEFS_BASE_PATH = "/storage";
static const char* CONFIG_DIR = "/storage/configs";

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

// Internal state - використовуємо smart pointers для heap allocation великих JSON
static std::unique_ptr<nlohmann::json> config_cache_ptr;      // RAM-кеш для швидкого доступу
static bool dirty_flag = false;                               // Прапорець "брудного" стану
static uint32_t config_version = 1;          // Поточна версія конфігурації
static esp_err_t last_error_code = ESP_OK;  // Останній код помилки
static bool startup_successful = false;     // Чи успішно завантажилась конфігурація
static std::vector<ChangeCallback> change_callbacks;

// Helper functions for thread-safe access to heap-allocated JSON
static nlohmann::json& get_config_cache() {
    if (!config_cache_ptr) {
#ifdef CONFIG_USE_PSRAM_FOR_CONFIG
        // Try to allocate in PSRAM if available
        size_t psram_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        if (psram_size > sizeof(nlohmann::json)) {
            void* psram_ptr = heap_caps_malloc(sizeof(nlohmann::json), MALLOC_CAP_SPIRAM);
            if (psram_ptr) {
                config_cache_ptr = std::unique_ptr<nlohmann::json>(
                    new (psram_ptr) nlohmann::json()
                );
                ESP_LOGI(TAG, "Allocated config cache in PSRAM (%zu KB available)", 
                         psram_size / 1024);
            }
        }
#endif
        // Fallback to regular allocation
        if (!config_cache_ptr) {
            config_cache_ptr = std::make_unique<nlohmann::json>();
            ESP_LOGI(TAG, "Allocated config cache in internal RAM");
        }
    }
    return *config_cache_ptr;
}

// Async save support
#ifdef CONFIG_USE_ASYNC_SAVE
static bool async_save_enabled = false;       // Флаг використання асинхронного збереження
static bool auto_save_enabled = false;        // Флаг автоматичного збереження
#endif

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

// Helper: завантажити один модульний файл (STACK-OPTIMIZED)
static nlohmann::json load_module_config(const ConfigModule& module) {
    if (!module.start || !module.end) {
        ESP_LOGW(TAG, "Module %s not embedded", module.name);
        return nlohmann::json::object();
    }
    
    size_t size = module.end - module.start;
    ESP_LOGD(TAG, "Loading module %s: %zu bytes", module.name, size);
    
    // STACK-SAFE: Use heap allocation with smart pointer for automatic cleanup
    std::unique_ptr<char[]> json_buffer = std::make_unique<char[]>(size + 1);
    if (!json_buffer) {
        ESP_LOGE(TAG, "Failed to allocate memory for module %s", module.name);
        return nlohmann::json::object();
    }
    
    memcpy(json_buffer.get(), module.start, size);
    json_buffer[size] = '\0';
    
    // STACK-SAFE: Minimal stack usage for JSON parsing without exceptions
    nlohmann::json result;
    // Parse with exceptions disabled
    result = nlohmann::json::parse(json_buffer.get(), nullptr, false, false);
    if (result.is_discarded()) {
        ESP_LOGE(TAG, "Failed to parse module %s: JSON parse error", module.name);
        result = nlohmann::json::object();
    }
    
    // json_buffer автоматично звільняється через unique_ptr
    ESP_LOGD(TAG, "Module %s parsing completed", module.name);
    
    return result;
}

// Helper: агрегувати всі модульні файли в єдиний об'єкт
static nlohmann::json aggregate_embedded_configs() {
    nlohmann::json aggregated = nlohmann::json::object();
    
    // Додаємо версію на верхньому рівні
    aggregated["version"] = config_version;
    
    // Агрегуємо всі модулі по одному для економії стеку
    for (size_t i = 0; i < CONFIG_MODULES_COUNT; ++i) {
        const auto& module = config_modules[i];
        
        // Обробляємо кожен модуль окремо щоб не накопичувати в стеку
        {
            // STACK-SAFE: Use smart pointer for automatic memory management
            std::unique_ptr<nlohmann::json> module_config_ptr = 
                std::make_unique<nlohmann::json>(load_module_config(module));
            
            if (!module_config_ptr->empty()) {
                aggregated[module.name] = std::move(*module_config_ptr); // Переміщуємо замість копіювання
                ESP_LOGD(TAG, "Loaded config section: '%s'", module.name);
            } else {
                ESP_LOGW(TAG, "Failed to load config for module: %s", module.name);
            }
            // module_config_ptr автоматично видаляється тут
        }
        
        // CRITICAL: Дамо планувальнику можливість перемкнутися після КОЖНОГО модуля
        vTaskDelay(pdMS_TO_TICKS(2));
        
        // Додатковий yield кожні 3 модулі
        if (i % 3 == 2) {
            ESP_LOGD(TAG, "Stack safety yield in aggregate after %zu modules", i + 1);
            vTaskDelay(pdMS_TO_TICKS(5));
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
    for (const auto& callback : change_callbacks) {
        // Note: callback errors are not caught since ESP-IDF builds without exceptions
        callback(path, old_value, new_value);
    }
}

// Helper: виконати міграцію конфігурації
static void migrate_config(nlohmann::json& config, uint32_t from_version);

// Forward declaration
static esp_err_t save_sync_internal();

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

// Внутрішня функція синхронного збереження
static esp_err_t save_sync_internal() {
    ESP_LOGI(TAG, "Saving configuration to filesystem (sync mode)...");
    
    size_t total_saved = 0;
    bool all_saved = true;
    esp_err_t err = ESP_OK;
    
    // Ensure config directory exists
    struct stat st;
    if (stat(CONFIG_DIR, &st) != 0) {
        if (mkdir(CONFIG_DIR, 0755) != 0) {
            ESP_LOGE(TAG, "Failed to create config directory: %s", CONFIG_DIR);
            return ESP_FAIL;
        }
        ESP_LOGI(TAG, "Created config directory: %s", CONFIG_DIR);
    }
    
    // Save version first
    uint32_t version = get_config_cache().value("version", config_version);
    char version_path[128];
    snprintf(version_path, sizeof(version_path), "%s/version.dat", CONFIG_DIR);
    
    FILE* version_file = fopen(version_path, "wb");
    if (version_file) {
        if (fwrite(&version, sizeof(version), 1, version_file) != 1) {
            ESP_LOGE(TAG, "Failed to write version data");
            all_saved = false;
        }
        fclose(version_file);
    } else {
        ESP_LOGE(TAG, "Failed to create version file: %s", version_path);
        all_saved = false;
    }
    
    // Save each module configuration separately
    for (size_t i = 0; i < CONFIG_MODULES_COUNT; i++) {
        const ConfigModule& module = config_modules[i];
        
        if (get_config_cache().contains(module.name)) {
            // Serialize this module's config
            std::string module_json = get_config_cache()[module.name].dump(-1, ' ', false);
            
            char module_path[128];
            snprintf(module_path, sizeof(module_path), "%s/%s.json", CONFIG_DIR, module.name);
            
            FILE* module_file = fopen(module_path, "w");
            if (module_file) {
                size_t written = fwrite(module_json.c_str(), 1, module_json.length(), module_file);
                fclose(module_file);
                
                if (written == module_json.length()) {
                    total_saved += written;
                    ESP_LOGD(TAG, "Saved module %s (%zu bytes)", module.name, written);
                } else {
                    ESP_LOGE(TAG, "Failed to write complete data for module %s", module.name);
                    all_saved = false;
                }
            } else {
                ESP_LOGE(TAG, "Failed to create file for module %s: %s", module.name, module_path);
                all_saved = false;
            }
        }
    }
    
    if (all_saved) {
        ESP_LOGI(TAG, "Configuration saved successfully (%zu bytes total)", total_saved);
        
        // Clear dirty flag
        dirty_flag = false;
        err = ESP_OK;
    } else {
        ESP_LOGW(TAG, "Some modules failed to save");
        err = ESP_FAIL;
    }
    
    return err;
}

esp_err_t init() {
    ESP_LOGI(TAG, "Initializing ConfigManager...");
    
    // Initialize LittleFS
    esp_vfs_littlefs_conf_t conf = {
        .base_path = LITTLEFS_BASE_PATH,
        .partition_label = "storage",
        .partition = nullptr,
        .format_if_mount_failed = true,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = false
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LittleFS: %s", esp_err_to_name(ret));
        return ret;
    }
    
    size_t total = 0, used = 0;
    ret = esp_littlefs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "LittleFS mounted: %zu KB total, %zu KB used", total/1024, used/1024);
    }
    
    // Ensure directory structure exists
    struct stat st;
    if (stat(LITTLEFS_BASE_PATH, &st) != 0) {
        ESP_LOGE(TAG, "LittleFS base path not accessible: %s", LITTLEFS_BASE_PATH);
        return ESP_FAIL;
    }
    
    // Create essential directories only
    if (stat(CONFIG_DIR, &st) != 0) {
        if (mkdir(CONFIG_DIR, 0755) == 0) {
            ESP_LOGD(TAG, "Created config directory: %s", CONFIG_DIR);
        } else {
            ESP_LOGW(TAG, "Failed to create config directory: %s", CONFIG_DIR);
        }
    }
    
    // Ініціалізуємо внутрішні структури
    change_callbacks.clear();
    dirty_flag = false;
    
    // ВАЖЛИВО: НЕ завантажуємо конфігурацію в init() через stack overflow!
    // Конфігурація буде завантажена окремим викликом load() після ініціалізації всіх модулів
    
#ifdef CONFIG_USE_ASYNC_SAVE
    // Ініціалізуємо асинхронний менеджер
    ConfigManagerAsync::AsyncConfig async_config;
    async_config.write_queue_size = CONFIG_ASYNC_SAVE_QUEUE_SIZE;
    async_config.batch_delay_ms = CONFIG_ASYNC_SAVE_BATCH_DELAY_MS;
    async_config.watchdog_feed_interval = CONFIG_ASYNC_SAVE_WATCHDOG_FEED_MS;
#ifdef CONFIG_CONFIG_FILE_COMPRESSION
    async_config.enable_compression = CONFIG_CONFIG_FILE_COMPRESSION;
#else
    async_config.enable_compression = false;
#endif
    
    if (ConfigManagerAsync::init_async(async_config) == ESP_OK) {
        async_save_enabled = true;
        ESP_LOGI(TAG, "Async save enabled (queue: %zu, batch: %lums, watchdog: %lums)", 
                async_config.write_queue_size, async_config.batch_delay_ms, 
                async_config.watchdog_feed_interval);
    } else {
        ESP_LOGW(TAG, "Failed to init async save, using sync mode");
        async_save_enabled = false;
    }
#endif
    
    ESP_LOGI(TAG, "ConfigManager initialized with %zu modules", CONFIG_MODULES_COUNT);
    return ESP_OK;
}

esp_err_t deinit() {
    ESP_LOGI(TAG, "Deinitializing ConfigManager...");
    
#ifdef CONFIG_USE_ASYNC_SAVE
    if (async_save_enabled) {
        // Зберегти всі незбережені зміни
        ESP_LOGI(TAG, "Flushing pending async saves...");
        ConfigManagerAsync::flush_pending_saves(5000);
        ConfigManagerAsync::stop_async();
        async_save_enabled = false;
    }
#endif
    
    // Save any pending changes
    if (dirty_flag) {
        ESP_LOGI(TAG, "Saving pending changes before shutdown");
        esp_err_t ret = save();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to save pending changes: %s", esp_err_to_name(ret));
        }
    }
    
    // Clear internal structures
    change_callbacks.clear();
    get_config_cache().clear();
    dirty_flag = false;
    
    // Unmount LittleFS
    esp_err_t ret = esp_vfs_littlefs_unregister("storage");
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unmount LittleFS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "LittleFS unmounted successfully");
    }
    
    ESP_LOGI(TAG, "ConfigManager deinitialized");
    return ESP_OK;
}

esp_err_t load_initial_config() {
    ESP_LOGI(TAG, "Loading initial configuration...");
    
    // Ініціалізуємо конфігурацію з default values якщо ще не зроблено
    if (!config_cache_ptr || config_cache_ptr->empty()) {
        ESP_LOGI(TAG, "Initializing config cache with embedded defaults");
        
        // STACK-SAFE: Завантажуємо по одному модулю з очищенням стеку
        nlohmann::json& cache = get_config_cache();
        cache = nlohmann::json::object();
        cache["version"] = config_version;
        
        ESP_LOGI(TAG, "Loading %zu config modules sequentially...", CONFIG_MODULES_COUNT);
        
        // STACK-SAFE: Завантажуємо модулі з додатковим yield та heap allocation
        for (size_t i = 0; i < CONFIG_MODULES_COUNT; ++i) {
            const ConfigModule& module = config_modules[i];
            ESP_LOGD(TAG, "Loading module %zu: %s", i, module.name);
            
            // Завантажуємо модуль в локальній області видимості з дуже обмеженим стеком
            {
                // Використовуємо вказівник для мінімізації стеку
                std::unique_ptr<nlohmann::json> module_config_ptr = 
                    std::make_unique<nlohmann::json>(load_module_config(module));
                
                if (!module_config_ptr->empty()) {
                    cache[module.name] = std::move(*module_config_ptr);
                    ESP_LOGD(TAG, "Module %s loaded successfully", module.name);
                } else {
                    ESP_LOGW(TAG, "Module %s is empty or failed to load", module.name);
                }
                // module_config_ptr автоматично знищується тут
            }
            
            // CRITICAL: Yield після КОЖНОГО модуля для очищення стеку
            vTaskDelay(pdMS_TO_TICKS(5));
            
            // Додатковий yield кожні 3 модулі для безпеки
            if (i % 3 == 2) {
                ESP_LOGD(TAG, "Stack safety yield after %zu modules", i + 1);
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        
        dirty_flag = true; // Mark for future save
        
        ESP_LOGI(TAG, "Config cache initialized with %zu modules", CONFIG_MODULES_COUNT);
    }
    
    ESP_LOGI(TAG, "Initial configuration loaded successfully");
    return ESP_OK;
}

esp_err_t load() {
    
    ESP_LOGI(TAG, "Loading configuration...");
    
    // Завантажуємо конфігурацію з файлової системи
    nlohmann::json loaded_config = nlohmann::json::object();
    bool any_section_loaded = false;
    
    // Try to load version first
    uint32_t saved_version = config_version; // default to current
    char version_path[128];
    snprintf(version_path, sizeof(version_path), "%s/version.dat", CONFIG_DIR);
    
    FILE* version_file = fopen(version_path, "rb");
    if (version_file) {
        if (fread(&saved_version, sizeof(saved_version), 1, version_file) == 1) {
            loaded_config["version"] = saved_version;
            ESP_LOGI(TAG, "Loaded config version: %lu", saved_version);
        }
        fclose(version_file);
    }
    
    // Load each module configuration separately
    for (size_t i = 0; i < CONFIG_MODULES_COUNT; i++) {
        const ConfigModule& module = config_modules[i];
        
        char module_path[128];
        snprintf(module_path, sizeof(module_path), "%s/%s.json", CONFIG_DIR, module.name);
        
        FILE* module_file = fopen(module_path, "r");
        if (module_file) {
            // Get file size
            fseek(module_file, 0, SEEK_END);
            long file_size = ftell(module_file);
            fseek(module_file, 0, SEEK_SET);
            
            if (file_size > 0 && file_size < 4096) { // Reasonable size limit
                // Allocate buffer and read
                char* json_str = (char*)malloc(file_size + 1);
                if (json_str != nullptr) {
                    size_t bytes_read = fread(json_str, 1, file_size, module_file);
                    json_str[bytes_read] = '\0';
                    
                    // Parse module JSON
                    nlohmann::json module_config = nlohmann::json::parse(json_str, nullptr, false);
                    if (!module_config.is_discarded()) {
                        loaded_config[module.name] = module_config;
                        any_section_loaded = true;
                        ESP_LOGD(TAG, "Loaded module %s (%zu bytes)", module.name, bytes_read);
                    } else {
                        ESP_LOGW(TAG, "Failed to parse JSON for module %s", module.name);
                    }
                    
                    free(json_str);
                } else {
                    ESP_LOGW(TAG, "Failed to allocate memory for module %s", module.name);
                }
            } else {
                ESP_LOGW(TAG, "Module %s file size invalid: %ld bytes", module.name, file_size);
            }
            fclose(module_file);
        } else {
            ESP_LOGD(TAG, "Module %s config file not found, will use defaults", module.name);
        }
    }
    
    if (any_section_loaded) {
        // Check version and migrate if necessary
        if (saved_version < config_version) {
            ESP_LOGI(TAG, "Migrating config from v%lu to v%lu", saved_version, config_version);
            migrate_config(loaded_config, saved_version);
            dirty_flag = true; // Need to save after migration
        }
        
        // Check for missing sections and add them with yielding
        ESP_LOGI(TAG, "Checking for missing config sections...");
        for (size_t i = 0; i < CONFIG_MODULES_COUNT; ++i) {
            const ConfigModule& module = config_modules[i];
            if (!loaded_config.contains(module.name)) {
                ESP_LOGI(TAG, "Adding missing config section: %s", module.name);
                
                // STACK-SAFE: Use smart pointer for minimal stack usage
                std::unique_ptr<nlohmann::json> module_config_ptr = 
                    std::make_unique<nlohmann::json>(load_module_config(module));
                if (!module_config_ptr->empty()) {
                    loaded_config[module.name] = std::move(*module_config_ptr);
                    dirty_flag = true;
                }
                
                // Yield після кожного відсутнього модуля
                vTaskDelay(pdMS_TO_TICKS(2));
            }
        }
        
        get_config_cache() = loaded_config;
        
        if (!dirty_flag) {
            ESP_LOGI(TAG, "Configuration loaded from filesystem successfully");
        } else {
            ESP_LOGI(TAG, "Configuration loaded with missing sections added");
        }
    } else {
        ESP_LOGW(TAG, "No saved configuration found, loading embedded defaults");
        // Завантажуємо defaults тільки якщо конфігурація ще не ініціалізована
        if (!config_cache_ptr || config_cache_ptr->empty()) {
            ESP_LOGI(TAG, "Initializing config cache with embedded defaults");
            get_config_cache() = aggregate_embedded_configs();
        }
        dirty_flag = true; // Mark for saving
    }
    
    // Mark startup as successful
    startup_successful = true;
    last_error_code = ESP_OK;
    
    return ESP_OK;
}

esp_err_t save() {
    if (!dirty_flag) {
        ESP_LOGD(TAG, "Configuration not changed, skipping save");
        return ESP_OK;
    }
    
#ifdef CONFIG_USE_ASYNC_SAVE
    if (async_save_enabled) {
        // Використовувати асинхронне збереження
        ESP_LOGI(TAG, "Scheduling async save of all modules");
        esp_err_t result = ConfigManagerAsync::schedule_save_all();
        if (result == ESP_OK) {
            // Очищаємо dirty flag, оскільки збереження заплановано
            dirty_flag = false;
        } else {
            last_error_code = result;
        }
        return result;
    }
#endif
    
    // Fallback на синхронне збереження
    esp_err_t result = save_sync_internal();
    last_error_code = result;
    return result;
}

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
    }
#endif
    
    // Виконати синхронне збереження
    return save_sync_internal();
}

nlohmann::json get(const std::string& path) {
    // Перевіряємо чи ініціалізована конфігурація
    if (!config_cache_ptr || config_cache_ptr->empty()) {
        ESP_LOGW(TAG, "Config cache not initialized, loading minimal defaults");
        nlohmann::json& cache = get_config_cache();
        cache = nlohmann::json::object();
        cache["version"] = config_version;
    }
    
    if (path.empty()) {
        return get_config_cache();
    }
    
    std::vector<std::string> keys = parse_path(path);
    return get_value_at_path(get_config_cache(), keys);
}

esp_err_t set(const std::string& path, const nlohmann::json& value) {
    if (path.empty()) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Перевіряємо чи ініціалізована конфігурація
    if (!config_cache_ptr || config_cache_ptr->empty()) {
        ESP_LOGW(TAG, "Config cache not initialized, loading minimal defaults");
        nlohmann::json& cache = get_config_cache();
        cache = nlohmann::json::object();
        cache["version"] = config_version;
    }
    
    // Отримуємо старе значення для порівняння
    nlohmann::json old_value = get(path);
    
    // Встановлюємо нове значення в кеші
    std::vector<std::string> keys = parse_path(path);
    set_value_at_path(get_config_cache(), keys, value);
    
    // Позначаємо як "брудний" та логуємо
    dirty_flag = true;
    ESP_LOGD(TAG, "Set %s = %s", path.c_str(), value.dump().c_str());
    
    // Повідомляємо про зміну якщо значення відрізняється
    if (old_value != value) {
        notify_change(path, old_value, value);
        
#ifdef CONFIG_USE_ASYNC_SAVE
        if (auto_save_enabled && async_save_enabled) {
            // Автоматично планувати збереження змін
            std::string module_name = keys.empty() ? "" : keys[0];
            if (!module_name.empty()) {
                ConfigManagerAsync::schedule_save(module_name);
                ESP_LOGD(TAG, "Auto-scheduled save for module: %s", module_name.c_str());
            }
        }
#endif
    }
    
    return ESP_OK;
}

bool is_dirty() {
    return dirty_flag;
}

esp_err_t reset_to_defaults(const std::string& section) {
    if (section.empty()) {
        // Повне скидання до заводських налаштувань
        ESP_LOGI(TAG, "Resetting all configuration to factory defaults");
        get_config_cache() = aggregate_embedded_configs();
    } else {
        // Скидання конкретної секції
        ESP_LOGI(TAG, "Resetting section '%s' to factory defaults", section.c_str());
        
        // Знаходимо відповідний модуль
        bool found = false;
        for (size_t i = 0; i < CONFIG_MODULES_COUNT; ++i) {
            if (section == config_modules[i].name) {
                nlohmann::json module_defaults = load_module_config(config_modules[i]);
                if (!module_defaults.empty()) {
                    get_config_cache()[section] = module_defaults;
                    found = true;
                }
                break;
            }
        }
        
        if (!found) {
            ESP_LOGW(TAG, "Section '%s' not found in defaults", section.c_str());
            return ESP_ERR_NOT_FOUND;
        }
    }
    
    dirty_flag = true;
    
    // Автоматично зберігаємо після скидання
    return save();
}

nlohmann::json get_all() {
    return get_config_cache();
}

bool has(const std::string& path) {
    nlohmann::json value = get(path);
    return !value.is_null();
}

uint32_t get_version() {
    return get_config_cache().value("version", 1);
}

bool validate(const nlohmann::json& config) {
    nlohmann::json to_validate = config.empty() ? get_config_cache() : config;
    
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
    return get_config_cache().dump(pretty ? 4 : -1);
}

esp_err_t import_config(const std::string& json_str) {
    // Parse JSON with error checking
    nlohmann::json new_config = nlohmann::json::parse(json_str, nullptr, false);
    if (new_config.is_discarded()) {
        ESP_LOGE(TAG, "Failed to parse imported JSON: JSON parse error");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!validate(new_config)) {
        ESP_LOGE(TAG, "Imported configuration failed validation");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Зберігаємо старий кеш для відновлення у разі помилки
    nlohmann::json old_cache = get_config_cache();
    
    get_config_cache() = new_config;
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
    ESP_LOGI(TAG, "Reloading default configuration from embedded files");
    
    // Зберігаємо старий кеш для відновлення
    nlohmann::json old_cache = get_config_cache();
    
    // STACK-SAFE: Завантажуємо defaults по модулях з yield
    nlohmann::json& cache = get_config_cache();
    cache = nlohmann::json::object();
    cache["version"] = config_version;
    
    for (size_t i = 0; i < CONFIG_MODULES_COUNT; ++i) {
        const ConfigModule& module = config_modules[i];
        
        // STACK-SAFE: Use smart pointer to minimize stack usage
        std::unique_ptr<nlohmann::json> module_config_ptr = 
            std::make_unique<nlohmann::json>(load_module_config(module));
        if (!module_config_ptr->empty()) {
            cache[module.name] = std::move(*module_config_ptr);
        }
        
        // Yield після кожного модуля для безпеки стеку
        vTaskDelay(pdMS_TO_TICKS(3));
    }
    
    dirty_flag = true;
    
    ESP_LOGI(TAG, "Default configuration reloaded");
    
    return ESP_OK;
}

esp_err_t enable_async_save() {
#ifdef CONFIG_USE_ASYNC_SAVE
    if (!async_save_enabled) {
        ConfigManagerAsync::AsyncConfig config;
        config.write_queue_size = CONFIG_ASYNC_SAVE_QUEUE_SIZE;
        config.batch_delay_ms = CONFIG_ASYNC_SAVE_BATCH_DELAY_MS;
        config.watchdog_feed_interval = CONFIG_ASYNC_SAVE_WATCHDOG_FEED_MS;
#ifdef CONFIG_CONFIG_FILE_COMPRESSION
        config.enable_compression = CONFIG_CONFIG_FILE_COMPRESSION;
#else
        config.enable_compression = false;
#endif
        
        esp_err_t ret = ConfigManagerAsync::init_async(config);
        if (ret == ESP_OK) {
            async_save_enabled = true;
            ESP_LOGI(TAG, "Async save enabled for runtime operations");
        } else {
            ESP_LOGW(TAG, "Failed to enable async save: 0x%x", ret);
        }
        return ret;
    }
#endif
    return ESP_OK;
}

void enable_auto_save(bool enable) {
#ifdef CONFIG_USE_ASYNC_SAVE
    auto_save_enabled = enable && async_save_enabled;
    ESP_LOGI(TAG, "Auto-save %s", auto_save_enabled ? "enabled" : "disabled");
#else
    ESP_LOGW(TAG, "Auto-save not available - async save disabled");
#endif
}

SaveStatus get_save_status() {
    SaveStatus status = {
        .has_unsaved_changes = dirty_flag,
        .pending_async_saves = 0,
        .last_save_timestamp = 0, // TODO: track timestamp
        .config_size_bytes = get_config_cache().dump().length()
    };
    
#ifdef CONFIG_USE_ASYNC_SAVE
    if (async_save_enabled) {
        auto async_stats = ConfigManagerAsync::get_async_stats();
        status.pending_async_saves = async_stats.pending_saves;
    }
#endif
    
    return status;
}

ConfigHealth get_config_health() {
    ConfigHealth health = {};
    
    health.startup_successful = startup_successful;
    health.last_error_code = last_error_code;
    health.save_working = (last_error_code == ESP_OK);
    
    // Check if all modules are present
    health.all_modules_present = true;
    for (size_t i = 0; i < CONFIG_MODULES_COUNT; i++) {
        const ConfigModule& module = config_modules[i];
        if (!module.start || !module.end) {
            health.all_modules_present = false;
            break;
        }
    }
    
    // Schema validation
    health.validation_passed = validate();
    
    // Config size
    health.config_size_bytes = 0;
    if (config_cache_ptr && !config_cache_ptr->empty()) {
        std::string json_str = config_cache_ptr->dump();
        health.config_size_bytes = json_str.size();
    }
    
    return health;
}

ConfigDiagnostics get_diagnostics() {
    ConfigDiagnostics diagnostics = {};
    
    // Check module loading status
    diagnostics.all_modules_loaded = true;
    diagnostics.missing_modules.clear();
    
    // Check each embedded module
    for (size_t i = 0; i < CONFIG_MODULES_COUNT; i++) {
        const ConfigModule& module = config_modules[i];
        if (!module.start || !module.end) {
            diagnostics.missing_modules.push_back(module.name);
            diagnostics.all_modules_loaded = false;
        }
    }
    
    // Basic information
    diagnostics.config_version = config_version;
    diagnostics.schema_validation_passed = validate();
    
    // Count configuration keys
    diagnostics.total_keys_count = 0;
    if (config_cache_ptr && !config_cache_ptr->empty()) {
        // Simple key counting (just top-level keys)
        for (auto it = config_cache_ptr->begin(); it != config_cache_ptr->end(); ++it) {
            diagnostics.total_keys_count++;
        }
    }
    
    return diagnostics;
}

} // namespace ConfigManager