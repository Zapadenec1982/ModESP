# ConfigManager: Швидкий довідник

## 🚦 Коли що використовувати

### При старті системи (в `Application::init()`):
```cpp
ConfigManager::init();        // 1. Ініціалізація
ConfigManager::load();        // 2. БЛОКУЮЧЕ завантаження
// ... init modules ...        // 3. Модулі мають config
ConfigManager::enable_async_save(); // 4. Тільки після!
```

### Під час роботи:
```cpp
// Звичайні зміни - автоматично async
ConfigManager::set("sensor.offset", 1.5);

// Критичні зміни - примусово sync
ConfigManager::set("safety.max_temp", 85.0);
ConfigManager::force_save_sync();  // Блокує, але надійно
```

### При зупинці (в `Application::stop()`):
```cpp
ConfigManager::force_save_sync();  // Гарантоване збереження
ConfigManager::deinit();           // Очищення
```

## ❌ Антипатерни

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

## ✅ Правильні патерни

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

## 📊 Моніторинг

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
```

## 🔧 Налаштування (menuconfig)

- `[*] Enable asynchronous save` - Увімкнути async
- `[*] Auto-save changes` - Автоматичне збереження  
- `(200) Batch delay (ms)` - Затримка групування
- `(30) Watchdog feed interval (ms)` - Інтервал WDT

## 💡 Підказки

1. **Load при старті = ЗАВЖДИ синхронний**
2. **Save під час роботи = async (якщо не критично)**  
3. **Save при shutdown = ЗАВЖДИ синхронний**
4. **Модулі ніколи не викликають load() самі**
5. **Application керує життєвим циклом**
