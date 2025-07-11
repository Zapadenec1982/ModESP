# ConfigManager - Modular Configuration Management

## 🎯 Overview

ConfigManager - централізований компонент для управління персистентними налаштуваннями ESP32 систем.

### Key Features:
- **Modular configuration** - окремі .json файли для кожного модуля
- **RAM caching** - швидкий доступ до налаштувань
- **Atomic saves** - надійне збереження в LittleFS
- **Lifecycle management** - правильний порядок завантаження/збереження
- **Async optimization** - watchdog-safe операції

## 🔄 Lifecycle Architecture

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

## 🚀 Quick Start

### Initialization (Boot Phase)
```cpp
// Правильна послідовність старту:
ConfigManager::init();
ConfigManager::load();              // БЛОКУЄ - це правильно!
ModuleManager::configure_all(...);  // Модулі мають config
ConfigManager::enable_async_save(); // Тільки після старту
```

### Runtime Operations
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

## ⚙️ Configuration

### Menuconfig Options
```
Component config → Configuration Manager Options:
[*] Enable asynchronous configuration save
[*] Auto-save configuration changes  
(20) Async save queue size
(200) Batch delay in milliseconds
(30) Watchdog feed interval in milliseconds
```

### Build Integration
```cmake
# CMakeLists.txt
idf_component_register(
    SRCS "config_manager.cpp"
         "config_manager_async.cpp"
    INCLUDE_DIRS "."
    REQUIRES "esp_littlefs" "nlohmann_json"
)
```

## 📁 File Structure

### Production Code
```
components/core/
├── config_manager.h           # Main API
├── config_manager.cpp         # Core implementation  
├── config_manager_async.h     # Async operations API
├── config_manager_async.cpp   # Watchdog-safe save
├── Kconfig.projbuild         # Configuration options
└── configs/                  # Default config files
    ├── system.json
    ├── climate.json
    └── sensors.json
```

### Documentation
```
components/core/docs/
├── README.md                 # This file
├── LIFECYCLE.md             # Detailed lifecycle guide
├── QUICK_REFERENCE.md       # API quick reference
└── ASYNC_OPTIMIZATION.md   # Performance optimization
```

## 🔧 API Reference

### Core Functions
| Function | Type | Usage |
|----------|------|-------|
| `init()` | SYNC | System initialization |
| `load()` | SYNC | Boot phase only |
| `save()` | ASYNC | Runtime operations |
| `force_save_sync()` | SYNC | Shutdown or critical |
| `get(path)` | SYNC | Always (RAM cache) |
| `set(path, value)` | SYNC | Always (RAM only) |

### Async Functions
| Function | Purpose |
|----------|---------|
| `enable_async_save()` | Enable after module init |
| `schedule_save()` | Queue specific module |
| `flush_pending_saves()` | Wait for completion |
| `get_async_stats()` | Performance monitoring |

## ⚠️ Best Practices

### ✅ DO
- Load config synchronously at boot
- Use async save during runtime
- Feed watchdog during long operations
- Validate config before save
- Handle migration gracefully

### ❌ DON'T
- Load config asynchronously at boot
- Block main loop with sync saves
- Save on every change (use batching)
- Skip watchdog feeding
- Change config structure without migration

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

## 📊 Monitoring

```cpp
// Get performance statistics
auto stats = ConfigManagerAsync::get_async_stats();
ESP_LOGI(TAG, "Pending: %d, Completed: %d, Avg time: %d ms", 
         stats.pending_saves, stats.completed_saves, 
         stats.total_write_time_ms / stats.completed_saves);
```

## 🎯 Integration Examples

See `_archive_config/application_config_usage.cpp` for complete Application integration example.

---

*ConfigManager v2.0 - Professional configuration management for ESP32* 