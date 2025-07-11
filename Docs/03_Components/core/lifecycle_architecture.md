# ConfigManager: Правильний життєвий цикл

## ✅ Виправлена архітектура

Ви абсолютно мали рацію - конфігурація МУСИТЬ бути завантажена синхронно при старті!

### 🔄 Життєвий цикл:

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

### 📋 Правильний порядок старту:

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

### ⚡ Коли що використовувати:

| Операція | Тип | Коли використовувати |
|----------|-----|---------------------|
| `load()` | **SYNC** | При старті системи |
| `save()` | **ASYNC** | Під час роботи (runtime) |
| `force_save_sync()` | **SYNC** | При shutdown або критичні зміни |
| `set()` | **SYNC** | Завжди (тільки RAM) |
| Auto-save | **ASYNC** | Опційно під час роботи |

### 🛡️ Переваги цього підходу:

1. **Детермінований старт**
   - Модулі гарантовано отримують конфігурацію
   - Немає race conditions
   - Передбачувана поведінка

2. **Responsive runtime**
   - Зміни конфігурації не блокують систему
   - Watchdog не спрацьовує
   - Плавна робота

3. **Надійний shutdown**
   - Гарантоване збереження всіх змін
   - Немає втрати даних
   - Clean exit

### 🔧 Налаштування:

```cpp
// sdkconfig або menuconfig:
CONFIG_USE_ASYNC_SAVE=y              // Включити async для runtime
CONFIG_AUTO_SAVE_ENABLED=y           // Автоматичне збереження змін
CONFIG_ASYNC_SAVE_BATCH_DELAY_MS=500 // Затримка для групування
```

### 📊 Приклад логів правильного старту:

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

### ❌ Що НЕ робити:

```cpp
// НЕПРАВИЛЬНО - модулі не матимуть конфігурації!
ConfigManager::init();
ConfigManager::enable_async_save(); 
ConfigManager::load_async();  // ← НІ!
ModuleManager::init_all();     // ← Fail - немає config!
```

## Висновок

Дякую за важливе зауваження! Асинхронність потрібна тільки для **збереження під час роботи**, а не для початкового завантаження. Це критично для правильної роботи системи.
