# ConfigManager - Modular Configuration Management

## 🎯 Огляд

ConfigManager - централізований компонент для управління персистентними налаштуваннями ESP32 систем. Забезпечує надійне, швидке та watchdog-safe збереження конфігурації модулів.

### Ключові особливості:
- **Modular configuration** - окремі .json файли для кожного модуля
- **RAM caching** - швидкий доступ до налаштувань
- **Atomic saves** - надійне збереження в LittleFS
- **Lifecycle management** - правильний порядок завантаження/збереження
- **Async optimization** - watchdog-safe операції

---

## 🔄 Lifecycle архітектура

### Правильний життєвий цикл

```
┌─────────────────┐
│   BOOT PHASE    │ ← Все СИНХРОННЕ і БЛОКУЮЧЕ
├─────────────────┤
│ 1. Mount FS     │
│ 2. Config Init  │
│ 3. Config LOAD  │ ← БЛОКУЄ! Модулі чекають
│ 4. Module Init  │ ← Тепер модулі мають конфігурацію
└────────┬────────┘
         │
┌────────▼────────┐
│  RUNTIME PHASE  │ ← Асинхронні операції дозволені
├─────────────────┤
│ • Auto-save     │ ← Неблокуюче збереження
│ • Batch writes  │ ← Групування змін
│ • WDT friendly  │ ← Без timeout
└────────┬────────┘
         │
┌────────▼────────┐
│ SHUTDOWN PHASE  │ ← Знову все СИНХРОННЕ
├─────────────────┤
│ 1. Stop modules │
│ 2. Flush saves  │ ← БЛОКУЄ! Гарантоване збереження
│ 3. Unmount FS   │
└─────────────────┘
```

### Правильний порядок старту

```cpp
// application.cpp
esp_err_t Application::init() {
    // 1. Базові підсистеми
    EventBus::init();
    SharedState::init();
    
    // 2. СИНХРОННЕ завантаження конфігурації
    ConfigManager::init();
    ConfigManager::load();  // ← БЛОКУЄ! Це правильно!
    
    // 3. Тепер модулі можуть ініціалізуватися
    ModuleManager::configure_all(ConfigManager::get_all());
    ModuleManager::init_all();
    
    // 4. Тільки ПІСЛЯ старту включаємо async
    ConfigManager::enable_async_save();
    
    return ESP_OK;
}
```

### Коли що використовувати

| Операція | Тип | Коли використовувати |
|----------|-----|---------------------|
| `load()` | **SYNC** | При старті системи |
| `save()` | **ASYNC** | Під час роботи (runtime) |
| `force_save_sync()` | **SYNC** | При shutdown або критичні зміни |
| `set()` | **SYNC** | Завжди (тільки RAM) |
| Auto-save | **ASYNC** | Опціонально під час роботи |

---

## 🚀 Швидкий старт

### Ініціалізація (Boot Phase)
```cpp
// Правильна послідовність старту:
ConfigManager::init();
ConfigManager::load();              // БЛОКУЄ - це правильно!
ModuleManager::configure_all(...);  // Модулі мають config
ConfigManager::enable_async_save(); // Тільки після старту
```

### Runtime операції
```cpp
// Звичайні зміни - автоматично async
ConfigManager::set("sensor.offset", 1.5);

// Критичні зміни - примусово sync
ConfigManager::set("safety.max_temp", 85.0);
ConfigManager::force_save_sync();  // Блокує, але надійно
```

### Shutdown
```cpp
ConfigManager::force_save_sync();  // Гарантоване збереження
ConfigManager::deinit();           // Очищення
```

---

## ⚡ Асинхронна оптимізація

### Проблема яку вирішує

Під час тестування виявлено проблему з довгими операціями читання/запису, що призводить до спрацювання watchdog timer через:
- Синхронні операції з файлами
- Великі JSON об'єкти (кілька модулів по кілька КБ кожен)
- Відсутність годування watchdog під час операцій

### Рішення: Асинхронний ConfigManager

#### Ключові особливості:

1. **Асинхронний запис**
   - Окрема задача для операцій з файлами
   - Черга запитів на збереження
   - Групування змін (batching)

2. **Watchdog-friendly**
   - Регулярне годування watchdog під час операцій
   - Розбиття великих файлів на chunks
   - Контроль часу виконання

3. **Оптимізація продуктивності**
   - Групування змін для зменшення кількості записів
   - Кешування в RAM
   - Інкрементальне збереження

#### Використання асинхронних функцій:

```cpp
// Ініціалізація async
ConfigManagerAsync::AsyncConfig config;
config.write_queue_size = 20;
config.batch_delay_ms = 200;  // Чекати 200ms для групування
config.watchdog_feed_interval = 30;  // Годувати watchdog кожні 30ms

ConfigManagerAsync::init_async(config);

// Планування збереження окремого модуля
ConfigManager::set("sensors.temperature.offset", 1.5);
ConfigManagerAsync::schedule_save("sensors");  // Неблокуюче

// Планування збереження всієї конфігурації
ConfigManagerAsync::schedule_save_all();

// Примусове збереження перед виключенням
ConfigManagerAsync::flush_pending_saves(3000);  // Timeout 3 секунди

// Отримання статистики
auto stats = ConfigManagerAsync::get_async_stats();
ESP_LOGI(TAG, "Saves: %d completed, %d failed, avg time: %dms",
         stats.completed_saves, stats.failed_saves,
         stats.total_write_time_ms / stats.completed_saves);
```

---

## 🔧 API Reference

### Core функції

| Function | Type | Usage |
|----------|------|-------|
| `init()` | SYNC | System initialization |
| `load()` | SYNC | Boot phase only |
| `save()` | ASYNC | Runtime operations |
| `force_save_sync()` | SYNC | Shutdown or critical |
| `get(path)` | SYNC | Always (RAM cache) |
| `set(path, value)` | SYNC | Always (RAM only) |

### Async функції

| Function | Purpose |
|----------|---------|
| `enable_async_save()` | Enable after module init |
| `schedule_save()` | Queue specific module |
| `flush_pending_saves()` | Wait for completion |
| `get_async_stats()` | Performance monitoring |

### Структура конфігурації

```cpp
// Приклад доступу до конфігурації модуля
auto sensor_config = ConfigManager::get("sensors");
float offset = sensor_config.value("temperature.offset", 0.0);

// Оновлення конфігурації
ConfigManager::set("sensors.temperature.offset", 1.5);

// Валідація структури
if (ConfigManager::validate_schema("sensors", sensor_schema)) {
    // конфігурація валідна
}
```

---

## 📁 Структура файлів

### Production код
```
components/core/
├── config_manager.h           # Main API
├── config_manager.cpp         # Core implementation  
├── config_manager_async.h     # Async operations API
├── config_manager_async.cpp   # Watchdog-safe save
├── Kconfig.projbuild          # Configuration options
└── configs/                   # Default config files
    ├── system.json
    ├── climate.json
    └── sensors.json
```

### Структура у LittleFS
```
/storage/
├── configs/
│   ├── system.json
│   ├── sensors.json
│   ├── actuators.json
│   ├── climate.json
│   ├── network.json
│   ├── version.dat
│   └── backup/
│       ├── system.json.bak
│       └── ...
├── logs/
└── data/
```

---

## ⚙️ Конфігурація

### Menuconfig опції
```
Component config → Configuration Manager Options:
[*] Enable asynchronous configuration save
[*] Auto-save configuration changes  
(20) Async save queue size
(200) Batch delay in milliseconds
(30) Watchdog feed interval in milliseconds
```

### Build інтеграція
```cmake
# CMakeLists.txt
idf_component_register(
    SRCS "config_manager.cpp"
         "config_manager_async.cpp"
    INCLUDE_DIRS "."
    REQUIRES "esp_littlefs" "nlohmann_json"
)
```

---

## ✅ Best Practices & Patterns

### ✅ Правильні патерни

```cpp
// Модуль отримує config при старті:
void MyModule::configure(const json& config) {
    m_enabled = config.value("enabled", true);
    m_interval = config.value("interval_ms", 1000);
    // Config гарантовано завантажена!
}

// Runtime зміни:
void MyModule::update_settings(float offset) {
    ConfigManager::set("mymodule.offset", offset);
    // Автоматично заплановано async save
}

// Критичні налаштування:
void MyModule::set_safety_limit(float limit) {
    ConfigManager::set("mymodule.safety_limit", limit);
    ConfigManager::force_save_sync(); // Негайне збереження
}
```

### ❌ Антипатерни - що НЕ робити

```cpp
// НЕ РОБИТИ:
ConfigManager::load_async();      // Не існує! Load завжди sync
ConfigManager::enable_async_save(); 
ConfigManager::load();            // Занадто пізно!
ModuleManager::init_all();        // Fail - модулі без config

// НЕ РОБИТИ:
while (running) {
    ConfigManager::save_sync();   // Блокує main loop!
}
```

### Ключові правила:
1. **Load при старті = ЗАВЖДИ синхронний**
2. **Save під час роботи = async (якщо не критично)**  
3. **Save при shutdown = ЗАВЖДИ синхронний**
4. **Модулі ніколи не викликають load() самі**
5. **Application керує життєвим циклом**

---

## 📊 Моніторинг та діагностика

### Статистика продуктивності

```cpp
// Перевірка стану збереження
auto status = ConfigManager::get_save_status();
if (status.pending_async_saves > 20) {
    ESP_LOGW(TAG, "Багато незбережених змін!");
}

// В критичних місцях
if (status.has_unsaved_changes && critical_event) {
    ConfigManager::force_save_sync();
}

// Get performance statistics
auto stats = ConfigManagerAsync::get_async_stats();
ESP_LOGI(TAG, "Pending: %d, Completed: %d, Avg time: %d ms", 
         stats.pending_saves, stats.completed_saves, 
         stats.total_write_time_ms / stats.completed_saves);
```

### Приклад логів правильного старту

```
I (1000) App: === PHASE 1: System Initialization ===
I (1010) ConfigManager: Loading configuration (SYNC)...
I (1150) ConfigManager: Configuration loaded in 140 ms (45632 bytes)
I (1160) App: === PHASE 2: Module Configuration ===
I (1170) ModuleManager: Configuring 8 modules...
I (1180) App: === PHASE 3: Module Initialization ===
I (1190) SensorModule: Initialized with 4 sensors
I (1200) ActuatorModule: Initialized with 3 actuators
I (1210) App: === PHASE 4: Enable Runtime Features ===
I (1220) ConfigManager: Async save enabled for runtime
I (1230) App: System ready!
```

---

## 📈 Performance Monitoring & Diagnostics

### Enhanced Performance Metrics

ConfigManager тепер забезпечує детальний моніторинг продуктивності для production environments:

```cpp
// Отримання детальних метрик
ConfigManager::PerformanceMetrics metrics = ConfigManager::get_performance_metrics();

ESP_LOGI(TAG, "Performance Report:");
ESP_LOGI(TAG, "  Load operations: %lu (avg: %llu μs)", 
         metrics.load_operations, metrics.avg_load_time_us);
ESP_LOGI(TAG, "  Save operations: %lu (avg: %llu μs)", 
         metrics.save_operations, metrics.avg_save_time_us);
ESP_LOGI(TAG, "  Cache efficiency: %lu hits, %lu misses (%.1f%% hit rate)",
         metrics.cache_hits, metrics.cache_misses,
         100.0f * metrics.cache_hits / (metrics.cache_hits + metrics.cache_misses));
ESP_LOGI(TAG, "  Config size: %zu bytes", metrics.total_config_size_bytes);
ESP_LOGI(TAG, "  JSON operations: %lu parse, %lu serialize",
         metrics.json_parse_operations, metrics.json_serialize_operations);
ESP_LOGI(TAG, "  Peak memory usage: %zu bytes", metrics.peak_memory_usage_bytes);
```

### Real-time Operation Monitoring

```cpp
// Реєстрація callback для моніторингу всіх операцій
ConfigManager::register_operation_monitor([](const std::string& operation, 
                                           esp_err_t result, 
                                           uint64_t duration_us, 
                                           size_t data_size_bytes) {
    if (duration_us > 1000) {  // Slow operation warning
        ESP_LOGW(TAG, "SLOW %s: %llu μs (%zu bytes)", 
                operation.c_str(), duration_us, data_size_bytes);
    }
    
    // Send to monitoring system
    send_telemetry("config_operation", {
        {"op", operation},
        {"duration_us", duration_us},
        {"size_bytes", data_size_bytes},
        {"success", result == ESP_OK}
    });
});
```

### Comprehensive Diagnostics

```cpp
// Отримання детальної діагностики системи
ConfigManager::ConfigDiagnostics diag = ConfigManager::get_diagnostics();

ESP_LOGI(TAG, "System Health Check:");
ESP_LOGI(TAG, "  All modules loaded: %s", diag.all_modules_loaded ? "YES" : "NO");
ESP_LOGI(TAG, "  Missing modules: %zu", diag.missing_modules.size());
ESP_LOGI(TAG, "  Corrupted modules: %zu", diag.corrupted_modules.size());
ESP_LOGI(TAG, "  Config version: %lu", diag.config_version);
ESP_LOGI(TAG, "  Migration needed: %s", diag.migration_needed ? "YES" : "NO");
ESP_LOGI(TAG, "  Schema validation: %s", diag.schema_validation_passed ? "PASS" : "FAIL");
ESP_LOGI(TAG, "  Total configuration keys: %zu", diag.total_keys_count);
ESP_LOGI(TAG, "  Last successful save: %llu", diag.last_successful_save_time);
ESP_LOGI(TAG, "  Failed save attempts: %lu", diag.failed_save_attempts);

// Автоматичні дії на основі діагностики
if (!diag.all_modules_loaded) {
    ESP_LOGW(TAG, "Attempting to reload missing modules...");
    ConfigManager::reload_defaults();
}

if (diag.failed_save_attempts > 5) {
    ESP_LOGE(TAG, "Critical: Multiple save failures detected!");
    // Trigger emergency backup or system reset
}
```

### Performance Benchmarking

```cpp
// Скидання метрик для точного benchmarking
ConfigManager::reset_performance_metrics();

// Benchmark load performance
uint64_t start_time = esp_timer_get_time();
ConfigManager::load();
uint64_t load_time = esp_timer_get_time() - start_time;

// Benchmark cache performance
start_time = esp_timer_get_time();
for (int i = 0; i < 1000; i++) {
    ConfigManager::get("sensors.temperature.offset");
}
uint64_t cache_time = esp_timer_get_time() - start_time;

ESP_LOGI(TAG, "Benchmark Results:");
ESP_LOGI(TAG, "  Full load: %llu μs", load_time);
ESP_LOGI(TAG, "  1000 cache hits: %llu μs (avg: %llu μs per get)", 
         cache_time, cache_time / 1000);
```

---

## 🏭 Industrial Use Cases & Examples

### Multi-Zone Cold Storage System

```cpp
// Конфігурація багатозонної холодильної системи
void configure_cold_storage_zones() {
    // Zone A: Frozen storage (-18°C)
    ConfigManager::set("zones.A.type", "frozen");
    ConfigManager::set("zones.A.target_temp", -18.0);
    ConfigManager::set("zones.A.tolerance", 1.0);
    ConfigManager::set("zones.A.defrost_interval", 6);  // hours
    ConfigManager::set("zones.A.alarm_delay", 300);     // seconds
    
    // Zone B: Fresh storage (0-4°C) 
    ConfigManager::set("zones.B.type", "fresh");
    ConfigManager::set("zones.B.target_temp", 2.0);
    ConfigManager::set("zones.B.tolerance", 2.0);
    ConfigManager::set("zones.B.humidity_control", true);
    ConfigManager::set("zones.B.ventilation_rate", 50); // %
    
    // Zone C: Ripening chamber (12-16°C)
    ConfigManager::set("zones.C.type", "ripening");
    ConfigManager::set("zones.C.target_temp", 14.0);
    ConfigManager::set("zones.C.co2_level", 5.0);       // %
    ConfigManager::set("zones.C.ethylene_control", true);
    
    // Emergency settings
    ConfigManager::set("emergency.power_failure_temp", -15.0);
    ConfigManager::set("emergency.backup_cooling_enabled", true);
    ConfigManager::set("emergency.alarm_escalation_time", 1800); // 30 min
    
    // Save critical settings immediately
    ConfigManager::force_save_sync();
}

// Отримання налаштувань зони в runtime
void control_zone_temperature(const std::string& zone_id) {
    auto zone_config = ConfigManager::get("zones." + zone_id);
    
    if (zone_config.is_null()) {
        ESP_LOGE(TAG, "Zone %s not configured!", zone_id.c_str());
        return;
    }
    
    float target = zone_config["target_temp"].get<float>();
    float tolerance = zone_config["tolerance"].get<float>();
    bool defrost_enabled = zone_config.value("defrost_enabled", true);
    
    // Apply control logic
    apply_temperature_control(zone_id, target, tolerance);
    
    if (defrost_enabled) {
        schedule_defrost_cycle(zone_id, zone_config["defrost_interval"].get<int>());
    }
}
```

### Configuration Backup & Recovery for Production

```cpp
// Автоматичне резервне копіювання конфігурації
void setup_configuration_backup() {
    // Створення backup перед критичними змінами
    std::string backup_name = "pre_update_" + get_firmware_version();
    create_configuration_backup(backup_name);
    
    // Планування регулярного backup
    esp_timer_handle_t backup_timer;
    esp_timer_create_args_t timer_args = {
        .callback = [](void*) {
            create_configuration_backup("daily_" + get_timestamp_string());
            cleanup_old_backups(7); // Keep last 7 days
        },
        .name = "config_backup"
    };
    
    esp_timer_create(&timer_args, &backup_timer);
    esp_timer_start_periodic(backup_timer, 24 * 60 * 60 * 1000000); // 24 hours
}

esp_err_t create_configuration_backup(const std::string& backup_name) {
    ESP_LOGI(TAG, "Creating configuration backup: %s", backup_name.c_str());
    
    // Export current configuration
    std::string config_json = ConfigManager::export_config(true);
    
    // Create backup directory
    std::string backup_dir = "/storage/backups";
    mkdir(backup_dir.c_str(), 0755);
    
    // Write backup file with metadata
    nlohmann::json backup_data = {
        {"timestamp", esp_timer_get_time()},
        {"firmware_version", get_firmware_version()},
        {"device_id", get_device_id()},
        {"configuration", nlohmann::json::parse(config_json)}
    };
    
    std::string backup_path = backup_dir + "/" + backup_name + ".json";
    FILE* backup_file = fopen(backup_path.c_str(), "w");
    if (!backup_file) {
        ESP_LOGE(TAG, "Failed to create backup file: %s", backup_path.c_str());
        return ESP_FAIL;
    }
    
    std::string backup_json = backup_data.dump(2);
    fwrite(backup_json.c_str(), 1, backup_json.size(), backup_file);
    fclose(backup_file);
    
    ESP_LOGI(TAG, "Backup created successfully (%zu bytes): %s", 
             backup_json.size(), backup_path.c_str());
    return ESP_OK;
}

esp_err_t restore_configuration_backup(const std::string& backup_name) {
    ESP_LOGI(TAG, "Restoring configuration from backup: %s", backup_name.c_str());
    
    std::string backup_path = "/storage/backups/" + backup_name + ".json";
    FILE* backup_file = fopen(backup_path.c_str(), "r");
    if (!backup_file) {
        ESP_LOGE(TAG, "Backup file not found: %s", backup_path.c_str());
        return ESP_ERR_NOT_FOUND;
    }
    
    // Read backup file
    fseek(backup_file, 0, SEEK_END);
    size_t file_size = ftell(backup_file);
    fseek(backup_file, 0, SEEK_SET);
    
    char* backup_data = (char*)malloc(file_size + 1);
    fread(backup_data, 1, file_size, backup_file);
    backup_data[file_size] = '\0';
    fclose(backup_file);
    
    try {
        nlohmann::json backup_json = nlohmann::json::parse(backup_data);
        std::string config_str = backup_json["configuration"].dump();
        
        // Import configuration
        esp_err_t result = ConfigManager::import_config(config_str);
        if (result == ESP_OK) {
            ConfigManager::force_save_sync();
            ESP_LOGI(TAG, "Configuration restored successfully from: %s", backup_name.c_str());
        }
        
        free(backup_data);
        return result;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Failed to parse backup file: %s", e.what());
        free(backup_data);
        return ESP_FAIL;
    }
}
```

### Performance Optimization for Large Configurations

```cpp
// Оптимізація для великих промислових конфігурацій
void optimize_for_large_configurations() {
    // Налаштування async операцій для великих систем
    ConfigManagerAsync::AsyncConfig async_config;
    async_config.write_queue_size = 50;        // Більша черга
    async_config.batch_delay_ms = 500;         // Більше батчування
    async_config.watchdog_feed_interval = 25;  // Частіше годування WDT
    async_config.enable_compression = true;    // Стиснення для великих JSON
    
    ConfigManagerAsync::init_async(async_config);
    
    // Моніторинг продуктивності для великих систем
    ConfigManager::register_operation_monitor([](const std::string& operation, 
                                               esp_err_t result, 
                                               uint64_t duration_us, 
                                               size_t data_size_bytes) {
        // Performance thresholds for industrial systems
        if (operation == "load" && duration_us > 500000) {  // 500ms
            ESP_LOGW(TAG, "Slow configuration load: %llu ms", duration_us / 1000);
        }
        
        if (operation == "save" && duration_us > 1000000) {  // 1s
            ESP_LOGW(TAG, "Slow configuration save: %llu ms", duration_us / 1000);
        }
        
        if (data_size_bytes > 100000) {  // 100KB
            ESP_LOGI(TAG, "Large config operation: %s (%zu KB)", 
                    operation.c_str(), data_size_bytes / 1024);
        }
    });
    
    // Периодична оптимізація конфігурації
    schedule_config_optimization();
}

void schedule_config_optimization() {
    esp_timer_handle_t optimization_timer;
    esp_timer_create_args_t timer_args = {
        .callback = [](void*) {
            optimize_configuration_storage();
        },
        .name = "config_optimize"
    };
    
    esp_timer_create(&timer_args, &optimization_timer);
    // Оптимізація щогодини
    esp_timer_start_periodic(optimization_timer, 60 * 60 * 1000000);
}

void optimize_configuration_storage() {
    auto metrics = ConfigManager::get_performance_metrics();
    auto diagnostics = ConfigManager::get_diagnostics();
    
    ESP_LOGI(TAG, "Configuration optimization check:");
    ESP_LOGI(TAG, "  Current size: %zu bytes", metrics.total_config_size_bytes);
    ESP_LOGI(TAG, "  Total keys: %zu", diagnostics.total_keys_count);
    ESP_LOGI(TAG, "  Cache efficiency: %.1f%%", 
             100.0f * metrics.cache_hits / (metrics.cache_hits + metrics.cache_misses));
    
    // Cleanup unused keys if needed
    if (diagnostics.total_keys_count > 10000) {
        ESP_LOGW(TAG, "Large number of config keys detected, consider cleanup");
    }
    
    // Reset performance counters periodically
    if (metrics.load_operations > 1000) {
        ConfigManager::reset_performance_metrics();
        ESP_LOGI(TAG, "Performance metrics reset for fresh monitoring");
    }
}
```

---

## 🐛 Troubleshooting

### Watchdog Timeout
- Enable async save: `CONFIG_USE_ASYNC_SAVE=y`
- Reduce batch delay: `CONFIG_ASYNC_SAVE_BATCH_DELAY_MS=100`
- Increase watchdog feed frequency

### Module Start Failure
- Check config is loaded before module init
- Validate JSON syntax in config files
- Ensure all required fields present

### Performance Issues
- Enable batching for multiple changes
- Monitor async stats for queue size
- Consider compression for large configs

### Testing асинхронної продуктивності

```cpp
void test_async_config_performance() {
    // Створити велику конфігурацію
    for (int i = 0; i < 100; i++) {
        std::string path = "test.module" + std::to_string(i) + ".data";
        nlohmann::json large_data = generate_large_json(10000);  // 10KB
        ConfigManager::set(path, large_data);
    }
    
    // Запланувати збереження
    uint64_t start = esp_timer_get_time();
    ConfigManagerAsync::schedule_save_all();
    
    // Продовжити роботу
    while (ConfigManagerAsync::get_async_stats().pending_saves > 0) {
        // Система продовжує працювати
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    uint64_t duration = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "Async save completed in %lld ms", duration / 1000);
}
```

---

## 🔗 Пов'язані документи

- [SharedState](shared_state.md) - Система обміну даними в runtime
- [Application Lifecycle](application_lifecycle.txt) - Життєвий цикл додатку
- [Module Manager](module_manager.txt) - Управління модулями

---

*ConfigManager v2.0 - Professional configuration management for ESP32*
*Документ створено з: components/core/docs/README.md, LIFECYCLE.md, ASYNC_OPTIMIZATION.md, CONFIG_QUICK_REFERENCE.md*
